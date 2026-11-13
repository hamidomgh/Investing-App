// Die Klasse, die hier getestet wird
#include "alphavantage_quelle.hpp"
// Fake-HTTP-Client für Tests ohne Netzwerk
#include "fake_http_client.hpp"
// Fehlerklassen
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// setenv und unsetenv für den API-Key im Test
#include <cstdlib>
// std::ifstream zum Lesen der Testdateien
#include <fstream>
// std::stringstream, um eine Datei komplett in einen String zu lesen
#include <sstream>
// std::string
#include <string>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Liest eine Testdatei aus tests/fixtures/alphavantage/. FIXTURES_DIR setzt CMake beim Kompilieren.
std::string fixture(const std::string& name) {
    // Vollständiger Pfad der Datei
    const std::string pfad = std::string(FIXTURES_DIR) + "/alphavantage/" + name;
    // Datei öffnen
    std::ifstream datei(pfad);
    // Fehlt die Datei, soll der Test deutlich scheitern statt mit leerem Text weiterzulaufen
    if (!datei) {
        // Fehler mit Pfad
        throw std::runtime_error("fixture not found: " + pfad);
    }
    // Ganzen Inhalt in einen Puffer lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Als String zurückgeben
    return puffer.str();
}

// Variable für den API-Key in diesen Tests (eigene Variable, damit ein echter Key in der Umgebung nicht stört)
const char* const keyVariable = "MARKETLENS_TEST_AV_KEY";

// Testfixture von GoogleTest: setzt vor jedem Test einen Key und räumt danach auf
class AlphaVantageTest : public ::testing::Test {
protected:
    // Vor jedem Test: Key setzen
    void SetUp() override { setenv(keyVariable, "testkey", 1); }
    // Nach jedem Test: Key wieder entfernen
    void TearDown() override { unsetenv(keyVariable); }

    // Ein US-Wertpapier für die Tests
    const Wertpapier ibm{"IBM", "International Business Machines", "USA", "USD"};
    // Der Fake-Client, der die Antworten liefert
    FakeHttpClient fake;
};

// Kalendertag als Zeitpunkt, für den Vergleich mit Kerze::datum
std::chrono::system_clock::time_point tag(int j, unsigned m, unsigned t) {
    // Jahr/Monat/Tag als sys_days, umgewandelt in einen Zeitpunkt
    return std::chrono::sys_days{std::chrono::year{j} / std::chrono::month{m} / std::chrono::day{t}};
}

// Erlaubte Rundungsabweichung
constexpr double eps = 1e-9;

}  // namespace

// ---------------------------------------------------------------- API-Key und URL

// Ohne API-Key scheitert schon der Konstruktor, und es wird keine einzige Anfrage gestellt (DoD E2)
TEST_F(AlphaVantageTest, FehlenderKeyVorDerErstenAnfrage) {
    // Key entfernen
    unsetenv(keyVariable);
    // Konstruktor wirft KonfigurationsFehler
    EXPECT_THROW(AlphaVantageQuelle(fake, keyVariable), KonfigurationsFehler);
    // Es wurde nichts angefragt
    EXPECT_TRUE(fake.anfragen().empty());
}

// Die URL enthält Funktion, Symbol und Key; Sonderzeichen werden kodiert
TEST_F(AlphaVantageTest, UrlEnthaeltSymbolUndKey) {
    // Quelle mit dem Test-Key
    const AlphaVantageQuelle quelle(fake, keyVariable);
    // Normales Symbol
    EXPECT_EQ(quelle.url("IBM"),
              "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=IBM&outputsize=compact&apikey=testkey");
    // Symbol mit Börsen-Suffix: der Punkt bleibt erhalten
    EXPECT_NE(quelle.url("SAP.DEX").find("symbol=SAP.DEX&"), std::string::npos);
    // Index-Symbol mit "^" und "&" wird kodiert, damit es die URL nicht zerstört
    EXPECT_NE(quelle.url("^A&B").find("symbol=%5EA%26B&"), std::string::npos);
}

// ---------------------------------------------------------------- Erfolgsfall

// Gültige Antwort: drei Kerzen, zeitlich aufsteigend, Werte aus dem Text korrekt gelesen
TEST_F(AlphaVantageTest, GueltigeAntwortWirdZuKursreihe) {
    // Quelle anlegen
    AlphaVantageQuelle quelle(fake, keyVariable);
    // Fake liefert die gültige Antwort unter genau der URL, die die Quelle anfragen wird
    fake.setzeAntwort(quelle.url("IBM"), 200, fixture("daily_ok.json"));
    // Kurse laden
    const Kursreihe reihe = quelle.ladeKurse(ibm);
    // Drei Tage
    ASSERT_EQ(reihe.groesse(), 3u);
    // Aufsteigend sortiert, obwohl die Datei mit dem neuesten Tag beginnt
    EXPECT_EQ(reihe[0].datum, tag(2024, 1, 3));
    EXPECT_EQ(reihe[1].datum, tag(2024, 1, 4));
    EXPECT_EQ(reihe[2].datum, tag(2024, 1, 5));
    // Alle Felder des ersten Tages (Text "159.0000" usw. als Zahl gelesen)
    EXPECT_NEAR(reihe[0].open, 159.0, eps);
    EXPECT_NEAR(reihe[0].high, 160.9, eps);
    EXPECT_NEAR(reihe[0].low, 158.4, eps);
    EXPECT_NEAR(reihe[0].close, 160.4, eps);
    EXPECT_NEAR(reihe[0].volumen, 3800000.0, eps);
    // Schlusskurs des letzten Tages
    EXPECT_NEAR(reihe[2].close, 162.75, eps);
    // Genau eine Anfrage
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// ---------------------------------------------------------------- Fehler bei der Übertragung

// Netzwerkfehler und Zeitüberschreitung kommen beim Aufrufer an
TEST_F(AlphaVantageTest, NetzwerkfehlerUndZeitueberschreitung) {
    // Quelle anlegen
    AlphaVantageQuelle quelle(fake, keyVariable);
    // Netzwerkausfall
    fake.setzeAusfall(quelle.url("IBM"), FakeHttpClient::Ausfall::Netzwerk);
    EXPECT_THROW(quelle.ladeKurse(ibm), NetzwerkFehler);
    // Zeitüberschreitung
    fake.setzeAusfall(quelle.url("IBM"), FakeHttpClient::Ausfall::Zeitueberschreitung);
    EXPECT_THROW(quelle.ladeKurse(ibm), ZeitueberschreitungFehler);
}

// HTTP-Fehlerstatus werden über pruefeStatus unterschieden
TEST_F(AlphaVantageTest, HttpFehlerstatus) {
    // Quelle anlegen
    AlphaVantageQuelle quelle(fake, keyVariable);
    // 401: ungültiger Schlüssel
    fake.setzeAntwort(quelle.url("IBM"), 401, "");
    EXPECT_THROW(quelle.ladeKurse(ibm), HttpFehler);
    // 429: Kontingent erschöpft
    fake.setzeAntwort(quelle.url("IBM"), 429, "");
    EXPECT_THROW(quelle.ladeKurse(ibm), KontingentFehler);
    // 500: Serverfehler
    fake.setzeAntwort(quelle.url("IBM"), 500, "");
    EXPECT_THROW(quelle.ladeKurse(ibm), HttpFehler);
}

// ---------------------------------------------------------------- Hinweise von Alpha Vantage mit Status 200

// "Error Message" (unbekanntes Symbol) wird wie HTTP 404 behandelt
TEST_F(AlphaVantageTest, ErrorMessageWieNichtGefunden) {
    // Status abfangen
    try {
        // Antwort parsen
        AlphaVantageQuelle::parse(fixture("error_message.json"), "XYZ");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Wie 404
        EXPECT_EQ(f.status(), 404);
        // Meldung nennt das Symbol
        EXPECT_NE(std::string(f.what()).find("XYZ"), std::string::npos) << f.what();
    }
}

// Kontingent erschöpft, in beiden Formen ("Note" und "Information" mit "rate limit")
TEST_F(AlphaVantageTest, KontingentHinweisMitStatus200) {
    // Ältere Form
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("note_kontingent.json"), "IBM"), KontingentFehler);
    // Neuere Form
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("information_kontingent.json"), "IBM"), KontingentFehler);
}

// Sonstiger Hinweis: allgemeiner QuellenFehler, aber KEIN Kontingentfehler
TEST_F(AlphaVantageTest, SonstigerHinweis) {
    // Merkt sich, welcher Fehler ankam
    bool kontingent = false;
    bool quelle = false;
    // Antwort parsen
    try {
        // Premium-Hinweis
        AlphaVantageQuelle::parse(fixture("information_sonstiges.json"), "IBM");
    } catch (const KontingentFehler&) {
        // Falsch
        kontingent = true;
    } catch (const QuellenFehler&) {
        // Richtig
        quelle = true;
    }
    // Prüfen
    EXPECT_FALSE(kontingent);
    EXPECT_TRUE(quelle);
}

// ---------------------------------------------------------------- Ungültige Antworten

// Ungültiges JSON, fehlende Zeitreihe, fehlendes Feld, keine Zahl, ungültiges Datum: jeweils ParseFehler
TEST_F(AlphaVantageTest, UngueltigeAntwortenWerdenAlsParseFehlerGemeldet) {
    // Abgeschnittenes JSON
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("ungueltiges_json.json"), "IBM"), ParseFehler);
    // Kein JSON-Objekt
    EXPECT_THROW(AlphaVantageQuelle::parse("[1, 2, 3]", "IBM"), ParseFehler);
    // Leerer Text
    EXPECT_THROW(AlphaVantageQuelle::parse("", "IBM"), ParseFehler);
    // "Time Series (Daily)" fehlt
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("ohne_zeitreihe.json"), "IBM"), ParseFehler);
    // "4. close" ist kein Zahlentext
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("keine_zahl.json"), "IBM"), ParseFehler);
    // 30. Februar
    EXPECT_THROW(AlphaVantageQuelle::parse(fixture("ungueltiges_datum.json"), "IBM"), ParseFehler);
}

// Fehlendes Feld: Meldung nennt Datum und Feldname
TEST_F(AlphaVantageTest, FehlendesFeldNenntDatumUndFeld) {
    // Meldung abfangen
    try {
        // "4. close" fehlt am 2024-01-05
        AlphaVantageQuelle::parse(fixture("fehlendes_feld.json"), "IBM");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const ParseFehler& f) {
        // Meldung als String
        const std::string m = f.what();
        // Datum und Feld enthalten
        EXPECT_NE(m.find("2024-01-05"), std::string::npos) << m;
        EXPECT_NE(m.find("4. close"), std::string::npos) << m;
    }
}

// Inkonsistente Kerze (High unter Low) wird von der E1-Validierung abgelehnt und als ParseFehler gemeldet
TEST_F(AlphaVantageTest, InkonsistenteKerzeWirdAbgelehnt) {
    // Meldung abfangen
    try {
        // High 160 unter Low 161,25
        AlphaVantageQuelle::parse(fixture("inkonsistente_kerze.json"), "IBM");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const ParseFehler& f) {
        // Meldung nennt das Datum (kommt aus der E1-Meldung)
        EXPECT_NE(std::string(f.what()).find("2024-01-05"), std::string::npos) << f.what();
    }
}

// Zahlen als echte JSON-Zahlen statt Text werden ebenfalls akzeptiert; leere Zeitreihe ergibt leere Kursreihe
TEST_F(AlphaVantageTest, ZahlenOhneAnfuehrungszeichenUndLeereReihe) {
    // Ein Tag mit echten Zahlen. Raw-String mit eigenem Begrenzer "json", weil der Text selbst )" enthält
    const std::string text = R"json({"Time Series (Daily)": {"2024-01-05":
        {"1. open": 10, "2. high": 12, "3. low": 9, "4. close": 11, "5. volume": 100}}})json";
    // Parsen
    const Kursreihe reihe = AlphaVantageQuelle::parse(text, "IBM");
    // Ein Tag mit Schlusskurs 11
    ASSERT_EQ(reihe.groesse(), 1u);
    EXPECT_NEAR(reihe[0].close, 11.0, eps);
    // Leere Zeitreihe: kein Fehler, leere Kursreihe
    EXPECT_EQ(AlphaVantageQuelle::parse(R"json({"Time Series (Daily)": {}})json", "IBM").groesse(), 0u);
}

// Alle Fehler der Quelle lassen sich gemeinsam als QuellenFehler fangen (wichtig für den Katalog in Teil 3)
TEST_F(AlphaVantageTest, AlleFehlerSindQuellenFehler) {
    // Quelle anlegen
    AlphaVantageQuelle quelle(fake, keyVariable);
    // Ungültiges JSON
    fake.setzeAntwort(quelle.url("IBM"), 200, "kein json");
    EXPECT_THROW(quelle.ladeKurse(ibm), QuellenFehler);
    // Unbekanntes Symbol
    fake.setzeAntwort(quelle.url("IBM"), 200, fixture("error_message.json"));
    EXPECT_THROW(quelle.ladeKurse(ibm), QuellenFehler);
}
