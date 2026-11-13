// Anfragebehandlung der HTTP-JSON-Schnittstelle (E8)
#include "anfragebehandlung.hpp"
// Offline-Kursquelle: liest gespeicherte Antworten aus einem Ordner
#include "dateiquelle.hpp"
// Netzwerkschicht des Servers
#include "http_server.hpp"
// Katalog der Wertpapiere (E2)
#include "katalog.hpp"
// Fehlerklassen (für Fehler beim Lesen des Katalogs)
#include "quellenfehler.hpp"

// std::from_chars für die Portnummer
#include <charconv>
// std::cout und std::cerr
#include <iostream>
// std::string
#include <string>
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
    // Ordner der Weboberfläche (E10)
    std::string web = "web";
};

// Gibt die Hilfe aus
void hilfe() {
    // Aufruf und alle Optionen mit Standardwerten
    std::cout << "Aufruf: projekt [Optionen]\n"
                 "  --host ADRESSE   Adresse (Standard 127.0.0.1; im Container 0.0.0.0)\n"
                 "  --port PORT      Port (Standard 8080)\n"
                 "  --katalog DATEI  Katalog der Wertpapiere (Standard config/katalog.json)\n"
                 "  --daten ORDNER   gespeicherte Kursdaten <SYMBOL>.json (Standard daten)\n"
                 "  --web ORDNER     Dateien der Weboberfläche (Standard web)\n"
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
        } else if (a == "--katalog") {
            // Katalogdatei
            e.katalog = wert;
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
//   ./build/projekt --host 0.0.0.0 --port 8080
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

        // Offline-Kursquelle (der Live-Zugriff über HTTPS kommt später mit einer CMake-Option)
        DateiQuelle quelle(e.daten);
        // Logik der Schnittstelle; Zinsentscheidungen gibt es erst mit der Zinsquelle (E6), bis dahin keine
        Anfragebehandlung api(katalog.wertpapiere(), quelle, {});
        // Netzwerkschicht
        HttpServer server(api, e.web);

        // Hinweis, wo der Server zu erreichen ist
        std::cout << "MarketLens: " << katalog.wertpapiere().size() << " Wertpapiere, Kursdaten aus '" << e.daten
                  << "/'\n"
                  << "Server läuft auf http://" << e.host << ":" << e.port << "  (beenden mit Strg+C)\n"
                  << std::flush;
        // Starten; kehrt erst beim Beenden zurück
        if (!server.starte(e.host, e.port)) {
            // Port belegt oder Adresse ungültig
            std::cerr << "Fehler: Server konnte nicht auf " << e.host << ":" << e.port << " starten\n";
            return 1;
        }
    } catch (const QuellenFehler& f) {
        // Katalog fehlt oder ist kein gültiges JSON
        std::cerr << "Fehler: " << f.what() << "\n";
        return 1;
    }
    // Normales Ende
    return 0;
}
