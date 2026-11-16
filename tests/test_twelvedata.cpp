// Die Teile von F3, die hier getestet werden
#include "twelvedata_quelle.hpp"
#include "alphavantage_quelle.hpp"
#include "anfragebehandlung.hpp"
#include "dateiquelle.hpp"
#include "katalog.hpp"
// Fake-HTTP-Client und Fehlerklassen
#include "fake_http_client.hpp"
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>
// JSON-Bibliothek, um Antworten der Schnittstelle zu lesen
#include <nlohmann/json.hpp>

// std::abs
#include <cmath>
// setenv, unsetenv
#include <cstdlib>
// std::ifstream
#include <fstream>
// std::map
#include <map>
// std::stringstream
#include <sstream>
// std::string
#include <string>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Ordner mit den Twelve-Data-Testdaten
const std::string ordner = std::string(FIXTURES_DIR) + "/twelvedata";

// Liest eine Testdatei aus tests/fixtures/twelvedata/
std::string fixture(const std::string& name) {
    // Datei öffnen
    std::ifstream datei(ordner + "/" + name);
    // Fehlt sie, deutlich scheitern
    if (!datei) throw std::runtime_error("fixture not found: " + name);
    // Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    return puffer.str();
}

// Kalendertag als Zeitpunkt, für den Vergleich mit Kerze::datum
std::chrono::system_clock::time_point tag(int j, unsigned m, unsigned t) {
    // Jahr/Monat/Tag
    return std::chrono::sys_days{std::chrono::year{j} / std::chrono::month{m} / std::chrono::day{t}};
}

// Variable für den API-Key in diesen Tests (eigene, damit ein echter Key nicht stört)
const char* const keyVariable = "MARKETLENS_TEST_TD_KEY";

// US-Wertpapier, das bei Twelve Data geführt wird
const Wertpapier ibm("IBM", "International Business Machines", "USA", "USD", {{"alphavantage", "IBM"}, {"twelvedata", "IBM"}});

// Erlaubte Rundungsabweichung
constexpr double eps = 1e-9;

// Testfixture: Key setzen und wieder entfernen
class TwelveDataTest : public ::testing::Test {
protected:
    // Vor jedem Test
    void SetUp() override { setenv(keyVariable, "testkey", 1); }
    // Nach jedem Test
    void TearDown() override { unsetenv(keyVariable); }
    // Fake-Client
    FakeHttpClient fake;
};

// Prüft, dass parse() einen HttpFehler mit dem erwarteten Status wirft und die Meldung einen Text enthält
void erwarteHttpFehler(const std::string& json, int status, const std::string& textteil) {
    // Fehler abfangen
    try {
        // Parsen
        TwelveDataQuelle::parse(json, "IBM");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status
        EXPECT_EQ(f.status(), status);
        // Meldung
        EXPECT_NE(std::string(f.what()).find(textteil), std::string::npos) << f.what();
    }
}

}  // namespace

// ================================================================ Wertpapier und Katalog: Kürzel je Anbieter

// Ohne "symbole" gilt das Wertpapier nur beim Standardanbieter, mit "symbol" als Kürzel (Verhalten wie vor F3)
TEST(SymboleJeAnbieter, WertpapierOhneUndMitSymbolen) {
    // Ohne "symbole"
    const Wertpapier alt("SAP", "SAP SE", "Deutschland", "EUR");
    EXPECT_TRUE(alt.bietet("alphavantage"));
    EXPECT_FALSE(alt.bietet("twelvedata"));
    EXPECT_EQ(alt.symbolFuer("alphavantage"), "SAP");
    // Mit "symbole": nur die eingetragenen Anbieter, jeweils mit ihrem Kürzel
    const Wertpapier neu("MBG", "Mercedes", "Deutschland", "EUR", {{"alphavantage", "MBG.DEX"}, {"twelvedata", "MBG"}});
    EXPECT_EQ(neu.symbolFuer("alphavantage"), "MBG.DEX");
    EXPECT_EQ(neu.symbolFuer("twelvedata"), "MBG");
    EXPECT_FALSE(neu.bietet("anderer"));
    // Nicht geführt: Fehler statt eines falschen Kürzels
    EXPECT_THROW(neu.symbolFuer("anderer"), std::invalid_argument);
    // Leeres Kürzel oder leerer Anbieter sind ungültig
    EXPECT_THROW(Wertpapier("X", "X", "USA", "USD", {{"twelvedata", ""}}), std::invalid_argument);
    EXPECT_THROW(Wertpapier("X", "X", "USA", "USD", {{"", "X"}}), std::invalid_argument);
}

// Der echte Katalog: Alpha Vantage liefert alle 6, Twelve Data nur die beiden US-Wertpapiere
TEST(SymboleJeAnbieter, EchterKatalogNachAnbieter) {
    // Katalog lesen
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // Keine Fehler
    EXPECT_TRUE(k.fehler().empty());
    // Alpha Vantage: alle
    EXPECT_EQ(k.fuerAnbieter("alphavantage").size(), 6u);
    // Twelve Data: nur IBM und AAPL, in Katalogreihenfolge
    const auto td = k.fuerAnbieter("twelvedata");
    ASSERT_EQ(td.size(), 2u);
    EXPECT_EQ(td[0].symbol, "IBM");
    EXPECT_EQ(td[1].symbol, "AAPL");
    // Unbekannter Anbieter: nichts
    EXPECT_TRUE(k.fuerAnbieter("gibtsnicht").empty());
}

// Fehlerhafte "symbole" werden wie andere fehlerhafte Einträge gemeldet und übersprungen
TEST(SymboleJeAnbieter, FehlerhafteSymboleImKatalog) {
    // Vier Einträge: gültig, kein Objekt, keine Zeichenkette, leeres Kürzel
    const Katalog k = Katalog::ausJson(R"json({"wertpapiere": [
        {"symbol": "A", "name": "A", "land": "USA", "waehrung": "USD", "symbole": {"twelvedata": "A"}},
        {"symbol": "B", "name": "B", "land": "USA", "waehrung": "USD", "symbole": ["twelvedata"]},
        {"symbol": "C", "name": "C", "land": "USA", "waehrung": "USD", "symbole": {"twelvedata": 7}},
        {"symbol": "D", "name": "D", "land": "USA", "waehrung": "USD", "symbole": {"twelvedata": ""}}]})json");
    // Nur A gültig
    ASSERT_EQ(k.wertpapiere().size(), 1u);
    EXPECT_EQ(k.wertpapiere()[0].symbol, "A");
    // Drei Fehler mit Positionen 1, 2, 3
    ASSERT_EQ(k.fehler().size(), 3u);
    EXPECT_EQ(k.fehler()[0].position, 1u);
    EXPECT_EQ(k.fehler()[1].position, 2u);
    EXPECT_EQ(k.fehler()[2].position, 3u);
    // Meldungen nennen das Feld
    EXPECT_NE(k.fehler()[0].meldung.find("symbole"), std::string::npos) << k.fehler()[0].meldung;
    EXPECT_NE(k.fehler()[1].meldung.find("symbole.twelvedata"), std::string::npos) << k.fehler()[1].meldung;
}

// ================================================================ TwelveDataQuelle

// Ohne API-Key scheitert schon der Konstruktor, vor jeder Anfrage (wie E2)
TEST_F(TwelveDataTest, FehlenderKeyVorDerErstenAnfrage) {
    // Key entfernen
    unsetenv(keyVariable);
    // Konstruktor wirft
    EXPECT_THROW(TwelveDataQuelle(fake, keyVariable), KonfigurationsFehler);
    // Keine Anfrage
    EXPECT_TRUE(fake.anfragen().empty());
}

// URL mit Kürzel, Tagesintervall, 100 Einträgen und Key; Sonderzeichen kodiert
TEST_F(TwelveDataTest, Url) {
    // Quelle
    const TwelveDataQuelle quelle(fake, keyVariable);
    // Erwartete URL
    EXPECT_EQ(quelle.url("IBM"),
              "https://api.twelvedata.com/time_series?symbol=IBM&interval=1day&outputsize=100&apikey=testkey");
    // Kodierung
    EXPECT_NE(quelle.url("A&B").find("symbol=A%26B&"), std::string::npos);
}

// Gültige Antwort: neuester Tag zuerst -> aufsteigend, Zahlen als Text gelesen
TEST_F(TwelveDataTest, GueltigeAntwort) {
    // Quelle
    TwelveDataQuelle quelle(fake, keyVariable);
    // Antwort unter der URL des Twelve-Data-Kürzels
    fake.setzeAntwort(quelle.url("IBM"), 200, fixture("gueltig.json"));
    // Laden
    const Kursreihe r = quelle.ladeKurse(ibm);
    // Drei Tage, aufsteigend
    ASSERT_EQ(r.groesse(), 3u);
    EXPECT_EQ(r[0].datum, tag(2024, 1, 3));
    EXPECT_EQ(r[2].datum, tag(2024, 1, 5));
    // Alle Felder des ersten Tages
    EXPECT_NEAR(r[0].open, 159.0, eps);
    EXPECT_NEAR(r[0].high, 160.9, eps);
    EXPECT_NEAR(r[0].low, 158.4, eps);
    EXPECT_NEAR(r[0].close, 160.4, eps);
    EXPECT_NEAR(r[0].volumen, 3800000.0, eps);
    // Eine Anfrage
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// Angefragt wird das Twelve-Data-Kürzel aus "symbole", nicht "symbol"
TEST_F(TwelveDataTest, FragtKuerzelDesAnbietersAn) {
    // Quelle
    TwelveDataQuelle quelle(fake, keyVariable);
    // Anzeige-Symbol und Twelve-Data-Kürzel verschieden
    const Wertpapier wp("ANZEIGE", "Test", "USA", "USD", {{"twelvedata", "XYZ"}});
    // Antwort nur unter dem Kürzel XYZ
    fake.setzeAntwort(quelle.url("XYZ"), 200, fixture("gueltig.json"));
    // Laden klappt, also wurde XYZ angefragt
    EXPECT_EQ(quelle.ladeKurse(wp).groesse(), 3u);
    // Wertpapier, das Twelve Data nicht führt: Programmierfehler (der Server filtert vorher), keine Anfrage
    const Wertpapier nurAv("SAP", "SAP", "Deutschland", "EUR");
    EXPECT_THROW(quelle.ladeKurse(nurAv), std::invalid_argument);
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// Fehler im Inhalt (HTTP 200 mit "status": "error"): je Code der passende Fehler
TEST(TwelveDataFehler, FehlerImInhalt) {
    // Ungültiger Key
    erwarteHttpFehler(fixture("fehler_401.json"), 401, "API key");
    // Nur im bezahlten Tarif: wie 404, Meldung nennt beide möglichen Gründe und den Text von Twelve Data
    erwarteHttpFehler(fixture("fehler_404_tarif.json"), 404, "free plan");
    erwarteHttpFehler(fixture("fehler_404_tarif.json"), 404, "Grow or Venture plan");
    // Ungültige Anfrage
    erwarteHttpFehler(fixture("fehler_400.json"), 400, "symbol");
    // Kontingent: eigener Fehlertyp
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("fehler_429.json"), "IBM"), KontingentFehler);
    // Fehler ohne Code
    erwarteHttpFehler(R"({"status":"error","message":"irgendwas"})", 0, "irgendwas");
}

// Fehler mit echtem HTTP-Status: ebenso übersetzt, mit dem Text aus "message"
TEST_F(TwelveDataTest, FehlerImHttpStatus) {
    // Quelle
    TwelveDataQuelle quelle(fake, keyVariable);
    // URL
    const std::string u = quelle.url("IBM");
    // HTTP 401 mit Inhalt: Meldung enthält den Text von Twelve Data
    fake.setzeAntwort(u, 401, fixture("fehler_401.json"));
    try {
        // Laden
        quelle.ladeKurse(ibm);
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status und Text
        EXPECT_EQ(f.status(), 401);
        EXPECT_NE(std::string(f.what()).find("apikey"), std::string::npos) << f.what();
    }
    // HTTP 429: Kontingent
    fake.setzeAntwort(u, 429, fixture("fehler_429.json"));
    EXPECT_THROW(quelle.ladeKurse(ibm), KontingentFehler);
    // HTTP 500 ohne JSON: allgemeiner HttpFehler
    fake.setzeAntwort(u, 500, "Internal Server Error");
    EXPECT_THROW(quelle.ladeKurse(ibm), HttpFehler);
    // Netzwerk weg
    fake.setzeAusfall(u, FakeHttpClient::Ausfall::Netzwerk);
    EXPECT_THROW(quelle.ladeKurse(ibm), NetzwerkFehler);
}

// Ungültige Antworten: jeweils ParseFehler
TEST(TwelveDataFehler, UngueltigeAntworten) {
    // Abgeschnitten, kein Objekt, ohne values
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("abgeschnitten.json"), "IBM"), ParseFehler);
    EXPECT_THROW(TwelveDataQuelle::parse("[]", "IBM"), ParseFehler);
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("ohne_values.json"), "IBM"), ParseFehler);
    // Keine Zahl, Datum mit Uhrzeit, doppelter Tag
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("keine_zahl.json"), "IBM"), ParseFehler);
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("datum_mit_uhrzeit.json"), "IBM"), ParseFehler);
    EXPECT_THROW(TwelveDataQuelle::parse(fixture("doppelter_tag.json"), "IBM"), ParseFehler);
    // Fehlendes Feld: Meldung nennt Datum und Feld
    try {
        // close fehlt am 2024-01-05
        TwelveDataQuelle::parse(fixture("fehlendes_feld.json"), "IBM");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const ParseFehler& f) {
        // Datum und Feld
        EXPECT_NE(std::string(f.what()).find("2024-01-05"), std::string::npos) << f.what();
        EXPECT_NE(std::string(f.what()).find("'close'"), std::string::npos) << f.what();
    }
    // Inkonsistente Kerze (E1): ParseFehler mit Datum
    try {
        // High unter Low
        TwelveDataQuelle::parse(fixture("inkonsistente_kerze.json"), "IBM");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const ParseFehler& f) {
        // Datum aus der E1-Meldung
        EXPECT_NE(std::string(f.what()).find("2024-01-05"), std::string::npos) << f.what();
    }
}

// Bewertung für den Festplatten-Cache (E7)
TEST(TwelveDataFehler, Bewertung) {
    // Gültig
    EXPECT_EQ(TwelveDataQuelle::bewerte({200, fixture("gueltig.json")}), Antwortbewertung::Verwertbar);
    // Kontingent im Inhalt und als HTTP-Status
    EXPECT_EQ(TwelveDataQuelle::bewerte({200, fixture("fehler_429.json")}), Antwortbewertung::KontingentErschoepft);
    EXPECT_EQ(TwelveDataQuelle::bewerte({429, ""}), Antwortbewertung::KontingentErschoepft);
    // Sonstige Fehler und kaputter Inhalt
    EXPECT_EQ(TwelveDataQuelle::bewerte({200, fixture("fehler_404_tarif.json")}), Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(TwelveDataQuelle::bewerte({401, fixture("fehler_401.json")}), Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(TwelveDataQuelle::bewerte({200, "kein json"}), Antwortbewertung::Unbrauchbar);
}

// ================================================================ Echte Daten (aufgezeichnet 2026-11-16)

// Echte Antworten für IBM und Apple: 100 Handelstage 2026-06-25 bis 2026-11-13, aufsteigend
//   IBM:  erster Close 258,26999; letzter Tag O 246,63 H 247,85001 L 244,83 C 247,13
//   AAPL: erster Close 275,14999; letzter Close 356,26999
TEST(TwelveDataAufgezeichnet, IbmUndApple) {
    // IBM
    const Kursreihe ibmReihe = TwelveDataQuelle::parse(fixture("aufgezeichnet_IBM.json"), "IBM");
    ASSERT_EQ(ibmReihe.groesse(), 100u);
    EXPECT_EQ(ibmReihe[0].datum, tag(2026, 6, 25));
    EXPECT_NEAR(ibmReihe[0].close, 258.26999, eps);
    EXPECT_EQ(ibmReihe[99].datum, tag(2026, 11, 13));
    EXPECT_NEAR(ibmReihe[99].open, 246.63, eps);
    EXPECT_NEAR(ibmReihe[99].high, 247.85001, eps);
    EXPECT_NEAR(ibmReihe[99].low, 244.83, eps);
    EXPECT_NEAR(ibmReihe[99].close, 247.13, eps);
    // Apple
    const Kursreihe aapl = TwelveDataQuelle::parse(fixture("aufgezeichnet_AAPL.json"), "AAPL");
    ASSERT_EQ(aapl.groesse(), 100u);
    EXPECT_NEAR(aapl[0].close, 275.14999, eps);
    EXPECT_NEAR(aapl[99].close, 356.26999, eps);
}

// Gegenprobe: Beide Anbieter liefern an denselben Tagen dieselben Schlusskurse bis auf Rundung (höchstens 0,01).
// Verglichen werden nur Preise: Das Volumen zählen die Anbieter unterschiedlich.
// Benutzt die Dateien in daten/ (beide aufgezeichnet), damit der Vergleich die tatsächlich ausgelieferten Daten prüft.
TEST(TwelveDataAufgezeichnet, GleicheKurseWieAlphaVantage) {
    // Beide Wertpapiere
    for (const std::string symbol : {"IBM", "AAPL"}) {
        // Dasselbe Wertpapier bei beiden Anbietern
        const Wertpapier wp(symbol, symbol, "USA", "USD", {{"alphavantage", symbol}, {"twelvedata", symbol}});
        // Kurse beider Anbieter aus ihren Ordnern
        const Kursreihe av = DateiQuelle(std::string(DATEN_DIR) + "/alphavantage").ladeKurse(wp);
        const Kursreihe td = DateiQuelle(std::string(DATEN_DIR) + "/twelvedata", TwelveDataQuelle::parse).ladeKurse(wp);
        // Schlusskurse von Alpha Vantage nach Datum
        std::map<std::chrono::system_clock::time_point, double> avClose;
        for (std::size_t i = 0; i < av.groesse(); ++i) avClose[av[i].datum] = av[i].close;
        // Gemeinsame Tage vergleichen
        int gemeinsam = 0;
        for (std::size_t i = 0; i < td.groesse(); ++i) {
            // Tag auch bei Alpha Vantage?
            const auto it = avClose.find(td[i].datum);
            if (it == avClose.end()) continue;
            // Höchstens 0,01 Abweichung (plus kleine Rechenungenauigkeit)
            EXPECT_LE(std::abs(td[i].close - it->second), 0.01 + 1e-9) << symbol << " Tag " << i;
            // Gezählt
            ++gemeinsam;
        }
        // Deutlich überlappende Zeiträume (bei der Aufnahme 2026-11-16: alle 100 Tage)
        EXPECT_GE(gemeinsam, 50) << symbol;
    }
}

// Jedes Wertpapier, das Twelve Data laut Katalog führt, lässt sich aus daten/twelvedata/ laden
TEST(TwelveDataAufgezeichnet, OfflineDatenVollstaendig) {
    // Echter Katalog, nur Twelve Data
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    const auto liste = k.fuerAnbieter("twelvedata");
    ASSERT_EQ(liste.size(), 2u);
    // Quelle mit dem Twelve-Data-Format
    DateiQuelle quelle(std::string(DATEN_DIR) + "/twelvedata", TwelveDataQuelle::parse);
    // Jedes laden, mindestens 34 Kerzen (MACD 12,26,9)
    for (const Wertpapier& wp : liste) {
        // Hier landet die Reihe
        Kursreihe reihe;
        // Darf nicht werfen
        ASSERT_NO_THROW(reihe = quelle.ladeKurse(wp)) << wp.symbol;
        EXPECT_GE(reihe.groesse(), 34u) << wp.symbol;
    }
}

// ================================================================ Schnittstelle des Twelve-Data-Servers

// So wie main.cpp den Server für Port 8081 aufbaut: nur US-Wertpapiere, Anbieter in jeder Antwort
TEST(TwelveDataServer, NurUsWertpapiereUndAnbieterInDerAntwort) {
    // Katalog gefiltert auf Twelve Data
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // Offline-Quelle im Twelve-Data-Format
    DateiQuelle quelle(std::string(DATEN_DIR) + "/twelvedata", TwelveDataQuelle::parse);
    // Schnittstelle mit Anbietername
    Anfragebehandlung api(k.fuerAnbieter("twelvedata"), quelle, {}, "Twelve Data");
    // Liste: genau IBM und AAPL, Anbieter genannt
    const auto liste = nlohmann::json::parse(api.behandle(Anfrage{"GET", "/api/wertpapiere", {}}).body);
    ASSERT_EQ(liste["wertpapiere"].size(), 2u);
    EXPECT_EQ(liste["wertpapiere"][0]["symbol"], "IBM");
    EXPECT_EQ(liste["wertpapiere"][1]["symbol"], "AAPL");
    EXPECT_EQ(liste["anbieter"], "Twelve Data");
    // Kurse und Indikatoren für IBM: 200 mit Anbieter
    const Antwort kurse = api.behandle(Anfrage{"GET", "/api/kurse", {{"symbol", "IBM"}}});
    ASSERT_EQ(kurse.status, 200) << kurse.body;
    EXPECT_EQ(nlohmann::json::parse(kurse.body)["anbieter"], "Twelve Data");
    EXPECT_EQ(nlohmann::json::parse(kurse.body)["kerzen"].size(), 100u);
    EXPECT_EQ(api.behandle(Anfrage{"GET", "/api/indikatoren", {{"symbol", "IBM"}}}).status, 200);
    // Ein Wertpapier, das nur Alpha Vantage führt, gibt es hier nicht: 404 wie ein unbekanntes Symbol
    EXPECT_EQ(api.behandle(Anfrage{"GET", "/api/kurse", {{"symbol", "MBG.DEX"}}}).status, 404);
}

// Ohne Anbieternamen (wie in den bestehenden Tests) steht "anbieter": null in der Antwort
TEST(TwelveDataServer, OhneAnbieterNamenIstAnbieterNull) {
    // Quelle und Schnittstelle ohne Namen
    DateiQuelle quelle(std::string(DATEN_DIR) + "/alphavantage");
    Anfragebehandlung api({Wertpapier("IBM", "IBM", "USA", "USD")}, quelle, {});
    // Feld vorhanden und null
    const auto j = nlohmann::json::parse(api.behandle(Anfrage{"GET", "/api/wertpapiere", {}}).body);
    ASSERT_TRUE(j.contains("anbieter"));
    EXPECT_TRUE(j["anbieter"].is_null());
}
