// Anfragebehandlung der HTTP-JSON-Schnittstelle (E8)
#include "anfragebehandlung.hpp"
// Offline-Kursquelle: liest gespeicherte Antworten aus einem Ordner
#include "dateiquelle.hpp"
// Offline-Zinsquelle und Laden aller Zentralbanken (E6)
#include "datei_zinsquelle.hpp"
#include "zinsabruf.hpp"
// Netzwerkschicht des Servers
#include "http_server.hpp"
// Katalog der Wertpapiere (E2)
#include "katalog.hpp"
// Fehlerklassen (für Fehler beim Lesen des Katalogs)
#include "quellenfehler.hpp"
// E9: Ergebnis-Cache, gecachte Anfragebehandlung, thread-sichere Quelle, echte Uhr
#include "ergebnis_cache.hpp"
#include "gecachte_anfragebehandlung.hpp"
#include "synchronisierte_quelle.hpp"
#include "uhr.hpp"

// pthread_sigmask, sigwait, pthread_kill: Signale (Strg+C) in einem eigenen Thread empfangen (POSIX)
#include <csignal>
#include <pthread.h>

// std::from_chars für die Portnummer
#include <charconv>
// std::chrono::minutes für die Gültigkeit des Ergebnis-Caches
#include <chrono>
// std::cout und std::cerr
#include <iostream>
// std::string
#include <string>
// std::thread
#include <thread>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Alle Einstellungen, die über die Kommandozeile geändert werden können
struct Einstellungen {
    // Adresse, auf der der Server lauscht. 127.0.0.1 = nur dieser Rechner (bzw. dieser Container).
    // Im Container mit veröffentlichtem Port: --host 0.0.0.0
    std::string host = "127.0.0.1";
    // Port
    int port = 8080;
    // Katalog der Wertpapiere
    std::string katalog = "config/katalog.json";
    // Ordner mit gespeicherten Alpha-Vantage-Antworten (<SYMBOL>.json)
    std::string daten = "daten";
    // Ordner mit gespeicherten FRED-Antworten (<SERIE>.json) für die Zinsentscheidungen (E6)
    std::string zinsen = "daten/zinsen";
    // Ordner der Weboberfläche (E10)
    std::string web = "web";
    // Anzahl der Arbeits-Threads (E9); 0 = so viele, wie der Rechner Kerne meldet
    int threads = 0;
};

// Gibt die Hilfe aus
void hilfe() {
    // Aufruf und alle Optionen mit Standardwerten
    std::cout << "Aufruf: projekt [Optionen]\n"
                 "  --host ADRESSE   Adresse (Standard 127.0.0.1; im Container 0.0.0.0)\n"
                 "  --port PORT      Port (Standard 8080)\n"
                 "  --katalog DATEI  Katalog der Wertpapiere (Standard config/katalog.json)\n"
                 "  --daten ORDNER   gespeicherte Kursdaten <SYMBOL>.json (Standard daten)\n"
                 "  --zinsen ORDNER  gespeicherte FRED-Zinsdaten <SERIE>.json (Standard daten/zinsen)\n"
                 "  --web ORDNER     Dateien der Weboberfläche (Standard web)\n"
                 "  --threads N      Arbeits-Threads (Standard: Anzahl der Prozessorkerne)\n"
                 "  --hilfe          diese Hilfe\n";
}

// Liest die Kommandozeile; liefert false bei einem Fehler oder bei --hilfe
bool leseArgumente(int argc, char* argv[], Einstellungen& e, int& rueckgabe) {
    // Alle Argumente nach dem Programmnamen
    const std::vector<std::string> args(argv + 1, argv + argc);
    // Der Reihe nach durchgehen
    for (std::size_t i = 0; i < args.size(); ++i) {
        // Aktuelles Argument
        const std::string& a = args[i];
        // Hilfe
        if (a == "--hilfe" || a == "-h") {
            // Hilfe zeigen und mit Erfolg beenden
            hilfe();
            rueckgabe = 0;
            return false;
        }
        // Alle anderen Optionen brauchen einen Wert danach
        if (i + 1 >= args.size()) {
            // Wert fehlt
            std::cerr << "Fehler: unbekannte Option oder Wert fehlt: " << a << "\n";
            hilfe();
            rueckgabe = 2;
            return false;
        }
        // Der Wert hinter der Option
        const std::string& wert = args[++i];
        // Option zuordnen
        if (a == "--host") {
            // Adresse
            e.host = wert;
        } else if (a == "--port") {
            // Port als Zahl lesen
            const auto r = std::from_chars(wert.data(), wert.data() + wert.size(), e.port);
            // Nur ganze Zahlen von 1 bis 65535
            if (r.ec != std::errc() || r.ptr != wert.data() + wert.size() || e.port < 1 || e.port > 65535) {
                // Ungültiger Port
                std::cerr << "Fehler: ungültiger Port: " << wert << "\n";
                rueckgabe = 2;
                return false;
            }
        } else if (a == "--threads") {
            // Anzahl Threads als Zahl lesen
            const auto r = std::from_chars(wert.data(), wert.data() + wert.size(), e.threads);
            // Nur ganze Zahlen von 1 bis 256
            if (r.ec != std::errc() || r.ptr != wert.data() + wert.size() || e.threads < 1 || e.threads > 256) {
                // Ungültige Anzahl
                std::cerr << "Fehler: ungültige Anzahl Threads: " << wert << "\n";
                rueckgabe = 2;
                return false;
            }
        } else if (a == "--katalog") {
            // Katalogdatei
            e.katalog = wert;
        } else if (a == "--zinsen") {
            // Ordner der Zinsdaten
            e.zinsen = wert;
        } else if (a == "--daten") {
            // Datenordner
            e.daten = wert;
        } else if (a == "--web") {
            // Ordner der Weboberfläche
            e.web = wert;
        } else {
            // Unbekannte Option
            std::cerr << "Fehler: unbekannte Option: " << a << "\n";
            hilfe();
            rueckgabe = 2;
            return false;
        }
    }
    // Alles gelesen
    return true;
}

}  // namespace

// Startet den MarketLens-Server
//   ./build/projekt
//   ./build/projekt --host 0.0.0.0 --port 8080 --threads 8
int main(int argc, char* argv[]) {
    // Einstellungen mit Standardwerten
    Einstellungen e;
    // Rückgabewert, falls die Kommandozeile das Programm beendet
    int rueckgabe = 0;
    // Kommandozeile lesen
    if (!leseArgumente(argc, argv, e, rueckgabe)) {
        // Hilfe gezeigt oder Fehler
        return rueckgabe;
    }

    // Katalog lesen; ein unbrauchbarer Katalog beendet das Programm
    try {
        // Datei lesen
        const Katalog katalog = Katalog::ausDatei(e.katalog);
        // Fehlerhafte Einträge melden; die übrigen werden trotzdem verwendet (DoD E2)
        for (const KatalogFehler& f : katalog.fehler()) {
            // Position, Symbol und Grund
            std::cerr << "Warnung: Katalog-Eintrag " << f.position << " (" << f.symbol << ") übersprungen: "
                      << f.meldung << "\n";
        }
        // Ebenso fehlerhafte Zentralbanken
        for (const KatalogFehler& f : katalog.zentralbankFehler()) {
            // Position, Name und Grund
            std::cerr << "Warnung: Zentralbank " << f.position << " (" << f.symbol << ") übersprungen: " << f.meldung
                      << "\n";
        }

        // Zinsentscheidungen aller Zentralbanken einmal beim Start laden (E6).
        // Offline aus gespeicherten FRED-Antworten; fehlt eine Datei, fehlen nur die Ereignisse dieser Bank.
        DateiZinsquelle zinsquelle(e.zinsen);
        const ZinsLadeergebnis zinsen = ladeAlleEntscheidungen(zinsquelle, katalog.zentralbanken());
        // Fehler einzelner Zentralbanken melden
        for (const ZinsLadefehler& f : zinsen.fehler) {
            // Name und Grund
            std::cerr << "Warnung: Zinsdaten von " << f.zentralbank << " nicht geladen: " << f.meldung << "\n";
        }

        // Kette der Anfragebearbeitung (E9):
        //   Server -> Ergebnis-Cache -> Anfragebehandlung -> SynchronisierteQuelle -> DateiQuelle
        // Offline-Kursquelle (der Live-Zugriff über HTTPS kommt später mit einer CMake-Option)
        DateiQuelle dateiQuelle(e.daten);
        // Nur ein Thread gleichzeitig lädt Daten; Berechnungen laufen parallel
        SynchronisierteQuelle quelle(dateiQuelle);
        // Logik der Schnittstelle mit Katalog, Kursquelle und Zinsentscheidungen
        Anfragebehandlung api(katalog.wertpapiere(), quelle, zinsen.entscheidungen);
        // Ergebnisse 5 Minuten aufbewahren, höchstens 1000 verschiedene Anfragen
        SystemUhr uhr;
        ErgebnisCache cache(uhr, std::chrono::minutes{5}, 1000);
        GecachteAnfragebehandlung gecacht(api, cache);

        // Anzahl Threads: Vorgabe oder Anzahl der Kerne (hardware_concurrency kann 0 liefern, dann 4)
        const unsigned kerne = std::thread::hardware_concurrency();
        const std::size_t threads = e.threads > 0 ? static_cast<std::size_t>(e.threads) : (kerne > 0 ? kerne : 4);

        // Signale SIGINT (Strg+C) und SIGTERM für ALLE Threads sperren, BEVOR Threads gestartet werden.
        // Neue Threads erben die Sperre. Dann kommen die Signale nur noch beim Signal-Thread unten an,
        // der sie mit sigwait() gezielt abholt. So läuft das Beenden als normaler Code in einem normalen
        // Thread, nicht in einem Signal-Handler (dort dürften fast keine Funktionen aufgerufen werden).
        sigset_t signale;
        sigemptyset(&signale);
        sigaddset(&signale, SIGINT);
        sigaddset(&signale, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &signale, nullptr);

        // Netzwerkschicht mit Thread-Pool
        HttpServer server(gecacht, e.web, threads);
        // Port öffnen
        if (server.binde(e.host, e.port) < 0) {
            // Port belegt oder Adresse ungültig
            std::cerr << "Fehler: Server konnte nicht auf " << e.host << ":" << e.port << " starten\n";
            return 1;
        }

        // Signal-Thread: wartet auf Strg+C oder SIGTERM und stoppt dann den Server
        std::thread signalThread([&server, &signale] {
            // Nummer des empfangenen Signals
            int signal = 0;
            // Blockiert, bis eines der Signale kommt
            sigwait(&signale, &signal);
            // Sicherstellen, dass lausche() wirklich läuft (oder nicht mehr starten wird), dann stoppen
            server.warteBisBereit();
            server.stoppe();
        });

        // Hinweis, wo der Server zu erreichen ist
        std::cout << "MarketLens: " << katalog.wertpapiere().size() << " Wertpapiere, Kursdaten aus '" << e.daten
                  << "/', " << katalog.zentralbanken().size() << " Zentralbanken (" << zinsen.entscheidungen.size()
                  << " Zinseinträge), " << threads << " Threads\n"
                  << "Server läuft auf http://" << e.host << ":" << e.port << "  (beenden mit Strg+C)\n"
                  << std::flush;
        // Anfragen bearbeiten; kehrt nach stoppe() zurück, wenn alle Anfragen fertig und alle Threads beendet sind
        server.lausche();

        // Falls lausche() ohne Signal endete: Server als gestoppt markieren und den Signal-Thread wecken,
        // damit auch er endet (er wartet sonst ewig auf ein Signal)
        server.stoppe();
        pthread_kill(signalThread.native_handle(), SIGTERM);
        // Auf den Signal-Thread warten: danach läuft kein Thread des Programms mehr außer main
        signalThread.join();
        // Meldung
        std::cout << "MarketLens beendet.\n";
    } catch (const QuellenFehler& f) {
        // Katalog fehlt oder ist kein gültiges JSON
        std::cerr << "Fehler: " << f.what() << "\n";
        return 1;
    }
    // Normales Ende
    return 0;
}
