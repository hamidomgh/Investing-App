// Die Teile, die hier getestet werden
#include "alphavantage_quelle.hpp"
// Offline-Kursquelle für den Test der Dateien in daten/
#include "dateiquelle.hpp"
#include "katalog.hpp"
#include "kursabruf.hpp"
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
// std::stringstream
#include <sstream>
// std::logic_error für den Test mit einem Programmierfehler
#include <stdexcept>
// std::string
#include <string>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Liest eine Datei vollständig in einen String; wirft, wenn sie fehlt
std::string liesDatei(const std::string& pfad) {
    // Datei öffnen
    std::ifstream datei(pfad);
    // Fehlt sie, soll der Test deutlich scheitern
    if (!datei) {
        // Fehler mit Pfad
        throw std::runtime_error("file not found: " + pfad);
    }
    // Ganzen Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Als String zurückgeben
    return puffer.str();
}

// Pfad einer Testdatei in tests/fixtures/
std::string fixturePfad(const std::string& name) {
    // FIXTURES_DIR setzt CMake beim Kompilieren
    return std::string(FIXTURES_DIR) + "/" + name;
}

// Variable für den API-Key in diesen Tests
const char* const keyVariable = "MARKETLENS_TEST_AV_KEY";

}  // namespace

// ---------------------------------------------------------------- Katalog

// Der echte Katalog im Repository ist fehlerfrei und enthält Wertpapiere aus mehreren Ländern
TEST(Katalog, EchterKatalogIstFehlerfrei) {
    // config/katalog.json lesen; KONFIG_DIR setzt CMake
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // Kein fehlerhafter Eintrag
    EXPECT_TRUE(k.fehler().empty());
    // Sechs Wertpapiere
    EXPECT_EQ(k.wertpapiere().size(), 6u);
    // Stichprobe: ein deutsches Wertpapier mit Land und Währung
    const Wertpapier* mbg = k.finde("MBG.DEX");
    ASSERT_NE(mbg, nullptr);
    EXPECT_EQ(mbg->land, "Deutschland");
    EXPECT_EQ(mbg->waehrung, "EUR");
}

// Gemischter Katalog: gültige Einträge bleiben, fehlerhafte werden mit Position und Grund gemeldet.
//   Position 0 IBM           gültig
//   Position 1 OHNELAND      Feld "land" fehlt
//   Position 2 "kein Objekt" ist kein Objekt
//   Position 3 MBG.DEX       gültig
//   Position 4 LEER          leerer Name (vom Wertpapier-Konstruktor aus E1 abgelehnt)
//   Position 5 42            Symbol ist eine Zahl statt Text
//   Position 6 IBM           doppeltes Symbol (der erste Eintrag gilt)
//   Position 7 7203.T        gültig (neues Land Japan, ohne Codeänderung)
TEST(Katalog, FehlerhafteEintraegeVerhindernNichtDieUebrigen) {
    // Gemischten Katalog lesen
    const Katalog k = Katalog::ausDatei(fixturePfad("katalog/gemischt.json"));
    // Drei gültige Wertpapiere, in der Reihenfolge der Datei
    ASSERT_EQ(k.wertpapiere().size(), 3u);
    EXPECT_EQ(k.wertpapiere()[0].symbol, "IBM");
    EXPECT_EQ(k.wertpapiere()[1].symbol, "MBG.DEX");
    EXPECT_EQ(k.wertpapiere()[2].symbol, "7203.T");
    // Der erste IBM-Eintrag gilt, nicht der doppelte
    EXPECT_EQ(k.finde("IBM")->name, "International Business Machines");
    // Fünf Fehler
    ASSERT_EQ(k.fehler().size(), 5u);
    // Positionen der Fehler
    EXPECT_EQ(k.fehler()[0].position, 1u);
    EXPECT_EQ(k.fehler()[1].position, 2u);
    EXPECT_EQ(k.fehler()[2].position, 4u);
    EXPECT_EQ(k.fehler()[3].position, 5u);
    EXPECT_EQ(k.fehler()[4].position, 6u);
    // Fehlendes Feld: Symbol bekannt, Meldung nennt das Feld
    EXPECT_EQ(k.fehler()[0].symbol, "OHNELAND");
    EXPECT_NE(k.fehler()[0].meldung.find("land"), std::string::npos) << k.fehler()[0].meldung;
    // Kein Objekt: Symbol unbekannt
    EXPECT_EQ(k.fehler()[1].symbol, "");
    // Leerer Name: Meldung aus E1
    EXPECT_NE(k.fehler()[2].meldung.find("name"), std::string::npos) << k.fehler()[2].meldung;
    // Zahl statt Text
    EXPECT_NE(k.fehler()[3].meldung.find("string"), std::string::npos) << k.fehler()[3].meldung;
    // Doppeltes Symbol
    EXPECT_EQ(k.fehler()[4].symbol, "IBM");
    EXPECT_NE(k.fehler()[4].meldung.find("duplicate"), std::string::npos) << k.fehler()[4].meldung;
}

// Ein neues Land wird nur durch einen Eintrag ergänzt: kein Code kennt eine feste Liste von Ländern
TEST(Katalog, NeuesLandOhneCodeaenderung) {
    // Katalog mit einem Land, das sonst nirgends vorkommt
    const Katalog k = Katalog::ausJson(R"json({"wertpapiere": [
        {"symbol": "ABC.XYZ", "name": "Beispiel", "land": "Neuseeland", "waehrung": "NZD"}]})json");
    // Wird ohne Fehler übernommen
    ASSERT_EQ(k.wertpapiere().size(), 1u);
    EXPECT_TRUE(k.fehler().empty());
    EXPECT_EQ(k.wertpapiere()[0].land, "Neuseeland");
}

// Ein ganz unbrauchbarer Katalog wird als Ganzes abgelehnt
TEST(Katalog, UnbrauchbarerKatalog) {
    // Kein gültiges JSON
    EXPECT_THROW(Katalog::ausJson("{ nicht json"), ParseFehler);
    // Feld "wertpapiere" fehlt
    EXPECT_THROW(Katalog::ausJson(R"({"aktien": []})"), ParseFehler);
    // "wertpapiere" ist keine Liste
    EXPECT_THROW(Katalog::ausJson(R"({"wertpapiere": {}})"), ParseFehler);
    // Oberste Ebene ist eine Liste statt eines Objekts
    EXPECT_THROW(Katalog::ausJson("[]"), ParseFehler);
    // Datei existiert nicht
    EXPECT_THROW(Katalog::ausDatei(fixturePfad("katalog/gibt_es_nicht.json")), KonfigurationsFehler);
}

// Leere Liste ist erlaubt; finde liefert nullptr für unbekannte Symbole
TEST(Katalog, LeererKatalogUndUnbekanntesSymbol) {
    // Katalog ohne Einträge
    const Katalog k = Katalog::ausJson(R"({"wertpapiere": []})");
    // Keine Wertpapiere, keine Fehler
    EXPECT_TRUE(k.wertpapiere().empty());
    EXPECT_TRUE(k.fehler().empty());
    // Unbekanntes Symbol
    EXPECT_EQ(k.finde("IBM"), nullptr);
}

// ---------------------------------------------------------------- Laden aller Kurse

// Ein Wertpapier lädt, vier scheitern aus verschiedenen Gründen; alle werden versucht, keins bricht ab
TEST(Kursabruf, FehlerEinesWertpapiersVerhindertNichtDieUebrigen) {
    // Test-Key setzen
    setenv(keyVariable, "testkey", 1);
    // Fake-Client und Quelle
    FakeHttpClient fake;
    AlphaVantageQuelle quelle(fake, keyVariable);
    // Fünf Wertpapiere aus dem echten Katalog
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // IBM: gültige Antwort
    fake.setzeAntwort(quelle.url("IBM"), 200, liesDatei(fixturePfad("alphavantage/daily_ok.json")));
    // AAPL: unbekanntes Symbol (Error Message)
    fake.setzeAntwort(quelle.url("AAPL"), 200, liesDatei(fixturePfad("alphavantage/error_message.json")));
    // MBG.DEX: Netzwerkausfall
    fake.setzeAusfall(quelle.url("MBG.DEX"), FakeHttpClient::Ausfall::Netzwerk);
    // TSCO.LON: Kontingent erschöpft
    fake.setzeAntwort(quelle.url("TSCO.LON"), 200, liesDatei(fixturePfad("alphavantage/note_kontingent.json")));
    // SHOP.TRT: ungültiges JSON
    fake.setzeAntwort(quelle.url("SHOP.TRT"), 200, "kein json");
    // RELIANCE.BSE: gültige Antwort (dieselbe Testdatei, es geht nur um den Ablauf)
    fake.setzeAntwort(quelle.url("RELIANCE.BSE"), 200, liesDatei(fixturePfad("alphavantage/daily_ok.json")));

    // Alle laden
    const Ladeergebnis e = ladeAlleKurse(quelle, k.wertpapiere());

    // Alle sechs wurden angefragt, auch nach den Fehlern
    EXPECT_EQ(fake.anfragen().size(), 6u);
    // Zwei erfolgreich, in der Reihenfolge des Katalogs
    ASSERT_EQ(e.erfolgreich.size(), 2u);
    EXPECT_EQ(e.erfolgreich[0].wertpapier.symbol, "IBM");
    EXPECT_EQ(e.erfolgreich[0].kurse.groesse(), 3u);
    EXPECT_EQ(e.erfolgreich[1].wertpapier.symbol, "RELIANCE.BSE");
    // Vier Fehler, jeweils mit Symbol und Meldung
    ASSERT_EQ(e.fehler.size(), 4u);
    EXPECT_EQ(e.fehler[0].symbol, "AAPL");
    EXPECT_EQ(e.fehler[1].symbol, "MBG.DEX");
    EXPECT_EQ(e.fehler[2].symbol, "TSCO.LON");
    EXPECT_EQ(e.fehler[3].symbol, "SHOP.TRT");
    // Meldungen sind nicht leer und verständlich
    EXPECT_NE(e.fehler[2].meldung.find("quota"), std::string::npos) << e.fehler[2].meldung;
    // Aufräumen
    unsetenv(keyVariable);
}

// Programmierfehler (keine QuellenFehler) werden nicht verschluckt, sondern weitergegeben
TEST(Kursabruf, AndereFehlerWerdenNichtVerschluckt) {
    // Quelle, die einen Programmierfehler simuliert
    class KaputteQuelle : public IPriceSource {
    public:
        // Wirft einen logic_error statt eines QuellenFehlers
        Kursreihe ladeKurse(const Wertpapier&) override { throw std::logic_error("bug"); }
    };
    // Quelle anlegen
    KaputteQuelle quelle;
    // Ein Wertpapier
    const std::vector<Wertpapier> liste{Wertpapier("IBM", "IBM", "USA", "USD")};
    // Der Fehler kommt beim Aufrufer an
    EXPECT_THROW(ladeAlleKurse(quelle, liste), std::logic_error);
}

// Leere Liste: leeres Ergebnis, keine Anfrage
TEST(Kursabruf, LeereListe) {
    // Quelle, die nie aufgerufen werden darf
    class NieAufrufen : public IPriceSource {
    public:
        // Würde den Test scheitern lassen
        Kursreihe ladeKurse(const Wertpapier&) override {
            // Darf nicht passieren
            ADD_FAILURE() << "ladeKurse called";
            // Leere Reihe zurückgeben
            return Kursreihe();
        }
    };
    // Quelle anlegen
    NieAufrufen quelle;
    // Laden ohne Wertpapiere
    const Ladeergebnis e = ladeAlleKurse(quelle, {});
    // Beide Listen leer
    EXPECT_TRUE(e.erfolgreich.empty());
    EXPECT_TRUE(e.fehler.empty());
}

// ---------------------------------------------------------------- Offline-Daten im Repository

// Für jedes Wertpapier des echten Katalogs liegt in daten/ eine gültige Datei, die sich vollständig laden lässt
// (gleiche Prüfungen wie eine echte Antwort, inklusive der Kerzen-Validierung aus E1).
// Bewusst ohne feste Kurswerte, damit der Test auch nach einer neuen Aufnahme der Dateien gilt.
TEST(OfflineDaten, JedesWertpapierDesKatalogsIstLadbar) {
    // Echter Katalog
    const Katalog k = Katalog::ausDatei(std::string(KONFIG_DIR) + "/katalog.json");
    // Quelle auf den Ordner von Alpha Vantage (DATEN_DIR setzt CMake; seit F3 je Anbieter ein Unterordner)
    DateiQuelle quelle(std::string(DATEN_DIR) + "/alphavantage");
    // Alpha Vantage liefert alle 6 Wertpapiere des Katalogs
    const std::vector<Wertpapier> liste = k.fuerAnbieter("alphavantage");
    EXPECT_EQ(liste.size(), 6u);
    // Jedes Wertpapier laden
    for (const Wertpapier& wp : liste) {
        // Hier landet die Reihe
        Kursreihe reihe;
        // Darf keinen Fehler werfen
        ASSERT_NO_THROW(reihe = quelle.ladeKurse(wp)) << wp.symbol;
        // Und muss Kerzen enthalten (mindestens 34 für MACD 12,26,9 mit den Standardwerten der Schnittstelle)
        EXPECT_GE(reihe.groesse(), 34u) << wp.symbol;
    }
}
