// Die Klasse, die hier getestet wird
#include "live_kursquelle.hpp"
// Für die erwarteten URLs der beiden Anbieter
#include "alphavantage_quelle.hpp"
#include "twelvedata_quelle.hpp"
// Fehlerklassen
#include "quellenfehler.hpp"
// Fake-Netz und Fake-Uhr: kein Internet, Zeit frei einstellbar
#include "fake_http_client.hpp"
#include "fake_uhr.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// setenv und unsetenv für die API-Keys im Test
#include <cstdlib>
// std::chrono
#include <chrono>
// std::filesystem für den Cache-Ordner
#include <filesystem>
// std::ifstream zum Lesen der Testdaten
#include <fstream>
// std::stringstream
#include <sstream>
// std::string
#include <string>
// getpid für einen eindeutigen Ordnernamen
#include <unistd.h>

// Anonymer Namespace: Hilfen nur in dieser Datei sichtbar
namespace {

// Liest eine Datei aus tests/fixtures/
std::string liesFixture(const std::string& pfad) {
    // Datei öffnen
    std::ifstream datei(std::string(FIXTURES_DIR) + "/" + pfad);
    // Ganzen Inhalt einlesen
    std::stringstream inhalt;
    inhalt << datei.rdbuf();
    return inhalt.str();
}

// IBM wird von beiden Anbietern geführt (Katalog mit "symbole", F3)
const Wertpapier ibm("IBM", "International Business Machines", "USA", "USD",
                     {{"alphavantage", "IBM"}, {"twelvedata", "IBM"}});

// Gemeinsamer Aufbau: API-Keys setzen, eigener Cache-Ordner, Fake-Netz und Fake-Uhr
class LiveKursquelleTest : public ::testing::Test {
protected:
    // Vor jedem Test: Keys setzen, leeren Ordner vorbereiten
    void SetUp() override {
        // Testschlüssel; echte Schlüssel werden nie gebraucht
        setenv(AlphaVantageQuelle::standardVariable, "testkey", 1);
        setenv(TwelveDataQuelle::standardVariable, "testkey", 1);
        // Eigener Ordner je Testlauf (Prozessnummer), damit sich parallele Läufe nicht stören
        ordner = std::filesystem::temp_directory_path() / ("marketlens_livequelle_" + std::to_string(getpid()));
        std::filesystem::remove_all(ordner);
    }

    // Nach jedem Test: aufräumen
    void TearDown() override {
        // Keys entfernen und Ordner löschen
        unsetenv(AlphaVantageQuelle::standardVariable);
        unsetenv(TwelveDataQuelle::standardVariable);
        std::filesystem::remove_all(ordner);
    }

    // URL, die die Quelle des Anbieters für IBM abfragt (mit dem Testschlüssel)
    std::string urlAlphaVantage() {
        // Hilfsquelle nur für die URL; fragt nichts an
        FakeHttpClient leer;
        return AlphaVantageQuelle(leer).url("IBM");
    }
    std::string urlTwelveData() {
        // Hilfsquelle nur für die URL; fragt nichts an
        FakeHttpClient leer;
        return TwelveDataQuelle(leer).url("IBM");
    }

    // Cache-Ordner dieses Tests
    std::filesystem::path ordner;
    // Fake-Netz
    FakeHttpClient netz;
    // Fake-Uhr, fester Start
    FakeUhr uhr{std::chrono::system_clock::time_point{std::chrono::hours{24 * 20000}}};
};

}  // namespace

// Empfohlene Gültigkeit je Anbieter; unbekannter Anbieter ist ein Konfigurationsfehler
TEST(LiveKursquelleGueltigkeit, JeAnbieter) {
    EXPECT_EQ(LiveKursquelle::standardGueltigkeit("alphavantage"), std::chrono::hours{12});
    EXPECT_EQ(LiveKursquelle::standardGueltigkeit("twelvedata"), std::chrono::hours{1});
    EXPECT_THROW(LiveKursquelle::standardGueltigkeit("finnhub"), KonfigurationsFehler);
}

// Unbekannter Anbieter: Fehler, und es wird kein Cache-Ordner angelegt
TEST_F(LiveKursquelleTest, UnbekannterAnbieter) {
    EXPECT_THROW(LiveKursquelle("finnhub", netz, uhr, ordner, std::chrono::hours{1}), KonfigurationsFehler);
    EXPECT_FALSE(std::filesystem::exists(ordner));
}

// Fehlt der API-Key, scheitert schon der Start (nicht erst die erste Anfrage)
TEST_F(LiveKursquelleTest, FehlenderSchluessel) {
    unsetenv(TwelveDataQuelle::standardVariable);
    EXPECT_THROW(LiveKursquelle("twelvedata", netz, uhr, ordner, std::chrono::hours{1}), KonfigurationsFehler);
    // Kein Abruf versucht
    EXPECT_TRUE(netz.anfragen().empty());
}

// Alpha Vantage: erster Abruf übers Netz, danach aus dem Cache; nach Ablauf der Gültigkeit wieder übers Netz
TEST_F(LiveKursquelleTest, AlphaVantageAbrufBeiBedarf) {
    netz.setzeAntwort(urlAlphaVantage(), 200, liesFixture("alphavantage/daily_ok.json"));
    LiveKursquelle quelle("alphavantage", netz, uhr, ordner, std::chrono::hours{12});

    // Erster Abruf: eine Anfrage, gültige Kurse
    const Kursdaten erste = quelle.ladeKursdaten(ibm);
    EXPECT_EQ(netz.anfragen().size(), 1u);
    EXPECT_GT(erste.reihe.groesse(), 0u);
    EXPECT_FALSE(erste.veraltet);

    // 11 Stunden später: noch gültig, keine neue Anfrage
    uhr.vorstellen(std::chrono::hours{11});
    quelle.ladeKursdaten(ibm);
    EXPECT_EQ(netz.anfragen().size(), 1u);

    // 13 Stunden nach dem ersten Abruf: abgelaufen, neue Anfrage
    uhr.vorstellen(std::chrono::hours{2});
    quelle.ladeKursdaten(ibm);
    EXPECT_EQ(netz.anfragen().size(), 2u);
}

// Twelve Data: die Anfrage geht an Twelve Data, und die Antwort wird mit dem Parser von Twelve Data gelesen
TEST_F(LiveKursquelleTest, TwelveDataNutztEigeneQuelle) {
    netz.setzeAntwort(urlTwelveData(), 200, liesFixture("twelvedata/gueltig.json"));
    LiveKursquelle quelle("twelvedata", netz, uhr, ordner, std::chrono::hours{1});

    const Kursreihe reihe = quelle.ladeKurse(ibm);
    ASSERT_EQ(netz.anfragen().size(), 1u);
    EXPECT_NE(netz.anfragen()[0].find("api.twelvedata.com"), std::string::npos);
    EXPECT_GT(reihe.groesse(), 0u);
}

// Kontingent erschöpft (Alpha Vantage meldet das mit HTTP 200 und "Information"):
// Die Bewertung von Alpha Vantage greift, die alten Kurse kommen mit Hinweis, und sie werden nicht überschrieben
TEST_F(LiveKursquelleTest, KontingentErschoepftLiefertAlteDaten) {
    netz.setzeAntwort(urlAlphaVantage(), 200, liesFixture("alphavantage/daily_ok.json"));
    LiveKursquelle quelle("alphavantage", netz, uhr, ordner, std::chrono::hours{12});
    const Kursdaten frisch = quelle.ladeKursdaten(ibm);

    // Einen Tag später ist das Kontingent aufgebraucht
    uhr.vorstellen(std::chrono::hours{24});
    netz.setzeAntwort(urlAlphaVantage(), 200, liesFixture("alphavantage/information_kontingent.json"));
    const Kursdaten alt = quelle.ladeKursdaten(ibm);
    EXPECT_EQ(netz.anfragen().size(), 2u);
    EXPECT_TRUE(alt.veraltet);
    EXPECT_FALSE(alt.hinweis.empty());
    EXPECT_EQ(alt.reihe.groesse(), frisch.reihe.groesse());
}

// Netz weg und noch nie geladen: der Fehler kommt beim Aufrufer an (kein Absturz, keine leeren Daten)
TEST_F(LiveKursquelleTest, OhneNetzUndOhneCacheFehler) {
    netz.setzeAusfall(urlAlphaVantage(), FakeHttpClient::Ausfall::Netzwerk);
    LiveKursquelle quelle("alphavantage", netz, uhr, ordner, std::chrono::hours{12});
    EXPECT_THROW(quelle.ladeKursdaten(ibm), NetzwerkFehler);
}
