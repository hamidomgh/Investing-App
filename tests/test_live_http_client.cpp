// Die Klasse, die hier getestet wird
#include "live_http_client.hpp"
// Cache (E7) für den Test der ganzen Kette
#include "http_cache.hpp"
// Fehlerklassen
#include "quellenfehler.hpp"
// Fake-Uhr für den Cache
#include "fake_uhr.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>
// Server der gleichen Bibliothek: ein lokaler Testserver statt des echten Anbieters (kein Internet nötig)
#include <httplib.h>

// std::chrono
#include <chrono>
// std::filesystem für den Cache-Ordner
#include <filesystem>
// std::string
#include <string>
// std::thread für den Server und std::this_thread::sleep_for
#include <thread>
// getpid für einen eindeutigen Ordnernamen
#include <unistd.h>

// Kurze Schreibweise für Zeitangaben
using namespace std::chrono_literals;

// Anonymer Namespace: Hilfsklassen nur in dieser Datei sichtbar
namespace {

// Lokaler HTTP-Server auf 127.0.0.1 mit freiem Port, der wie ein Anbieter antwortet:
//   /ok?...       200 mit der empfangenen Abfrage als Inhalt (zeigt, dass Pfad und Abfrage ankommen)
//   /status/<n>   Antwort mit Status n und Inhalt "fehler <n>"
//   /langsam      antwortet erst nach 1 s
class TestServer {
public:
    // Routen anlegen, Port wählen, Server in eigenem Thread starten
    TestServer() {
        // 200 mit der Abfrage als Text
        server_.Get("/ok", [](const httplib::Request& anfrage, httplib::Response& antwort) {
            // Wert des Parameters "symbol" zurückschicken
            antwort.set_content("symbol=" + anfrage.get_param_value("symbol"), "text/plain");
        });
        // Beliebiger Status aus dem Pfad
        server_.Get(R"(/status/(\d+))", [](const httplib::Request& anfrage, httplib::Response& antwort) {
            // Erste Gruppe des regulären Ausdrucks: die Zahl
            const int status = std::stoi(anfrage.matches[1]);
            antwort.status = status;
            antwort.set_content("fehler " + std::to_string(status), "text/plain");
        });
        // Langsamer Server für die Zeitüberschreitung
        server_.Get("/langsam", [](const httplib::Request&, httplib::Response& antwort) {
            // Länger warten, als der Client im Test erlaubt
            std::this_thread::sleep_for(1s);
            antwort.set_content("zu spät", "text/plain");
        });
        // Freien Port wählen lassen
        port = server_.bind_to_any_port("127.0.0.1");
        // listen_after_bind() blockiert, deshalb eigener Thread
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        // Warten, bis Anfragen angenommen werden
        server_.wait_until_ready();
    }

    // Server stoppen und Thread einsammeln
    ~TestServer() {
        // Beendet listen_after_bind()
        server_.stop();
        // Auf den Thread warten
        thread_.join();
    }

    // Adresse des Servers, z. B. "http://127.0.0.1:43210"
    std::string basis() const { return "http://127.0.0.1:" + std::to_string(port); }

    // Gewählter Port
    int port = 0;

private:
    // Der eigentliche Server
    httplib::Server server_;
    // Thread, in dem er läuft
    std::thread thread_;
};

// Liefert einen Port, auf dem sicher niemand mehr lauscht: Testserver starten, Port merken, Server beenden.
// (Nur binden reicht nicht: httplib lauscht schon nach bind_to_any_port, und ohne laufenden Server
// schließt stop() den Socket nicht. Die Verbindung würde dann hängen statt abgelehnt zu werden.)
int freierPortOhneServer() {
    // Der Destruktor von TestServer stoppt den Server und schließt den Socket
    TestServer server;
    return server.port;
}

}  // namespace

// Die URL wird in Basis (Schema, Rechner, Port) und Pfad mit Abfrage zerlegt
TEST(LiveHttpClientUrl, Zerlegung) {
    // Normalfall mit Port, Pfad und Abfrage
    const auto a = LiveHttpClient::zerlege("https://api.twelvedata.com:8443/time_series?symbol=IBM&apikey=x");
    EXPECT_EQ(a.basis, "https://api.twelvedata.com:8443");
    EXPECT_EQ(a.pfad, "/time_series?symbol=IBM&apikey=x");
    EXPECT_TRUE(a.https);
    // Nur Rechner: Pfad "/"
    const auto b = LiveHttpClient::zerlege("http://localhost");
    EXPECT_EQ(b.basis, "http://localhost");
    EXPECT_EQ(b.pfad, "/");
    EXPECT_FALSE(b.https);
    // Abfrage ohne Pfad: "/" wird ergänzt
    const auto c = LiveHttpClient::zerlege("http://localhost?x=1");
    EXPECT_EQ(c.basis, "http://localhost");
    EXPECT_EQ(c.pfad, "/?x=1");
}

// Unbrauchbare URLs werden als Konfigurationsfehler gemeldet
TEST(LiveHttpClientUrl, UngueltigeUrl) {
    // Falsches Schema
    EXPECT_THROW(LiveHttpClient::zerlege("ftp://example.com/datei"), KonfigurationsFehler);
    // Kein Schema
    EXPECT_THROW(LiveHttpClient::zerlege("example.com/pfad"), KonfigurationsFehler);
    // Kein Rechner
    EXPECT_THROW(LiveHttpClient::zerlege("https:///pfad"), KonfigurationsFehler);
}

// Eine gültige Antwort kommt mit Status und Inhalt an; Pfad und Abfrage erreichen den Server
TEST(LiveHttpClient, GueltigeAntwort) {
    TestServer server;
    LiveHttpClient client;
    const HttpAntwort antwort = client.get(server.basis() + "/ok?symbol=IBM");
    EXPECT_EQ(antwort.status, 200);
    EXPECT_EQ(antwort.body, "symbol=IBM");
    // Frische Antwort aus dem Netz, nicht aus dem Cache
    EXPECT_FALSE(antwort.veraltet);
}

// Fehlerstatus werden zurückgegeben, nicht geworfen (Vertrag von IHttpClient; pruefeStatus wertet sie aus)
TEST(LiveHttpClient, FehlerstatusWirdZurueckgegeben) {
    TestServer server;
    LiveHttpClient client;
    // Typische Fehler der Anbieter
    for (const int status : {401, 404, 429, 500}) {
        const HttpAntwort antwort = client.get(server.basis() + "/status/" + std::to_string(status));
        EXPECT_EQ(antwort.status, status);
        EXPECT_EQ(antwort.body, "fehler " + std::to_string(status));
    }
}

// Lauscht niemand auf dem Port, gibt es einen NetzwerkFehler; die Meldung enthält den API-Key nicht
TEST(LiveHttpClient, KeineVerbindungIstNetzwerkFehler) {
    const int port = freierPortOhneServer();
    LiveHttpClient client(1s, 1s);
    try {
        client.get("http://127.0.0.1:" + std::to_string(port) + "/ok?apikey=GEHEIM");
        FAIL() << "NetzwerkFehler erwartet";
    } catch (const ZeitueberschreitungFehler&) {
        // Verbindung abgelehnt ist keine Zeitüberschreitung
        FAIL() << "NetzwerkFehler erwartet, nicht ZeitueberschreitungFehler";
    } catch (const NetzwerkFehler& f) {
        // Rechner und Port stehen drin, der Key nicht
        const std::string meldung = f.what();
        EXPECT_NE(meldung.find("127.0.0.1"), std::string::npos) << meldung;
        EXPECT_EQ(meldung.find("GEHEIM"), std::string::npos) << meldung;
    }
}

// Antwortet der Server nicht rechtzeitig, gibt es eine Zeitüberschreitung (auch ein NetzwerkFehler)
TEST(LiveHttpClient, LangsamerServerIstZeitueberschreitung) {
    TestServer server;
    // Lesezeit 200 ms, der Server braucht 1 s
    LiveHttpClient client(1s, 200ms);
    EXPECT_THROW(client.get(server.basis() + "/langsam"), ZeitueberschreitungFehler);
}

// HTTPS ohne OpenSSL-Build: klare Meldung mit Hinweis auf die CMake-Option, ohne Netzzugriff
TEST(LiveHttpClient, HttpsOhneOpenSslIstKonfigurationsFehler) {
    // Im Live-Build gibt es HTTPS; dann ist dieser Fall nicht prüfbar
    if (LiveHttpClient::unterstuetztHttps()) GTEST_SKIP() << "mit MARKETLENS_LIVE_HTTP gebaut";
    LiveHttpClient client;
    try {
        client.get("https://api.twelvedata.com/time_series?apikey=GEHEIM");
        FAIL() << "KonfigurationsFehler erwartet";
    } catch (const KonfigurationsFehler& f) {
        const std::string meldung = f.what();
        EXPECT_NE(meldung.find("MARKETLENS_LIVE_HTTP"), std::string::npos) << meldung;
        EXPECT_EQ(meldung.find("GEHEIM"), std::string::npos) << meldung;
    }
}

// Ganze Kette wie im Programm: CachingHttpClient (E7) um den LiveHttpClient.
// Erst wird live geladen und gespeichert; ist der Server danach weg, liefert der Cache die alten Daten mit Hinweis.
TEST(LiveHttpClient, MitCacheBeiNetzausfallAlteDaten) {
    // Eigener Ordner je Testlauf
    const std::filesystem::path ordner =
        std::filesystem::temp_directory_path() / ("marketlens_live_" + std::to_string(getpid()));
    std::filesystem::remove_all(ordner);
    FakeUhr uhr(std::chrono::system_clock::now());
    LiveHttpClient live(1s, 1s);
    // Gültigkeit 1 Stunde
    CachingHttpClient cache(live, uhr, ordner, std::chrono::hours{1});

    // URL des Servers merken; Server nur in diesem Block
    std::string url;
    {
        TestServer server;
        url = server.basis() + "/ok?symbol=AAPL";
        // Erste Anfrage: live, wird gespeichert
        const HttpAntwort erste = cache.get(url);
        EXPECT_EQ(erste.body, "symbol=AAPL");
        EXPECT_FALSE(erste.veraltet);
    }
    // Server ist jetzt gestoppt; zwei Stunden später ist der Eintrag abgelaufen
    uhr.vorstellen(std::chrono::hours{2});
    // Neuer Abruf scheitert am Netz, der Cache springt mit den alten Daten ein
    const HttpAntwort zweite = cache.get(url);
    EXPECT_EQ(zweite.body, "symbol=AAPL");
    EXPECT_TRUE(zweite.veraltet);
    EXPECT_FALSE(zweite.hinweis.empty());

    // Aufräumen
    std::filesystem::remove_all(ordner);
}
