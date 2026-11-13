// Die Klasse, die hier getestet wird
#include "http_server.hpp"
// Für den Vergleich mit der echten Anfragebehandlung
#include "anfragebehandlung.hpp"
#include "ergebnis_cache.hpp"
#include "gecachte_anfragebehandlung.hpp"
#include "synchronisierte_quelle.hpp"
// Fake-Uhr für den Ergebnis-Cache
#include "fake_uhr.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>
// HTTP-Client der gleichen Bibliothek, um den Server über echte Sockets anzusprechen
#include <httplib.h>

// std::atomic
#include <atomic>
// std::chrono
#include <chrono>
// std::sin
#include <cmath>
// std::condition_variable
#include <condition_variable>
// std::filesystem für den Ordner mit index.html
#include <filesystem>
// std::ofstream
#include <fstream>
// std::async und std::future
#include <future>
// std::mutex
#include <mutex>
// std::string
#include <string>
// std::thread
#include <thread>
// getpid für einen eindeutigen Ordnernamen
#include <unistd.h>
// std::vector
#include <vector>

// Kurze Schreibweise für Zeitangaben
using namespace std::chrono_literals;

// Anonymer Namespace: Hilfsklassen nur in dieser Datei sichtbar
namespace {

// Startet einen Server auf einem freien Port von 127.0.0.1 in einem eigenen Thread und stoppt ihn am Ende.
// Hält fest, ob der Server-Thread nach stoppe() innerhalb von 10 s endet (kein Hängen).
class LaufenderServer {
public:
    // Server anlegen, Port wählen, lausche() im Hintergrund starten, auf Bereitschaft warten
    LaufenderServer(IAnfrageBehandler& behandler, std::size_t threads, std::filesystem::path web = "")
        : server_(behandler, web, threads) {
        // Freien Port wählen lassen
        port = server_.binde("127.0.0.1", 0);
        // lausche() blockiert, deshalb in einem eigenen Thread; das future meldet das Ende
        ende_ = std::async(std::launch::async, [this] { return server_.lausche(); });
        // Warten, bis Anfragen angenommen werden
        server_.warteBisBereit();
    }

    // Stoppt den Server; liefert true, wenn lausche() innerhalb von 10 s zurückgekehrt ist
    bool stoppeUndWarte() {
        // Stoppen
        server_.stoppe();
        // Auf das Ende von lausche() warten, aber nicht unbegrenzt
        return ende_.wait_for(10s) == std::future_status::ready;
    }

    // Am Ende sicherheitshalber stoppen, falls der Test es nicht getan hat
    ~LaufenderServer() {
        // Stoppen (mehrfach erlaubt)
        server_.stoppe();
        // Warten, falls noch nicht geschehen
        if (ende_.valid()) {
            // Ende abwarten
            ende_.wait();
        }
    }

    // Neuer Client für diesen Server
    httplib::Client client() const {
        // Client auf 127.0.0.1 und den gewählten Port
        httplib::Client c("127.0.0.1", port);
        // Nicht ewig warten, falls etwas schiefgeht
        c.set_read_timeout(10, 0);
        // Client zurückgeben
        return c;
    }

    // Der gewählte Port
    int port = -1;

private:
    // Der Server
    HttpServer server_;
    // Ende von lausche()
    std::future<bool> ende_;
};

// Behandler, der nur dann 200 liefert, wenn "ziel" Anfragen GLEICHZEITIG bei ihm sind (sonst nach 5 s 500).
// Beweist, dass der Server Anfragen wirklich parallel bearbeitet.
class GleichzeitigBehandler : public IAnfrageBehandler {
public:
    // Ziel merken
    explicit GleichzeitigBehandler(int ziel) : ziel_(ziel) {}
    // Wartet auf die anderen
    Antwort behandle(const Anfrage&) override {
        // Sperren
        std::unique_lock<std::mutex> sperre(m_);
        // Eine mehr ist da
        ++drin_;
        // Alle wecken
        cv_.notify_all();
        // Warten, bis genug gleichzeitig da sind
        const bool erreicht = cv_.wait_for(sperre, 5s, [this] { return drin_ >= ziel_; });
        // Antwort mit passendem Status
        Antwort a;
        a.status = erreicht ? 200 : 500;
        a.body = "{}";
        return a;
    }

private:
    // Wie viele gleichzeitig nötig sind
    int ziel_;
    // Wie viele schon da sind
    int drin_ = 0;
    // Schutz und Signal
    std::mutex m_;
    std::condition_variable cv_;
};

// Langsamer Behandler: meldet den Beginn und braucht 300 ms
class LangsamerBehandler : public IAnfrageBehandler {
public:
    // Bearbeitung
    Antwort behandle(const Anfrage&) override {
        // Beginn melden
        begonnen = true;
        // Langsam arbeiten
        std::this_thread::sleep_for(300ms);
        // Antwort
        Antwort a;
        a.body = R"({"fertig":true})";
        return a;
    }
    // Ob eine Bearbeitung begonnen hat
    std::atomic<bool> begonnen{false};
};

// Testreihe (60 Tage, Welle) und Testquelle wie in den anderen Tests
class TestQuelle : public IPriceSource {
public:
    // Kurse laden
    Kursreihe ladeKurse(const Wertpapier& wp) override {
        // Leere Reihe
        Kursreihe reihe;
        // Startdatum
        const std::chrono::sys_days start = std::chrono::year{2026} / 1 / 1;
        // Versatz je Symbol
        const double v = static_cast<double>(wp.symbol.size());
        // 60 Tage
        for (int i = 0; i < 60; ++i) {
            // Schlusskurs
            const double c = 100.0 + v + 10.0 * std::sin(i / 5.0 + v) + 0.2 * i;
            // Kerze
            reihe.hinzufuegen(Kerze(start + std::chrono::days{i}, c, c + 1.0, c - 1.0, c, 1000.0));
        }
        // Fertige Reihe
        return reihe;
    }
};

}  // namespace

// Über echte Sockets mit vielen gleichzeitigen Clients: jede Antwort gleich der direkten Berechnung
TEST(HttpServer, ParalleleClientsBekommenGleicheErgebnisseWieDirekt) {
    // Katalog
    const std::vector<Wertpapier> katalog = {Wertpapier("IBM", "IBM", "USA", "USD"),
                                             Wertpapier("AAPL", "Apple", "USA", "USD")};
    // Referenz: direkt, ohne Server
    TestQuelle referenzQuelle;
    Anfragebehandlung referenz(katalog, referenzQuelle, {});
    // Server-Kette wie in main.cpp
    TestQuelle quelle;
    SynchronisierteQuelle sicher(quelle);
    Anfragebehandlung api(katalog, sicher, {});
    FakeUhr uhr(std::chrono::sys_days{std::chrono::year{2026} / 11 / 13});
    ErgebnisCache cache(uhr, 5min, 1000);
    GecachteAnfragebehandlung gecacht(api, cache);
    // Server mit 4 Threads
    LaufenderServer server(gecacht, 4);
    ASSERT_GT(server.port, 0);

    // Anfragen, die jeder Client stellt (Pfad und Parameter)
    const std::vector<Anfrage> anfragen = {
        {"GET", "/api/wertpapiere", {}},
        {"GET", "/api/kurse", {{"symbol", "IBM"}}},
        {"GET", "/api/indikatoren", {{"symbol", "AAPL"}, {"sma", "10"}}},
        {"GET", "/api/indikatoren", {{"symbol", "IBM"}, {"macd", "3,6,2"}}},
        {"GET", "/api/ereignisstudie", {{"symbol", "IBM"}}},
        {"GET", "/api/kurse", {{"symbol", "MSFT"}}},          // 404
        {"GET", "/api/indikatoren", {{"symbol", "IBM"}, {"sma", "0"}}},  // 400
        {"GET", "/api/gibtsnicht", {}},                       // 404
    };
    // Erwartete Antworten
    std::vector<Antwort> erwartet;
    for (const Anfrage& a : anfragen) {
        // Direkt berechnen
        erwartet.push_back(referenz.behandle(a));
    }

    // 8 Clients gleichzeitig, jeder stellt alle Anfragen 5-mal
    std::atomic<int> fehler{0};
    std::vector<std::thread> clients;
    for (int t = 0; t < 8; ++t) {
        // Client-Thread
        clients.emplace_back([&] {
            // Eigener Client je Thread
            httplib::Client c = server.client();
            // 5 Runden
            for (int runde = 0; runde < 5; ++runde) {
                // Alle Anfragen
                for (std::size_t i = 0; i < anfragen.size(); ++i) {
                    // Parameter für die Bibliothek (sie kodiert sie für die URL)
                    httplib::Params p(anfragen[i].parameter.begin(), anfragen[i].parameter.end());
                    // Anfrage senden
                    const auto r = c.Get(anfragen[i].pfad, p, httplib::Headers{});
                    // Antwort muss da sein und genau der Referenz entsprechen
                    if (!r || r->status != erwartet[i].status || r->body != erwartet[i].body) {
                        // Abweichung zählen
                        ++fehler;
                    }
                }
            }
        });
    }
    // Auf alle Clients warten
    for (auto& t : clients) {
        // Warten
        t.join();
    }
    // Keine Abweichung bei 8 x 5 x 8 = 320 Anfragen
    EXPECT_EQ(fehler.load(), 0);
    // Jede verschiedene Anfrage nur einmal berechnet (8 verschiedene)
    EXPECT_EQ(cache.anzahlBerechnungen(), anfragen.size());
    // Server endet sauber
    EXPECT_TRUE(server.stoppeUndWarte());
}

// Der Server bearbeitet wirklich parallel: 4 Anfragen sind gleichzeitig im Behandler
TEST(HttpServer, BearbeitetAnfragenParallel) {
    // Behandler, der 4 gleichzeitige Anfragen verlangt
    GleichzeitigBehandler behandler(4);
    // Server mit 4 Threads
    LaufenderServer server(behandler, 4);
    // 4 Clients gleichzeitig
    std::vector<std::future<int>> status;
    for (int i = 0; i < 4; ++i) {
        // Jeder Client in eigenem Thread
        status.push_back(std::async(std::launch::async, [&server] {
            // Anfrage
            const auto r = server.client().Get("/api/x");
            // Status oder -1 bei Verbindungsfehler
            return r ? r->status : -1;
        }));
    }
    // Alle 200: alle vier waren gleichzeitig beim Behandler
    for (auto& s : status) {
        // Prüfen
        EXPECT_EQ(s.get(), 200);
    }
    // Sauber beenden
    EXPECT_TRUE(server.stoppeUndWarte());
}

// Stoppen während einer laufenden Anfrage: die Anfrage wird noch fertig beantwortet, dann enden alle Threads
TEST(HttpServer, StopptSauberMitLaufenderAnfrage) {
    // Langsamer Behandler
    LangsamerBehandler behandler;
    // Server mit 2 Threads
    LaufenderServer server(behandler, 2);
    // Anfrage im Hintergrund
    auto antwort = std::async(std::launch::async, [&server] {
        // Anfrage senden
        const auto r = server.client().Get("/api/x");
        // Inhalt oder leer
        return r ? r->body : std::string();
    });
    // Warten, bis die Bearbeitung begonnen hat
    while (!behandler.begonnen) {
        // Kurz warten
        std::this_thread::sleep_for(1ms);
    }
    // Jetzt stoppen, während die Anfrage noch läuft: muss innerhalb von 10 s enden
    EXPECT_TRUE(server.stoppeUndWarte());
    // Die laufende Anfrage wurde noch vollständig beantwortet
    EXPECT_EQ(antwort.get(), R"({"fertig":true})");
    // Danach nimmt der Server keine Verbindungen mehr an
    EXPECT_FALSE(server.client().Get("/api/x"));
}

// stoppe() vor lausche(): lausche() startet gar nicht erst (wichtig, wenn Strg+C sehr früh kommt)
TEST(HttpServer, StoppeVorLauschen) {
    // Beliebiger Behandler
    LangsamerBehandler behandler;
    // Server direkt (ohne Hilfsklasse)
    HttpServer server(behandler, "", 1);
    // Port öffnen
    ASSERT_GT(server.binde("127.0.0.1", 0), 0);
    // Vor dem Lauschen stoppen
    server.stoppe();
    // lausche() kehrt sofort mit false zurück
    auto ende = std::async(std::launch::async, [&server] { return server.lausche(); });
    ASSERT_EQ(ende.wait_for(5s), std::future_status::ready);
    EXPECT_FALSE(ende.get());
}

// Ein belegter Port wird erkannt
TEST(HttpServer, BelegterPort) {
    // Beliebiger Behandler
    LangsamerBehandler behandler;
    // Erster Server belegt einen Port
    LaufenderServer erster(behandler, 1);
    // Zweiter Server auf demselben Port: -1
    HttpServer zweiter(behandler, "", 1);
    EXPECT_EQ(zweiter.binde("127.0.0.1", erster.port), -1);
}

// Statische Dateien aus dem Web-Ordner; alles andere geht als JSON an den Behandler
TEST(HttpServer, StatischeDateienUndJsonFuerAnderePfade) {
    // Eigener Ordner mit index.html
    const auto ordner = std::filesystem::temp_directory_path() / ("marketlens_web_" + std::to_string(getpid()));
    std::filesystem::create_directories(ordner);
    // index.html schreiben
    std::ofstream(ordner / "index.html") << "<h1>MarketLens</h1>";
    // Behandler wie in main.cpp (nur der Katalog spielt hier eine Rolle)
    TestQuelle quelle;
    Anfragebehandlung api({Wertpapier("IBM", "IBM", "USA", "USD")}, quelle, {});
    // Server mit dem Ordner
    {
        // Server läuft nur in diesem Block
        LaufenderServer server(api, 2, ordner);
        // Client
        httplib::Client c = server.client();
        // "/" liefert index.html
        const auto start = c.Get("/");
        ASSERT_TRUE(start);
        EXPECT_EQ(start->status, 200);
        EXPECT_EQ(start->body, "<h1>MarketLens</h1>");
        // Unbekannter Pfad: JSON-404 der Anfragebehandlung
        const auto fehlt = c.Get("/gibtsnicht.html");
        ASSERT_TRUE(fehlt);
        EXPECT_EQ(fehlt->status, 404);
        EXPECT_NE(fehlt->body.find("unknown path"), std::string::npos);
        // API funktioniert daneben
        const auto liste = c.Get("/api/wertpapiere");
        ASSERT_TRUE(liste);
        EXPECT_EQ(liste->status, 200);
        // Sauber beenden
        EXPECT_TRUE(server.stoppeUndWarte());
    }
    // Aufräumen
    std::filesystem::remove_all(ordner);
}
