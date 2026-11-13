// Die Klassen, die hier getestet werden
#include "alphavantage_quelle.hpp"
#include "http_cache.hpp"
// Fakes für Netzwerk und Uhr
#include "fake_http_client.hpp"
#include "fake_uhr.hpp"
// Fehlerklassen
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::chrono für Zeiten und Dauern
#include <chrono>
// std::filesystem für das temporäre Cache-Verzeichnis
#include <filesystem>
// std::ifstream und std::ofstream zum Lesen und Beschädigen von Cache-Dateien
#include <fstream>
// std::stringstream
#include <sstream>
// std::string
#include <string>
// getpid für einen eindeutigen Ordnernamen
#include <unistd.h>

// Kurze Schreibweise für Stunden, Minuten usw. (z. B. 24h)
using namespace std::chrono_literals;

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Liest eine Datei komplett ein (leer, wenn sie fehlt)
std::string liesDatei(const std::filesystem::path& pfad) {
    // Datei im Binärmodus öffnen
    std::ifstream datei(pfad, std::ios::binary);
    // Inhalt in einen Puffer lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Als String zurückgeben
    return puffer.str();
}

// Überschreibt eine Datei mit einem Text (zum gezielten Beschädigen von Cache-Dateien)
void schreibeDatei(const std::filesystem::path& pfad, const std::string& inhalt) {
    // Datei im Binärmodus öffnen und leeren
    std::ofstream datei(pfad, std::ios::binary | std::ios::trunc);
    // Inhalt schreiben
    datei << inhalt;
}

// Feste Startzeit für alle Tests: 2026-11-13 12:00 UTC
const std::chrono::system_clock::time_point startzeit =
    std::chrono::sys_days{std::chrono::year{2026} / 11 / 13} + 12h;

// Eine Test-URL (mit "API-Key", um zu prüfen, dass er nicht auf der Platte landet)
const std::string url = "https://example.test/query?symbol=IBM&apikey=GEHEIM123";

// Testfixture: legt vor jedem Test einen leeren, eigenen Cache-Ordner an und löscht ihn danach
class CacheTest : public ::testing::Test {
protected:
    // Vor jedem Test
    void SetUp() override {
        // Name des laufenden Tests, damit jeder Test seinen eigenen Ordner bekommt
        const std::string test = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        // Ordner im temporären Verzeichnis des Systems, eindeutig durch Prozessnummer und Testname
        ordner = std::filesystem::temp_directory_path() /
                 ("marketlens_cache_" + std::to_string(getpid()) + "_" + test);
        // Reste eines früheren Laufs entfernen
        std::filesystem::remove_all(ordner);
    }
    // Nach jedem Test: Ordner samt Inhalt löschen
    void TearDown() override { std::filesystem::remove_all(ordner); }

    // Cache mit 24 Stunden Gültigkeit und Standard-Bewertung erzeugen
    CachingHttpClient cache() { return CachingHttpClient(fake, uhr, ordner, 24h); }

    // Der Cache-Ordner dieses Tests
    std::filesystem::path ordner;
    // Fake-Netzwerk (innerer Client)
    FakeHttpClient fake;
    // Fake-Uhr
    FakeUhr uhr{startzeit};
};

}  // namespace

// ---------------------------------------------------------------- Treffer und Ablauf

// Zweiter Abruf innerhalb der Gültigkeit kommt aus dem Cache: keine zweite Anfrage
TEST_F(CacheTest, TrefferOhneNeueAnfrage) {
    // Antwort festlegen
    fake.setzeAntwort(url, 200, "daten-1");
    // Cache anlegen
    CachingHttpClient c = cache();
    // Erster Abruf: geht ans Netz
    EXPECT_EQ(c.get(url).body, "daten-1");
    // Eine Stunde später: aus dem Cache, auch wenn der Server inzwischen anderes liefern würde
    uhr.vorstellen(1h);
    fake.setzeAntwort(url, 200, "daten-2");
    const HttpAntwort zweite = c.get(url);
    // Noch die alten (gültigen) Daten
    EXPECT_EQ(zweite.body, "daten-1");
    // Nicht als veraltet markiert
    EXPECT_FALSE(zweite.veraltet);
    EXPECT_TRUE(zweite.hinweis.empty());
    // Nur eine Anfrage ans Netz
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// "Beim nächsten Start": ein neues Cache-Objekt mit demselben Ordner findet die Datei wieder
TEST_F(CacheTest, UeberlebtNeustart) {
    // Antwort festlegen
    fake.setzeAntwort(url, 200, "daten-1");
    // Erster "Programmlauf"
    {
        // Cache anlegen und abrufen
        CachingHttpClient c = cache();
        c.get(url);
    }
    // Zweiter "Programmlauf" mit neuem Objekt
    CachingHttpClient c2 = cache();
    // Liefert die gespeicherten Daten
    EXPECT_EQ(c2.get(url).body, "daten-1");
    // Ohne zweite Anfrage
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// Abgelaufener Eintrag wird neu abgerufen und ersetzt; die Grenze (genau 24 h) zählt als abgelaufen
TEST_F(CacheTest, AbgelaufenerEintragWirdNeuAbgerufen) {
    // Erste Antwort
    fake.setzeAntwort(url, 200, "alt");
    // Cache anlegen
    CachingHttpClient c = cache();
    // Erster Abruf
    c.get(url);
    // 23 h 59 min später: noch gültig
    uhr.vorstellen(24h - 1min);
    EXPECT_EQ(c.get(url).body, "alt");
    EXPECT_EQ(fake.anfragen().size(), 1u);
    // Genau 24 h nach dem Abruf: abgelaufen
    uhr.vorstellen(1min);
    fake.setzeAntwort(url, 200, "neu");
    // Neue Daten
    EXPECT_EQ(c.get(url).body, "neu");
    // Zweite Anfrage
    EXPECT_EQ(fake.anfragen().size(), 2u);
    // Und die neuen Daten sind jetzt gespeichert (eine Stunde später ohne Anfrage)
    uhr.vorstellen(1h);
    EXPECT_EQ(c.get(url).body, "neu");
    EXPECT_EQ(fake.anfragen().size(), 2u);
}

// Ein Abrufzeitpunkt in der Zukunft (Uhr zurückgestellt) gilt als abgelaufen
TEST_F(CacheTest, ZeitpunktInDerZukunftGiltAlsAbgelaufen) {
    // Antwort festlegen
    fake.setzeAntwort(url, 200, "daten");
    // Cache anlegen und abrufen
    CachingHttpClient c = cache();
    c.get(url);
    // Uhr eine Stunde zurückstellen
    uhr.vorstellen(-1h);
    // Erneuter Abruf geht ans Netz
    c.get(url);
    EXPECT_EQ(fake.anfragen().size(), 2u);
}

// Gültigkeit 0: jeder Abruf geht ans Netz, aber der Eintrag dient trotzdem als Reserve bei Ausfall
TEST_F(CacheTest, GueltigkeitNull) {
    // Antwort festlegen
    fake.setzeAntwort(url, 200, "daten");
    // Cache ohne Gültigkeit
    CachingHttpClient c(fake, uhr, ordner, 0s);
    // Zwei Abrufe, zwei Anfragen
    c.get(url);
    c.get(url);
    EXPECT_EQ(fake.anfragen().size(), 2u);
    // Netz fällt aus: alte Daten mit Hinweis
    fake.setzeAusfall(url, FakeHttpClient::Ausfall::Netzwerk);
    EXPECT_TRUE(c.get(url).veraltet);
}

// ---------------------------------------------------------------- Ausfall: alte Daten mit Hinweis

// Netzwerkfehler bei abgelaufenem Eintrag: alte Daten, markiert als veraltet, Hinweis mit Datum und Grund
TEST_F(CacheTest, NetzwerkfehlerLiefertAlteDatenMitHinweis) {
    // Erster Abruf klappt
    fake.setzeAntwort(url, 200, "alt");
    CachingHttpClient c = cache();
    c.get(url);
    // Zwei Tage später ist das Netz weg
    uhr.vorstellen(48h);
    fake.setzeAusfall(url, FakeHttpClient::Ausfall::Netzwerk);
    // Abruf
    const HttpAntwort a = c.get(url);
    // Alte Daten
    EXPECT_EQ(a.body, "alt");
    EXPECT_EQ(a.status, 200);
    // Als veraltet markiert
    EXPECT_TRUE(a.veraltet);
    // Hinweis nennt den Abrufzeitpunkt und den Grund
    EXPECT_NE(a.hinweis.find("2026-11-13 12:00 UTC"), std::string::npos) << a.hinweis;
    EXPECT_NE(a.hinweis.find("network error"), std::string::npos) << a.hinweis;
}

// Zeitüberschreitung ist ein Netzwerkfehler und wird genauso behandelt
TEST_F(CacheTest, ZeitueberschreitungLiefertAlteDaten) {
    // Erster Abruf klappt
    fake.setzeAntwort(url, 200, "alt");
    CachingHttpClient c = cache();
    c.get(url);
    // Abgelaufen, Server antwortet nicht rechtzeitig
    uhr.vorstellen(48h);
    fake.setzeAusfall(url, FakeHttpClient::Ausfall::Zeitueberschreitung);
    // Alte Daten mit Hinweis
    EXPECT_TRUE(c.get(url).veraltet);
}

// HTTP 429 bei abgelaufenem Eintrag: alte Daten mit Hinweis auf das Kontingent; 429 wird nicht gespeichert
TEST_F(CacheTest, KontingentLiefertAlteDatenMitHinweis) {
    // Erster Abruf klappt
    fake.setzeAntwort(url, 200, "alt");
    CachingHttpClient c = cache();
    c.get(url);
    // Abgelaufen, Kontingent erschöpft
    uhr.vorstellen(48h);
    fake.setzeAntwort(url, 429, "too many requests");
    // Abruf
    const HttpAntwort a = c.get(url);
    // Alte Daten mit Hinweis
    EXPECT_EQ(a.body, "alt");
    EXPECT_TRUE(a.veraltet);
    EXPECT_NE(a.hinweis.find("quota"), std::string::npos) << a.hinweis;
    // Die Datei enthält weiterhin die alten Daten, nicht die 429-Antwort
    EXPECT_NE(liesDatei(c.dateiFuer(url)).find("alt"), std::string::npos);
}

// Ohne Eintrag gibt es nichts Altes: Netzwerkfehler wird weitergegeben, 429 unverändert zurückgegeben
TEST_F(CacheTest, OhneEintragKeineReserve) {
    // Cache anlegen
    CachingHttpClient c = cache();
    // Netz weg
    fake.setzeAusfall(url, FakeHttpClient::Ausfall::Netzwerk);
    EXPECT_THROW(c.get(url), NetzwerkFehler);
    // Kontingent erschöpft
    const std::string url2 = "https://example.test/anderes";
    fake.setzeAntwort(url2, 429, "");
    const HttpAntwort a = c.get(url2);
    // Unveränderte 429-Antwort, nicht als veraltet markiert
    EXPECT_EQ(a.status, 429);
    EXPECT_FALSE(a.veraltet);
    // Nichts gespeichert
    EXPECT_FALSE(std::filesystem::exists(c.dateiFuer(url2)));
}

// Unbrauchbare Antwort (404) wird nicht gespeichert und nicht durch alte Daten ersetzt
TEST_F(CacheTest, UnbrauchbareAntwortWirdNichtGespeichert) {
    // Cache anlegen
    CachingHttpClient c = cache();
    // 404
    fake.setzeAntwort(url, 404, "not found");
    // Unverändert zurück
    EXPECT_EQ(c.get(url).status, 404);
    // Keine Datei
    EXPECT_FALSE(std::filesystem::exists(c.dateiFuer(url)));
}

// ---------------------------------------------------------------- Beschädigte Dateien

// Jede Art von Beschädigung wird erkannt: neu abrufen statt abstürzen, und die Datei wird repariert
TEST_F(CacheTest, BeschaedigteDateienWerdenIgnoriert) {
    // Antwort festlegen
    fake.setzeAntwort(url, 200, "daten");
    // Cache anlegen und einmal füllen
    CachingHttpClient c = cache();
    c.get(url);
    // Pfad der Cache-Datei
    const auto pfad = c.dateiFuer(url);
    // Gültiger Inhalt als Vorlage für die Beschädigungen
    const std::string gut = liesDatei(pfad);
    // Liste von Beschädigungen
    const std::vector<std::string> kaputt = {
        "",                                   // leere Datei
        "irgendwas\n",                        // falsche Kennung
        gut.substr(0, 30),                    // mitten im Kopf abgeschnitten
        gut.substr(0, gut.size() - 2),        // Inhalt abgeschnitten (zu kurz)
        gut + "xx",                           // zusätzliche Bytes am Ende
        gut.substr(0, gut.size() - 5) + "DATEN",  // Inhalt verändert (Prüfsumme stimmt nicht)
    };
    // Jede Beschädigung einzeln prüfen
    for (std::size_t i = 0; i < kaputt.size(); ++i) {
        // Datei beschädigen
        schreibeDatei(pfad, kaputt[i]);
        // Anzahl Anfragen vorher
        const std::size_t vorher = fake.anfragen().size();
        // Abruf darf nicht abstürzen und muss die richtigen Daten liefern
        EXPECT_EQ(c.get(url).body, "daten") << "Beschädigung " << i;
        // Er musste neu abrufen
        EXPECT_EQ(fake.anfragen().size(), vorher + 1) << "Beschädigung " << i;
        // Danach ist die Datei wieder gültig (wieder genau der gute Inhalt)
        EXPECT_EQ(liesDatei(pfad), gut) << "Beschädigung " << i;
    }
}

// Eine Datei, die zu einer anderen URL gehört, wird nicht verwendet
TEST_F(CacheTest, DateiEinerAnderenUrlWirdNichtVerwendet) {
    // Zwei URLs mit verschiedenen Antworten
    const std::string url2 = "https://example.test/zwei";
    fake.setzeAntwort(url, 200, "eins");
    fake.setzeAntwort(url2, 200, "zwei");
    // Cache anlegen, beide abrufen
    CachingHttpClient c = cache();
    c.get(url);
    c.get(url2);
    // Datei von url2 über die von url kopieren
    std::filesystem::copy_file(c.dateiFuer(url2), c.dateiFuer(url),
                               std::filesystem::copy_options::overwrite_existing);
    // Abruf von url darf nicht "zwei" liefern
    EXPECT_EQ(c.get(url).body, "eins");
}

// Ein beschädigter Eintrag ist keine Reserve: bei Netzausfall kommt der Fehler
TEST_F(CacheTest, BeschaedigterEintragIstKeineReserve) {
    // Füllen
    fake.setzeAntwort(url, 200, "daten");
    CachingHttpClient c = cache();
    c.get(url);
    // Beschädigen
    schreibeDatei(c.dateiFuer(url), "kaputt");
    // Netz weg
    fake.setzeAusfall(url, FakeHttpClient::Ausfall::Netzwerk);
    // Fehler statt beschädigter Daten
    EXPECT_THROW(c.get(url), NetzwerkFehler);
}

// ---------------------------------------------------------------- Datenschutz und Konfiguration

// Weder der API-Key noch die URL stehen in den Dateinamen oder Dateien
TEST_F(CacheTest, KeinApiKeyAufDerPlatte) {
    // Füllen
    fake.setzeAntwort(url, 200, "daten");
    CachingHttpClient c = cache();
    c.get(url);
    // Alle Dateien im Ordner durchsuchen
    for (const auto& datei : std::filesystem::directory_iterator(ordner)) {
        // Dateiname ohne Key
        EXPECT_EQ(datei.path().filename().string().find("GEHEIM123"), std::string::npos);
        // Inhalt ohne Key und ohne URL
        const std::string inhalt = liesDatei(datei.path());
        EXPECT_EQ(inhalt.find("GEHEIM123"), std::string::npos);
        EXPECT_EQ(inhalt.find("example.test"), std::string::npos);
    }
}

// Ungültige Konfiguration wird abgelehnt
TEST_F(CacheTest, UngueltigeKonfiguration) {
    // Negative Gültigkeit
    EXPECT_THROW(CachingHttpClient(fake, uhr, ordner, -1s), std::invalid_argument);
    // Ordner kann nicht angelegt werden, weil an der Stelle eine Datei liegt
    std::filesystem::create_directories(ordner);
    const auto datei = ordner / "ich_bin_eine_datei";
    schreibeDatei(datei, "x");
    EXPECT_THROW(CachingHttpClient(fake, uhr, datei, 1h), KonfigurationsFehler);
}

// ---------------------------------------------------------------- Alpha-Vantage-Bewertung

// Kontingent-Hinweis mit HTTP 200 wird nicht gespeichert; alte Daten werden mit Hinweis geliefert
TEST_F(CacheTest, AlphaVantageKontingentMitStatus200) {
    // Cache mit der Alpha-Vantage-Bewertung
    CachingHttpClient c(fake, uhr, ordner, 24h, AlphaVantageQuelle::bewerte);
    // Gültige Kursdaten (minimal)
    const std::string daten = R"json({"Time Series (Daily)": {"2026-11-13":
        {"1. open": "10", "2. high": "12", "3. low": "9", "4. close": "11", "5. volume": "100"}}})json";
    // Erster Abruf: gültige Daten werden gespeichert
    fake.setzeAntwort(url, 200, daten);
    c.get(url);
    EXPECT_TRUE(std::filesystem::exists(c.dateiFuer(url)));
    // Zwei Tage später: Kontingent erschöpft, aber mit HTTP 200
    uhr.vorstellen(48h);
    fake.setzeAntwort(url, 200, R"({"Note": "Our standard API call frequency is 25 requests per day."})");
    // Abruf liefert die alten Kursdaten mit Hinweis
    const HttpAntwort a = c.get(url);
    EXPECT_EQ(a.body, daten);
    EXPECT_TRUE(a.veraltet);
    // Die Datei enthält weiterhin die Kursdaten, nicht den Hinweis
    EXPECT_EQ(liesDatei(c.dateiFuer(url)).find("Note"), std::string::npos);
}

// Die Bewertungsfunktion von Alpha Vantage für alle Fälle
TEST(AlphaVantageBewertung, AlleFaelle) {
    // Gültige Daten
    EXPECT_EQ(AlphaVantageQuelle::bewerte({200, R"json({"Time Series (Daily)": {}})json"}),
              Antwortbewertung::Verwertbar);
    // Kontingent als "Note" und als "Information"
    EXPECT_EQ(AlphaVantageQuelle::bewerte({200, R"({"Note": "call frequency"})"}),
              Antwortbewertung::KontingentErschoepft);
    EXPECT_EQ(AlphaVantageQuelle::bewerte({200, R"({"Information": "rate limit is 25 requests per day"})"}),
              Antwortbewertung::KontingentErschoepft);
    // HTTP 429
    EXPECT_EQ(AlphaVantageQuelle::bewerte({429, ""}), Antwortbewertung::KontingentErschoepft);
    // Unbekanntes Symbol, kaputtes JSON, Fehlerstatus
    EXPECT_EQ(AlphaVantageQuelle::bewerte({200, R"({"Error Message": "Invalid API call"})"}),
              Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(AlphaVantageQuelle::bewerte({200, "kein json"}), Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(AlphaVantageQuelle::bewerte({500, ""}), Antwortbewertung::Unbrauchbar);
}

// Die Standard-Bewertung entscheidet nur nach dem Status
TEST(StandardBewertung, NachStatus) {
    // 2xx verwertbar
    EXPECT_EQ(standardBewertung({200, ""}), Antwortbewertung::Verwertbar);
    EXPECT_EQ(standardBewertung({204, ""}), Antwortbewertung::Verwertbar);
    // 429 Kontingent
    EXPECT_EQ(standardBewertung({429, ""}), Antwortbewertung::KontingentErschoepft);
    // Sonst unbrauchbar
    EXPECT_EQ(standardBewertung({404, ""}), Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(standardBewertung({500, ""}), Antwortbewertung::Unbrauchbar);
}
