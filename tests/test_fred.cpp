// Die Teile, die hier getestet werden
#include "datei_zinsquelle.hpp"
#include "fred_quelle.hpp"
#include "katalog.hpp"
#include "zentralbank.hpp"
#include "zinsabruf.hpp"
// Für den Test über die Schnittstelle (E8) und die Ereignisstudie (E6)
#include "anfragebehandlung.hpp"
// Offline-Kursquelle für den Test mit echten IBM-Kursen
#include "dateiquelle.hpp"
#include "ereignisstudie.hpp"
// Fake-HTTP-Client und Fehlerklassen
#include "fake_http_client.hpp"
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>
// JSON-Bibliothek, um die Antwort der Schnittstelle zu lesen
#include <nlohmann/json.hpp>

// std::chrono
#include <chrono>
// setenv, unsetenv
#include <cstdlib>
// std::ifstream
#include <fstream>
// std::stringstream
#include <sstream>
// std::string
#include <string>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Ordner mit den FRED-Testdaten
const std::string fredOrdner = std::string(FIXTURES_DIR) + "/fred";

// Liest eine Testdatei aus tests/fixtures/fred/
std::string fixture(const std::string& name) {
    // Datei öffnen
    std::ifstream datei(fredOrdner + "/" + name);
    // Fehlt sie, soll der Test deutlich scheitern
    if (!datei) {
        // Fehler mit Namen
        throw std::runtime_error("fixture not found: " + name);
    }
    // Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Als String
    return puffer.str();
}

// Kalendertag kurz schreiben
std::chrono::sys_days tag(int j, unsigned m, unsigned t) {
    // Jahr/Monat/Tag
    return std::chrono::year{j} / std::chrono::month{m} / std::chrono::day{t};
}

// Die Fed wie im Katalog
const Zentralbank fed("Fed", {"USA"}, "USD", "DFEDTARU");

// Variable für den API-Key in diesen Tests
const char* const keyVariable = "MARKETLENS_TEST_FRED_KEY";

// Testfixture: Key setzen und wieder entfernen
class FredTest : public ::testing::Test {
protected:
    // Vor jedem Test
    void SetUp() override { setenv(keyVariable, "testkey", 1); }
    // Nach jedem Test
    void TearDown() override { unsetenv(keyVariable); }
    // Fake-Client
    FakeHttpClient fake;
};

}  // namespace

// ================================================================ Zentralbank

// Zuständigkeit über die Länderliste; ungültige Zentralbanken werden abgelehnt
TEST(Zentralbank, ZustaendigkeitUndValidierung) {
    // EZB mit zwei Ländern
    const Zentralbank ezb("EZB", {"Deutschland", "Frankreich"}, "EUR", "ECBDFR");
    // Zuständig für beide, nicht für die USA
    EXPECT_TRUE(ezb.zustaendigFuer("Deutschland"));
    EXPECT_TRUE(ezb.zustaendigFuer("Frankreich"));
    EXPECT_FALSE(ezb.zustaendigFuer("USA"));
    // Leere Felder und leere Länderliste
    EXPECT_THROW(Zentralbank("", {"A"}, "EUR", "S"), std::invalid_argument);
    EXPECT_THROW(Zentralbank("X", {}, "EUR", "S"), std::invalid_argument);
    EXPECT_THROW(Zentralbank("X", {"A", ""}, "EUR", "S"), std::invalid_argument);
    EXPECT_THROW(Zentralbank("X", {"A"}, "", "S"), std::invalid_argument);
    EXPECT_THROW(Zentralbank("X", {"A"}, "EUR", ""), std::invalid_argument);
}

// ================================================================ Tageswerte lesen und Entscheidungen ableiten

// fed_ok.json: 9 Beobachtungen, eine ohne Wert ("."), zwei vertauschte Tage -> 8 Werte, aufsteigend
TEST(FredQuelle, LeseBeobachtungen) {
    // Lesen
    const auto b = FredQuelle::leseBeobachtungen(fixture("fed_ok.json"), "DFEDTARU");
    // Der Tag mit "." fehlt
    ASSERT_EQ(b.size(), 8u);
    // Aufsteigend sortiert, obwohl der 19. und 20. September in der Datei vertauscht sind
    EXPECT_EQ(b[0].datum, tag(2024, 9, 16));
    EXPECT_EQ(b[3].datum, tag(2024, 9, 19));
    EXPECT_EQ(b[4].datum, tag(2024, 9, 20));
    // Werte
    EXPECT_DOUBLE_EQ(b[0].wert, 5.50);
    EXPECT_DOUBLE_EQ(b[3].wert, 5.00);
    EXPECT_DOUBLE_EQ(b[7].wert, 4.75);
}

// Entscheidungen = Tage, an denen sich der Wert ändert:
//   5.50 (16.-18.9.) -> 5.00 ab 19.9.  -> Entscheidung 2024-09-19, 5.00
//   5.00 (bis 7.11.) -> 4.75 ab 8.11.  -> Entscheidung 2024-11-08, 4.75
//   Der Tag ohne Wert (21.9.) unterbricht nichts; der erste Tag ist keine Entscheidung.
TEST(FredQuelle, LeiteEntscheidungenAb) {
    // Lesen und ableiten
    const auto e = FredQuelle::leiteEntscheidungenAb(FredQuelle::leseBeobachtungen(fixture("fed_ok.json"), "S"), fed);
    // Zwei Entscheidungen
    ASSERT_EQ(e.size(), 2u);
    // Erste
    EXPECT_EQ(e[0].zentralbank, "Fed");
    EXPECT_EQ(e[0].land, "USA");
    EXPECT_EQ(e[0].datum, tag(2024, 9, 19));
    EXPECT_DOUBLE_EQ(e[0].leitzins, 5.00);
    // Zweite
    EXPECT_EQ(e[1].datum, tag(2024, 11, 8));
    EXPECT_DOUBLE_EQ(e[1].leitzins, 4.75);
}

// Eine Zentralbank mit mehreren Ländern: jede Entscheidung einmal pro Land
TEST(FredQuelle, EineEntscheidungProLand) {
    // EZB mit zwei Ländern
    const Zentralbank ezb("EZB", {"Deutschland", "Frankreich"}, "EUR", "ECBDFR");
    // Gleiche Tageswerte wie oben
    const auto e = FredQuelle::leiteEntscheidungenAb(FredQuelle::leseBeobachtungen(fixture("fed_ok.json"), "S"), ezb);
    // 2 Entscheidungen x 2 Länder
    ASSERT_EQ(e.size(), 4u);
    // Die erste Entscheidung für beide Länder
    EXPECT_EQ(e[0].land, "Deutschland");
    EXPECT_EQ(e[1].land, "Frankreich");
    EXPECT_EQ(e[0].datum, e[1].datum);
}

// Sonderfälle: keine Werte, nur ein Wert, konstanter Wert -> keine Entscheidungen
TEST(FredQuelle, OhneAenderungKeineEntscheidung) {
    // Hilfsfunktion: Entscheidungen aus einem JSON-Text
    auto aus = [](const std::string& text) {
        // Lesen und ableiten
        return FredQuelle::leiteEntscheidungenAb(FredQuelle::leseBeobachtungen(text, "S"), fed);
    };
    // Leere Liste
    EXPECT_TRUE(aus(R"({"observations": []})").empty());
    // Ein einziger Wert
    EXPECT_TRUE(aus(R"({"observations": [{"date": "2024-01-02", "value": "5.50"}]})").empty());
    // Gleicher Wert an mehreren Tagen
    EXPECT_TRUE(aus(R"({"observations": [{"date": "2024-01-02", "value": "5.50"},
                                         {"date": "2024-01-03", "value": "5.50"}]})")
                    .empty());
    // Negativer Leitzins ist erlaubt (EZB 2014-2022)
    const auto negativ = aus(R"({"observations": [{"date": "2019-09-17", "value": "-0.40"},
                                                   {"date": "2019-09-18", "value": "-0.50"}]})");
    ASSERT_EQ(negativ.size(), 1u);
    EXPECT_DOUBLE_EQ(negativ[0].leitzins, -0.50);
}

// Ungültige Antworten: jeweils ParseFehler
TEST(FredQuelle, UngueltigeAntworten) {
    // Abgeschnitten
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("abgeschnitten.json"), "S"), ParseFehler);
    // "observations" fehlt
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("ohne_observations.json"), "S"), ParseFehler);
    // Wert keine Zahl
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("keine_zahl.json"), "S"), ParseFehler);
    // Ungültiges Datum
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("ungueltiges_datum.json"), "S"), ParseFehler);
    // Doppelter Tag
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("doppeltes_datum.json"), "S"), ParseFehler);
    // Feld "value" fehlt
    EXPECT_THROW(FredQuelle::leseBeobachtungen(fixture("feld_fehlt.json"), "S"), ParseFehler);
    // Oberste Ebene keine Liste/kein Objekt
    EXPECT_THROW(FredQuelle::leseBeobachtungen("[]", "S"), ParseFehler);
}

// ================================================================ Laden über IHttpClient

// Ohne API-Key scheitert schon der Konstruktor, vor jeder Anfrage (wie E2)
TEST_F(FredTest, FehlenderKeyVorDerErstenAnfrage) {
    // Key entfernen
    unsetenv(keyVariable);
    // Konstruktor wirft
    EXPECT_THROW(FredQuelle(fake, keyVariable), KonfigurationsFehler);
    // Keine Anfrage
    EXPECT_TRUE(fake.anfragen().empty());
}

// URL mit Zeitreihe, Key, JSON-Format und Startdatum
TEST_F(FredTest, Url) {
    // Quelle mit Startdatum
    const FredQuelle quelle(fake, keyVariable, "2020-01-01");
    // Erwartete URL
    EXPECT_EQ(quelle.url("DFEDTARU"),
              "https://api.stlouisfed.org/fred/series/observations?series_id=DFEDTARU&api_key=testkey"
              "&file_type=json&observation_start=2020-01-01");
}

// Gültige Antwort über den Client
TEST_F(FredTest, LaedtUndLeitetAb) {
    // Quelle
    FredQuelle quelle(fake, keyVariable);
    // Fake liefert die gültige Antwort
    fake.setzeAntwort(quelle.url("DFEDTARU"), 200, fixture("fed_ok.json"));
    // Laden
    const auto e = quelle.ladeEntscheidungen(fed);
    // Zwei Entscheidungen, eine Anfrage
    EXPECT_EQ(e.size(), 2u);
    EXPECT_EQ(fake.anfragen().size(), 1u);
}

// Fehlerfälle wie in E2: Key ungültig, Zeitreihe unbekannt, Kontingent, Serverfehler, Netzwerk
TEST_F(FredTest, Fehlerfaelle) {
    // Quelle
    FredQuelle quelle(fake, keyVariable);
    // URL der Fed-Zeitreihe
    const std::string u = quelle.url("DFEDTARU");

    // 400 mit ungültigem Key: HttpFehler 400, Meldung enthält den Grund von FRED
    fake.setzeAntwort(u, 400, fixture("fehler_api_key.json"));
    try {
        // Laden
        quelle.ladeEntscheidungen(fed);
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status und Grund
        EXPECT_EQ(f.status(), 400);
        EXPECT_NE(std::string(f.what()).find("api_key"), std::string::npos) << f.what();
    }

    // 400 mit unbekannter Zeitreihe: wie 404 (wie ein unbekanntes Symbol in E2)
    fake.setzeAntwort(u, 400, fixture("serie_unbekannt.json"));
    try {
        // Laden
        quelle.ladeEntscheidungen(fed);
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status 404
        EXPECT_EQ(f.status(), 404);
    }

    // 429: Kontingent
    fake.setzeAntwort(u, 429, R"({"error_code":429,"error_message":"Too Many Requests."})");
    EXPECT_THROW(quelle.ladeEntscheidungen(fed), KontingentFehler);
    // 500 ohne JSON-Inhalt: allgemeiner HttpFehler
    fake.setzeAntwort(u, 500, "Internal Server Error");
    EXPECT_THROW(quelle.ladeEntscheidungen(fed), HttpFehler);
    // Netzwerk weg
    fake.setzeAusfall(u, FakeHttpClient::Ausfall::Netzwerk);
    EXPECT_THROW(quelle.ladeEntscheidungen(fed), NetzwerkFehler);
}

// Bewertung für den Festplatten-Cache (E7)
TEST(FredQuelle, Bewertung) {
    // Gültig
    EXPECT_EQ(FredQuelle::bewerte({200, fixture("fed_ok.json")}), Antwortbewertung::Verwertbar);
    // Kontingent
    EXPECT_EQ(FredQuelle::bewerte({429, ""}), Antwortbewertung::KontingentErschoepft);
    // Fehlerstatus und kaputter Inhalt
    EXPECT_EQ(FredQuelle::bewerte({400, fixture("fehler_api_key.json")}), Antwortbewertung::Unbrauchbar);
    EXPECT_EQ(FredQuelle::bewerte({200, "kein json"}), Antwortbewertung::Unbrauchbar);
}

// ================================================================ Offline-Quelle und Laden aller Banken

// DateiZinsquelle liest <Serie>.json; fehlende Datei und unzulässiger Name sind QuellenFehler
TEST(DateiZinsquelle, LiestUndPrueft) {
    // Quelle auf den Testordner
    DateiZinsquelle quelle(fredOrdner);
    // Zeitreihe "fed_ok" entspricht der Datei fed_ok.json
    const Zentralbank test("Test", {"USA"}, "USD", "fed_ok");
    EXPECT_EQ(quelle.ladeEntscheidungen(test).size(), 2u);
    // Fehlende Datei
    EXPECT_THROW(quelle.ladeEntscheidungen(Zentralbank("X", {"A"}, "EUR", "GIBTSNICHT")), QuellenFehler);
    // Name, der aus dem Ordner führen würde
    EXPECT_THROW(quelle.ladeEntscheidungen(Zentralbank("X", {"A"}, "EUR", "../geheim")), QuellenFehler);
}

// Eine fehlerhafte Zentralbank verhindert nicht die übrigen
TEST(Zinsabruf, FehlerEinerBankVerhindertNichtDieUebrigen) {
    // Offline-Quelle
    DateiZinsquelle quelle(fredOrdner);
    // Drei Banken: gültig, Datei fehlt, gültig
    const std::vector<Zentralbank> banken = {Zentralbank("A", {"USA"}, "USD", "fed_ok"),
                                             Zentralbank("B", {"Kanada"}, "CAD", "GIBTSNICHT"),
                                             Zentralbank("C", {"Japan"}, "JPY", "fed_ok")};
    // Laden
    const ZinsLadeergebnis e = ladeAlleEntscheidungen(quelle, banken);
    // Je 2 Entscheidungen von A und C
    EXPECT_EQ(e.entscheidungen.size(), 4u);
    // Ein Fehler, bei B
    ASSERT_EQ(e.fehler.size(), 1u);
    EXPECT_EQ(e.fehler[0].zentralbank, "B");
}

// ================================================================ Katalog

// Der echte Katalog enthält Fed und EZB; die EZB ist für Deutschland zuständig
TEST(KatalogZentralbanken, EchterKatalog) {
    // Katalog lesen
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // Keine Fehler
    EXPECT_TRUE(k.zentralbankFehler().empty());
    // Zwei Banken
    ASSERT_EQ(k.zentralbanken().size(), 2u);
    EXPECT_EQ(k.zentralbanken()[0].name, "Fed");
    EXPECT_EQ(k.zentralbanken()[1].name, "EZB");
    // 21 Euro-Länder, darunter Deutschland
    EXPECT_EQ(k.zentralbanken()[1].laender.size(), 21u);
    EXPECT_TRUE(k.zentralbanken()[1].zustaendigFuer("Deutschland"));
    // Jedes Wertpapier aus einem Euro-Land findet seine Zentralbank
    const Wertpapier* mbg = k.finde("MBG.DEX");
    ASSERT_NE(mbg, nullptr);
    EXPECT_TRUE(k.zentralbanken()[1].zustaendigFuer(mbg->land));
}

// Fehlerhafte Zentralbanken werden mit Position gemeldet; die übrigen bleiben
//   0 Fed           gültig
//   1 OhneLaender   "laender" fehlt
//   2 LeereListe    leere Liste (Konstruktor)
//   3 ZahlImLand    Zahl in der Liste
//   4 OhneSerie     leere fredSerie (Konstruktor)
//   5 Fed           doppelter Name
//   6 EZB           gültig
TEST(KatalogZentralbanken, FehlerhafteEintraege) {
    // Testkatalog lesen
    const Katalog k = Katalog::ausDatei(fredOrdner + "/../katalog/zentralbanken.json");
    // Zwei gültige
    ASSERT_EQ(k.zentralbanken().size(), 2u);
    EXPECT_EQ(k.zentralbanken()[0].name, "Fed");
    EXPECT_EQ(k.zentralbanken()[0].laender, std::vector<std::string>{"USA"});
    EXPECT_EQ(k.zentralbanken()[1].name, "EZB");
    // Fünf Fehler mit Positionen
    ASSERT_EQ(k.zentralbankFehler().size(), 5u);
    EXPECT_EQ(k.zentralbankFehler()[0].position, 1u);
    EXPECT_EQ(k.zentralbankFehler()[1].position, 2u);
    EXPECT_EQ(k.zentralbankFehler()[2].position, 3u);
    EXPECT_EQ(k.zentralbankFehler()[3].position, 4u);
    EXPECT_EQ(k.zentralbankFehler()[4].position, 5u);
    // Doppelter Name
    EXPECT_NE(k.zentralbankFehler()[4].meldung.find("duplicate"), std::string::npos);
}

// Katalog ohne Zentralbanken bleibt gültig; "zentralbanken" als Nicht-Liste macht ihn unbrauchbar
TEST(KatalogZentralbanken, OptionalerAbschnitt) {
    // Ohne Abschnitt: keine Banken, keine Fehler
    const Katalog ohne = Katalog::ausJson(R"({"wertpapiere": []})");
    EXPECT_TRUE(ohne.zentralbanken().empty());
    EXPECT_TRUE(ohne.zentralbankFehler().empty());
    // Abschnitt ist keine Liste
    EXPECT_THROW(Katalog::ausJson(R"({"wertpapiere": [], "zentralbanken": {}})"), ParseFehler);
}

// ================================================================ Von FRED bis zur Ereignisstudie

// Ganze Kette: FRED-Daten -> Entscheidungen -> Ereignisstudie für eine US-Aktie, über die Schnittstelle (E8)
//   Entscheidung wirksam Do 2024-09-19 -> Reaktionstag Fr 2024-09-20, Kurs vorher Do 101, nachher Fr 104
TEST(FredBisEreignisstudie, GanzeKette) {
    // Entscheidungen der Fed aus den Testdaten
    DateiZinsquelle zinsen(fredOrdner);
    const auto entscheidungen = zinsen.ladeEntscheidungen(Zentralbank("Fed", {"USA"}, "USD", "fed_ok"));
    // Kursquelle mit Handelstagen Mo 16.9. bis Fr 27.9.2024 (ohne Wochenende)
    class Quelle : public IPriceSource {
    public:
        // Feste Kurse
        Kursreihe ladeKurse(const Wertpapier&) override {
            // Handelstage und Schlusskurse
            const std::vector<std::pair<unsigned, double>> tage = {{16, 100}, {17, 100}, {18, 99},  {19, 101},
                                                                    {20, 104}, {23, 103}, {24, 105}, {25, 104},
                                                                    {26, 106}, {27, 107}};
            // Reihe
            Kursreihe r;
            // Kerzen anhängen
            for (const auto& [t, c] : tage) {
                // Open = High = Low = Close
                r.hinzufuegen(Kerze(tag(2024, 9, t), c, c, c, c, 1.0));
            }
            // Fertig
            return r;
        }
    };
    // Quelle
    Quelle quelle;
    // Schnittstelle mit einer US-Aktie und einer Aktie ohne passende Zentralbank
    Anfragebehandlung api({Wertpapier("IBM", "IBM", "USA", "USD"), Wertpapier("TSCO.LON", "Tesco", "Großbritannien", "GBP")},
                          quelle, entscheidungen);
    // US-Aktie
    const auto j = nlohmann::json::parse(api.behandle(Anfrage{"GET", "/api/ereignisstudie", {{"symbol", "IBM"}}}).body);
    // Art des Datums wird ausgewiesen
    EXPECT_EQ(j["datumsart"], "wirksam");
    // Nur die September-Entscheidung liegt im Kurszeitraum (die vom November wird übersprungen)
    ASSERT_EQ(j["ereignisse"].size(), 1u);
    EXPECT_EQ(j["ereignisse"][0]["zentralbank"], "Fed");
    EXPECT_EQ(j["ereignisse"][0]["datum"], "2024-09-19");
    EXPECT_EQ(j["ereignisse"][0]["reaktionstag"], "2024-09-20");
    EXPECT_DOUBLE_EQ(j["ereignisse"][0]["leitzins"].get<double>(), 5.00);
    EXPECT_DOUBLE_EQ(j["ereignisse"][0]["veraenderung"].get<double>(), 104.0 / 101.0 - 1.0);
    // Britische Aktie: keine passende Zentralbank -> leere Liste, kein Fehler
    const auto gb = nlohmann::json::parse(api.behandle(Anfrage{"GET", "/api/ereignisstudie", {{"symbol", "TSCO.LON"}}}).body);
    EXPECT_TRUE(gb["ereignisse"].empty());
}

// ================================================================ Aufgezeichnete echte FRED-Antworten

// Echte Antworten von FRED, aufgezeichnet am 2026-11-13 (observation_start 2015-01-01, ohne API-Key in der Datei).
// Prüfen das echte Format und dass die Zeitreihen DFEDTARU und ECBDFR wirklich tägliche Leitzinsen enthalten.
//   DFEDTARU: 4335 Tage (2015-01-01 bis 2026-11-13), 32 Änderungen, letzte: 2026-09-17 auf 4.00
//   ECBDFR:   4335 Tage, 23 Änderungen, erster Wert -0.20 (negativer Zins), letzte: 2026-09-16 auf 2.50
TEST(FredAufgezeichnet, FedUndEzb) {
    // Fed
    const auto fedTage = FredQuelle::leseBeobachtungen(fixture("aufgezeichnet_DFEDTARU.json"), "DFEDTARU");
    EXPECT_EQ(fedTage.size(), 4335u);
    EXPECT_EQ(fedTage.front().datum, tag(2015, 1, 1));
    EXPECT_EQ(fedTage.back().datum, tag(2026, 11, 13));
    const auto fedEntscheidungen = FredQuelle::leiteEntscheidungenAb(fedTage, fed);
    ASSERT_EQ(fedEntscheidungen.size(), 32u);
    EXPECT_EQ(fedEntscheidungen.back().datum, tag(2026, 9, 17));
    EXPECT_DOUBLE_EQ(fedEntscheidungen.back().leitzins, 4.0);
    // Die Senkungen aus den handgeschriebenen Testdaten gibt es wirklich (2024-09-19 und 2024-11-08)
    bool september = false;
    bool november = false;
    for (const Zinsentscheidung& e : fedEntscheidungen) {
        // Datum und Wert vergleichen
        september = september || (e.datum == tag(2024, 9, 19) && e.leitzins == 5.0);
        november = november || (e.datum == tag(2024, 11, 8) && e.leitzins == 4.75);
    }
    EXPECT_TRUE(september);
    EXPECT_TRUE(november);

    // EZB mit zwei Ländern
    const Zentralbank ezb("EZB", {"Deutschland", "Frankreich"}, "EUR", "ECBDFR");
    const auto ezbTage = FredQuelle::leseBeobachtungen(fixture("aufgezeichnet_ECBDFR.json"), "ECBDFR");
    EXPECT_DOUBLE_EQ(ezbTage.front().wert, -0.20);
    const auto ezbEntscheidungen = FredQuelle::leiteEntscheidungenAb(ezbTage, ezb);
    // 23 Änderungen x 2 Länder
    ASSERT_EQ(ezbEntscheidungen.size(), 46u);
    EXPECT_EQ(ezbEntscheidungen.back().datum, tag(2026, 9, 16));
    EXPECT_DOUBLE_EQ(ezbEntscheidungen.back().leitzins, 2.5);
}

// Echte Daten von Anfang bis Ende: Fed-Zinsdaten (FRED) + IBM-Kurse (Alpha Vantage), beide aufgezeichnet.
//   Änderung auf 4.00 % wirksam Do 2026-09-17 -> Reaktionstag Fr 2026-09-18
//   IBM Schlusskurs Do 237.75, Fr 229.55 -> Veränderung 229.55 / 237.75 - 1 = -3,45 %
//   Die IBM-Kurse reichen von 2026-06-25 bis 2026-11-13; ältere Fed-Entscheidungen werden übersprungen.
TEST(FredAufgezeichnet, EreignisstudieMitEchtenDaten) {
    // Fed-Entscheidungen aus der echten Antwort
    const auto entscheidungen = FredQuelle::leiteEntscheidungenAb(
        FredQuelle::leseBeobachtungen(fixture("aufgezeichnet_DFEDTARU.json"), "DFEDTARU"), fed);
    // IBM-Kurse aus der echten Alpha-Vantage-Antwort (Datei aufgezeichnet_IBM.json)
    DateiQuelle kurse(std::string(FIXTURES_DIR) + "/alphavantage");
    const Wertpapier ibm("aufgezeichnet_IBM", "IBM", "USA", "USD");
    const Kursreihe reihe = kurse.ladeKurse(ibm);
    // Ereignisstudie
    const auto ereignisse = Ereignisstudie(entscheidungen).auswerten(ibm, reihe);
    // Genau eine Fed-Änderung liegt im Zeitraum der Kurse
    ASSERT_EQ(ereignisse.size(), 1u);
    EXPECT_EQ(ereignisse[0].entscheidung.datum, tag(2026, 9, 17));
    EXPECT_EQ(ereignisse[0].reaktionstag, tag(2026, 9, 18));
    EXPECT_DOUBLE_EQ(ereignisse[0].kursVorher, 237.75);
    EXPECT_DOUBLE_EQ(ereignisse[0].kursNachher, 229.55);
    EXPECT_NEAR(ereignisse[0].veraenderung, -0.0345, 1e-4);
}

// Echte Daten für ein Euro-Land: EZB-Zinsdaten (FRED) + Mercedes-Benz-Kurse (Alpha Vantage, MBG.DEX), beide aufgezeichnet.
//   Die EZB ist laut Katalog für Deutschland zuständig (Länderliste), MBG.DEX hat land = "Deutschland".
//   Kurse 2026-06-29 bis 2026-11-13: die EZB-Änderung vom 2026-06-17 liegt davor und wird übersprungen.
//   Änderung auf 2,50 % wirksam Mi 2026-09-16 -> Reaktionstag Do 2026-09-17
//   MBG.DEX Schlusskurs Mi 45,365, Do 46,195 -> Veränderung 46,195 / 45,365 - 1 = +1,83 %
TEST(FredAufgezeichnet, EzbUndDeutscheAktie) {
    // EZB mit der Länderliste aus dem echten Katalog
    const Katalog katalog = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    ASSERT_EQ(katalog.zentralbanken().size(), 2u);
    const Zentralbank& ezb = katalog.zentralbanken()[1];
    ASSERT_EQ(ezb.name, "EZB");
    // Entscheidungen aus der echten FRED-Antwort (eine pro Euro-Land)
    const auto entscheidungen = FredQuelle::leiteEntscheidungenAb(
        FredQuelle::leseBeobachtungen(fixture("aufgezeichnet_ECBDFR.json"), "ECBDFR"), ezb);
    // Mercedes-Kurse aus der echten Alpha-Vantage-Antwort
    DateiQuelle kurse(std::string(FIXTURES_DIR) + "/alphavantage");
    const Wertpapier mbg("aufgezeichnet_MBG.DEX", "Mercedes-Benz Group AG", "Deutschland", "EUR");
    const Kursreihe reihe = kurse.ladeKurse(mbg);
    ASSERT_EQ(reihe.groesse(), 100u);
    // Ereignisstudie
    const auto ereignisse = Ereignisstudie(entscheidungen).auswerten(mbg, reihe);
    // Genau eine EZB-Änderung im Zeitraum, und zwar nur einmal (nicht einmal pro Euro-Land)
    ASSERT_EQ(ereignisse.size(), 1u);
    EXPECT_EQ(ereignisse[0].entscheidung.zentralbank, "EZB");
    EXPECT_EQ(ereignisse[0].entscheidung.datum, tag(2026, 9, 16));
    EXPECT_DOUBLE_EQ(ereignisse[0].entscheidung.leitzins, 2.5);
    EXPECT_EQ(ereignisse[0].reaktionstag, tag(2026, 9, 17));
    EXPECT_DOUBLE_EQ(ereignisse[0].kursVorher, 45.365);
    EXPECT_DOUBLE_EQ(ereignisse[0].kursNachher, 46.195);
    EXPECT_NEAR(ereignisse[0].veraenderung, 0.0183, 1e-4);
}
