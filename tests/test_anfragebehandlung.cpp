// Die Klasse, die hier getestet wird
#include "anfragebehandlung.hpp"
// Zum Vergleich: dieselben Berechnungen direkt über die Analyse-Klassen
#include "supportresistance.hpp"
#include "trendindikatoren.hpp"
// Fehlerklassen, um Fehler der Quelle zu simulieren
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>
// JSON-Bibliothek, um die Antworten zu lesen
#include <nlohmann/json.hpp>

// std::chrono für die Datumswerte
#include <chrono>
// std::sin für eine Testreihe mit Auf und Ab
#include <cmath>
// std::map
#include <map>
// std::logic_error für den Test mit einem internen Fehler
#include <stdexcept>
// std::string
#include <string>

// Kurzer Name für den JSON-Typ
using json = nlohmann::json;

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Testreihe mit 60 Tagen ab 2026-01-01: Kurs schwankt (Sinus) und steigt leicht, damit es Hochs und Tiefs gibt.
// High = Close + 1, Low = Close - 1, Open = Close: jede Kerze ist gültig (E1).
Kursreihe testreihe() {
    // Leere Reihe
    Kursreihe reihe;
    // Startdatum
    const std::chrono::sys_days start = std::chrono::year{2026} / 1 / 1;
    // 60 Tage
    for (int i = 0; i < 60; ++i) {
        // Schlusskurs: 100 plus Welle plus leichter Anstieg
        const double c = 100.0 + 10.0 * std::sin(i / 5.0) + 0.2 * i;
        // Kerze anhängen
        reihe.hinzufuegen(Kerze(start + std::chrono::days{i}, c, c + 1.0, c - 1.0, c, 1000.0 + i));
    }
    // Fertige Reihe
    return reihe;
}

// Fake-Kursquelle: liefert vorbereitete Daten oder simuliert Fehler, zählt die Aufrufe
class FakeQuelle : public IPriceSource {
public:
    // Was die Quelle für ein Symbol tun soll
    enum class Verhalten { Daten, Veraltet, Kontingent, Netzwerk, InternerFehler };

    // Verhalten für ein Symbol festlegen
    void setze(const std::string& symbol, Verhalten v) { verhalten_[symbol] = v; }

    // Anzahl der Ladevorgänge (um zu prüfen, dass bei ungültigen Parametern nichts geladen wird)
    int aufrufe() const { return aufrufe_; }

    // Einfache Variante: über ladeKursdaten
    Kursreihe ladeKurse(const Wertpapier& wp) override { return ladeKursdaten(wp).reihe; }

    // Liefert Daten oder wirft je nach festgelegtem Verhalten
    Kursdaten ladeKursdaten(const Wertpapier& wp) override {
        // Aufruf zählen
        ++aufrufe_;
        // Verhalten suchen (Standard: normale Daten)
        const auto it = verhalten_.find(wp.symbol);
        const Verhalten v = it == verhalten_.end() ? Verhalten::Daten : it->second;
        // Je nach Verhalten
        switch (v) {
            // Normale Daten
            case Verhalten::Daten:
                return Kursdaten{testreihe()};
            // Daten aus einem abgelaufenen Cache-Eintrag
            case Verhalten::Veraltet:
                return Kursdaten{testreihe(), true, "Stale data from 2026-03-01 10:00 UTC (network error)"};
            // Kontingent erschöpft
            case Verhalten::Kontingent:
                throw KontingentFehler("quota exhausted");
            // Netzwerkfehler
            case Verhalten::Netzwerk:
                throw NetzwerkFehler("network down");
            // Programmierfehler
            case Verhalten::InternerFehler:
                throw std::logic_error("geheimes internes Detail");
        }
        // Wird nicht erreicht
        return Kursdaten{};
    }

private:
    // Festgelegtes Verhalten pro Symbol
    std::map<std::string, Verhalten> verhalten_;
    // Zähler der Aufrufe
    int aufrufe_ = 0;
};

// Testfixture: Katalog mit drei Wertpapieren, Fake-Quelle und zwei Fed-Entscheidungen
class ApiTest : public ::testing::Test {
protected:
    // Fake-Quelle
    FakeQuelle quelle;
    // Die zu testende Anfragebehandlung
    Anfragebehandlung api{
        {Wertpapier("IBM", "International Business Machines", "USA", "USD"),
         Wertpapier("AAPL", "Apple Inc.", "USA", "USD"), Wertpapier("7203.T", "Toyota Motor", "Japan", "JPY")},
        quelle,
        {Zinsentscheidung("Fed", "USA", std::chrono::year{2026} / 1 / 10, 4.25),
         Zinsentscheidung("Fed", "USA", std::chrono::year{2026} / 2 / 5, 4.0)}};

    // Kurzschreibweise: GET-Anfrage stellen
    Antwort get(const std::string& pfad, std::map<std::string, std::string> parameter = {}) {
        // Anfrage zusammenbauen und behandeln
        return api.behandle(Anfrage{"GET", pfad, std::move(parameter)});
    }
};

// Prüft eine Fehlerantwort: Status, JSON mit "fehler" und "status", Text in der Meldung
void erwarteFehler(const Antwort& a, int status, const std::string& textteil) {
    // Status
    EXPECT_EQ(a.status, status) << a.body;
    // Inhalt ist JSON
    const json j = json::parse(a.body);
    // Feld "status" stimmt
    EXPECT_EQ(j["status"], status);
    // Meldung enthält den erwarteten Text
    EXPECT_NE(j["fehler"].get<std::string>().find(textteil), std::string::npos) << a.body;
}

}  // namespace

// ---------------------------------------------------------------- Allgemein

// Unbekannter Pfad: 404 mit JSON-Fehlermeldung
TEST_F(ApiTest, UnbekannterPfad) {
    // Pfad gibt es nicht
    erwarteFehler(get("/api/gibtsnicht"), 404, "unknown path");
    // Auch die Wurzel ist (noch) kein API-Pfad
    erwarteFehler(get("/"), 404, "unknown path");
}

// Andere Methode als GET: 405
TEST_F(ApiTest, NurGetErlaubt) {
    // POST auf einen bekannten Pfad
    erwarteFehler(api.behandle(Anfrage{"POST", "/api/wertpapiere", {}}), 405, "GET");
}

// Jede Antwort ist JSON mit dem passenden Inhaltstyp
TEST_F(ApiTest, InhaltstypIstJson) {
    // Erfolgreiche und fehlerhafte Antwort
    EXPECT_EQ(get("/api/wertpapiere").inhaltstyp, "application/json; charset=utf-8");
    EXPECT_EQ(get("/api/gibtsnicht").inhaltstyp, "application/json; charset=utf-8");
}

// ---------------------------------------------------------------- /api/wertpapiere

// Liste aller Wertpapiere mit allen Feldern
TEST_F(ApiTest, Wertpapierliste) {
    // Anfrage
    const Antwort a = get("/api/wertpapiere");
    // 200
    ASSERT_EQ(a.status, 200);
    // JSON lesen
    const json j = json::parse(a.body);
    // Drei Einträge in Katalogreihenfolge
    ASSERT_EQ(j["wertpapiere"].size(), 3u);
    EXPECT_EQ(j["wertpapiere"][0]["symbol"], "IBM");
    EXPECT_EQ(j["wertpapiere"][2]["land"], "Japan");
    EXPECT_EQ(j["wertpapiere"][2]["waehrung"], "JPY");
    // Dafür muss nichts geladen werden
    EXPECT_EQ(quelle.aufrufe(), 0);
    // Parameter sind hier nicht erlaubt
    erwarteFehler(get("/api/wertpapiere", {{"x", "1"}}), 400, "unknown parameter 'x'");
}

// ---------------------------------------------------------------- /api/kurse

// Alle Kerzen mit Datum und Preisen, nicht veraltet
TEST_F(ApiTest, Kurse) {
    // Anfrage
    const Antwort a = get("/api/kurse", {{"symbol", "IBM"}});
    // 200
    ASSERT_EQ(a.status, 200) << a.body;
    // JSON lesen
    const json j = json::parse(a.body);
    // Kopf
    EXPECT_EQ(j["symbol"], "IBM");
    EXPECT_EQ(j["waehrung"], "USD");
    EXPECT_EQ(j["veraltet"], false);
    EXPECT_TRUE(j["hinweis"].is_null());
    // 60 Kerzen
    ASSERT_EQ(j["kerzen"].size(), 60u);
    // Erste Kerze: Datum und Preise wie in der Testreihe
    const Kursreihe r = testreihe();
    EXPECT_EQ(j["kerzen"][0]["datum"], "2026-01-01");
    EXPECT_DOUBLE_EQ(j["kerzen"][0]["close"].get<double>(), r[0].close);
    EXPECT_DOUBLE_EQ(j["kerzen"][0]["high"].get<double>(), r[0].high);
    EXPECT_EQ(j["kerzen"][59]["datum"], "2026-03-01");
}

// Zeitraum von/bis: beide Grenzen eingeschlossen
TEST_F(ApiTest, KurseImZeitraum) {
    // 10. bis 19. Januar = 10 Tage
    const json j = json::parse(get("/api/kurse", {{"symbol", "IBM"}, {"von", "2026-01-10"}, {"bis", "2026-01-19"}}).body);
    // 10 Kerzen
    ASSERT_EQ(j["kerzen"].size(), 10u);
    // Erste und letzte
    EXPECT_EQ(j["kerzen"][0]["datum"], "2026-01-10");
    EXPECT_EQ(j["kerzen"][9]["datum"], "2026-01-19");
    // Nur "von": ab dem 1. März bleibt ein Tag
    EXPECT_EQ(json::parse(get("/api/kurse", {{"symbol", "IBM"}, {"von", "2026-03-01"}}).body)["kerzen"].size(), 1u);
    // Zeitraum ohne Daten: leere Liste, kein Fehler
    EXPECT_EQ(json::parse(get("/api/kurse", {{"symbol", "IBM"}, {"von", "2027-01-01"}}).body)["kerzen"].size(), 0u);
}

// Parameterfehler bei /api/kurse: 400 und nichts wird geladen
TEST_F(ApiTest, KurseParameterfehler) {
    // Symbol fehlt
    erwarteFehler(get("/api/kurse"), 400, "symbol");
    // Ungültiges Datum (Monat 13)
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}, {"von", "2026-13-01"}}), 400, "von");
    // Falsches Format
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}, {"bis", "1.1.2026"}}), 400, "bis");
    // von nach bis
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}, {"von", "2026-02-01"}, {"bis", "2026-01-01"}}), 400, "after");
    // Unbekannter Parameter (Tippfehler)
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}, {"vonn", "2026-01-01"}}), 400, "vonn");
    // Bei keinem dieser Fehler wurde etwas geladen
    EXPECT_EQ(quelle.aufrufe(), 0);
}

// Unbekanntes Symbol: 404, ohne die Quelle zu fragen (spart das Kontingent)
TEST_F(ApiTest, UnbekanntesSymbol) {
    // Symbol nicht im Katalog
    erwarteFehler(get("/api/kurse", {{"symbol", "MSFT"}}), 404, "MSFT");
    erwarteFehler(get("/api/indikatoren", {{"symbol", "MSFT"}}), 404, "MSFT");
    erwarteFehler(get("/api/ereignisstudie", {{"symbol", "MSFT"}}), 404, "MSFT");
    // Nichts geladen
    EXPECT_EQ(quelle.aufrufe(), 0);
}

// Veraltete Daten aus dem Cache (E7): Markierung und Hinweis erscheinen im JSON
TEST_F(ApiTest, VeralteteDatenMitHinweis) {
    // Quelle liefert für IBM veraltete Daten
    quelle.setze("IBM", FakeQuelle::Verhalten::Veraltet);
    // Anfrage
    const json j = json::parse(get("/api/kurse", {{"symbol", "IBM"}}).body);
    // Markierung und Hinweis
    EXPECT_EQ(j["veraltet"], true);
    EXPECT_NE(j["hinweis"].get<std::string>().find("Stale data"), std::string::npos);
    // Auch bei den Indikatoren
    EXPECT_EQ(json::parse(get("/api/indikatoren", {{"symbol", "IBM"}}).body)["veraltet"], true);
}

// Fehler der Quelle: Kontingent -> 503, Netzwerk -> 502, interner Fehler -> 500 ohne Details
TEST_F(ApiTest, FehlerDerQuelle) {
    // Kontingent erschöpft
    quelle.setze("IBM", FakeQuelle::Verhalten::Kontingent);
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}}), 503, "quota");
    // Netzwerk weg
    quelle.setze("IBM", FakeQuelle::Verhalten::Netzwerk);
    erwarteFehler(get("/api/kurse", {{"symbol", "IBM"}}), 502, "network");
    // Programmierfehler: 500, und die interne Meldung wird nicht nach außen gegeben
    quelle.setze("IBM", FakeQuelle::Verhalten::InternerFehler);
    const Antwort a = get("/api/kurse", {{"symbol", "IBM"}});
    erwarteFehler(a, 500, "internal server error");
    EXPECT_EQ(a.body.find("geheim"), std::string::npos);
    // Andere Wertpapiere funktionieren weiter
    EXPECT_EQ(get("/api/kurse", {{"symbol", "AAPL"}}).status, 200);
}

// ---------------------------------------------------------------- /api/indikatoren

// Alle Indikatoren mit Standardperioden; Werte stimmen mit den Analyse-Klassen überein
TEST_F(ApiTest, IndikatorenMitStandardwerten) {
    // Anfrage
    const Antwort a = get("/api/indikatoren", {{"symbol", "IBM"}});
    // 200
    ASSERT_EQ(a.status, 200) << a.body;
    // JSON lesen
    const json j = json::parse(a.body);
    // Verwendete Standardparameter
    EXPECT_EQ(j["parameter"]["sma"], 20);
    EXPECT_EQ(j["parameter"]["rsi"], 14);
    EXPECT_EQ(j["parameter"]["macd"], json({12, 26, 9}));
    // Zeitachse und alle Reihen haben einen Eintrag pro Tag
    ASSERT_EQ(j["daten"].size(), 60u);
    for (const char* feld : {"sma", "ema", "rsi", "atr"}) {
        // Gleiche Länge wie die Zeitachse
        EXPECT_EQ(j[feld].size(), 60u) << feld;
    }
    EXPECT_EQ(j["macd"]["histogramm"].size(), 60u);
    EXPECT_EQ(j["bollinger"]["oben"].size(), 60u);
    EXPECT_EQ(j["renditen"]["log"].size(), 60u);
    // Einschwingphase als null: SMA(20) erst ab Index 19
    EXPECT_TRUE(j["sma"][18].is_null());
    ASSERT_FALSE(j["sma"][19].is_null());
    // Gleiche Werte wie bei direkter Berechnung (die API rechnet nichts anders, sie liefert nur aus)
    const Trendindikatoren t(testreihe());
    EXPECT_DOUBLE_EQ(j["sma"][19].get<double>(), *t.sma(20)[19]);
    EXPECT_DOUBLE_EQ(j["rsi"][59].get<double>(), *t.rsi(14)[59]);
    EXPECT_DOUBLE_EQ(j["macd"]["signal"][59].get<double>(), *t.macd(12, 26, 9).signal[59]);
    // Einzelwerte vorhanden
    EXPECT_TRUE(j["volatilitaet"].is_number());
    EXPECT_TRUE(j["maxDrawdown"].is_number());
    // Pivot-Punkte aus der letzten Kerze
    const PivotPunkte p = SupportResistance(testreihe()).pivotPunkte();
    EXPECT_DOUBLE_EQ(j["pivot"]["p"].get<double>(), p.p);
    // Die wellenförmige Reihe hat Niveaus; jedes ist support oder resistance
    ASSERT_FALSE(j["niveaus"].empty());
    for (const json& n : j["niveaus"]) {
        // Art ist einer der beiden Werte
        EXPECT_TRUE(n["art"] == "support" || n["art"] == "resistance") << n;
    }
}

// Eigene Perioden werden verwendet
TEST_F(ApiTest, IndikatorenMitEigenenPerioden) {
    // SMA 5, MACD 3,6,2, Bollinger 10 mit k = 1.5
    const json j = json::parse(
        get("/api/indikatoren", {{"symbol", "IBM"}, {"sma", "5"}, {"macd", "3,6,2"}, {"bollinger", "10,1.5"}}).body);
    // Parameter übernommen
    EXPECT_EQ(j["parameter"]["sma"], 5);
    EXPECT_EQ(j["parameter"]["macd"], json({3, 6, 2}));
    EXPECT_DOUBLE_EQ(j["parameter"]["bollinger"][1].get<double>(), 1.5);
    // SMA(5) schon ab Index 4
    EXPECT_FALSE(j["sma"][4].is_null());
    EXPECT_DOUBLE_EQ(j["sma"][4].get<double>(), *Trendindikatoren(testreihe()).sma(5)[4]);
}

// Ungültige Perioden: 400 mit Name des Parameters, und es wird nichts geladen
TEST_F(ApiTest, IndikatorenParameterfehler) {
    // Hilfsfunktion: Anfrage mit einem zusätzlichen Parameter
    auto mit = [this](const std::string& name, const std::string& wert) {
        // Anfrage für IBM mit dem Parameter
        return get("/api/indikatoren", {{"symbol", "IBM"}, {name, wert}});
    };
    // Periode 0, negativ, keine Zahl, Kommazahl
    erwarteFehler(mit("sma", "0"), 400, "sma");
    erwarteFehler(mit("rsi", "-3"), 400, "rsi");
    erwarteFehler(mit("ema", "abc"), 400, "ema");
    erwarteFehler(mit("atr", "2.5"), 400, "atr");
    // MACD: falsche Anzahl, schnell >= langsam, keine Zahl
    erwarteFehler(mit("macd", "12,26"), 400, "macd");
    erwarteFehler(mit("macd", "26,12,9"), 400, "fast");
    erwarteFehler(mit("macd", "12,x,9"), 400, "macd");
    // Bollinger: falsche Anzahl, negativer Faktor
    erwarteFehler(mit("bollinger", "20"), 400, "bollinger");
    erwarteFehler(mit("bollinger", "20,-1"), 400, "bollinger");
    // Fenster und Toleranz
    erwarteFehler(mit("fenster", "0"), 400, "fenster");
    erwarteFehler(mit("toleranz", "-0.1"), 400, "toleranz");
    erwarteFehler(mit("toleranz", "nan"), 400, "toleranz");
    // Unbekannter Parameter
    erwarteFehler(mit("smaa", "20"), 400, "smaa");
    // Nichts wurde geladen
    EXPECT_EQ(quelle.aufrufe(), 0);
}

// Gültige Periode, aber zu wenige Daten: 422 mit der Meldung aus E3
TEST_F(ApiTest, IndikatorenZuWenigDaten) {
    // SMA 100 bei 60 Tagen
    erwarteFehler(get("/api/indikatoren", {{"symbol", "IBM"}, {"sma", "100"}}), 422, "too short");
    // Zeitraum mit nur 10 Tagen: MACD(12,26,9) braucht 34
    erwarteFehler(get("/api/indikatoren", {{"symbol", "IBM"}, {"von", "2026-02-20"}}), 422, "MACD");
}

// ---------------------------------------------------------------- /api/ereignisstudie

// US-Aktie: beide Fed-Entscheidungen; japanische Aktie: leere Liste statt Fehler
TEST_F(ApiTest, Ereignisstudie) {
    // US-Aktie
    const Antwort a = get("/api/ereignisstudie", {{"symbol", "IBM"}});
    // 200
    ASSERT_EQ(a.status, 200) << a.body;
    // JSON lesen
    const json j = json::parse(a.body);
    // Zwei Ereignisse
    ASSERT_EQ(j["ereignisse"].size(), 2u);
    // Erstes: Entscheidung 10. Januar, Reaktion 11. Januar (die Testreihe hat jeden Kalendertag)
    EXPECT_EQ(j["ereignisse"][0]["zentralbank"], "Fed");
    EXPECT_EQ(j["ereignisse"][0]["datum"], "2026-01-10");
    EXPECT_EQ(j["ereignisse"][0]["reaktionstag"], "2026-01-11");
    // Veränderung = nachher / vorher - 1, aus den Kursen der Testreihe (Index 9 und 10)
    const Kursreihe r = testreihe();
    EXPECT_DOUBLE_EQ(j["ereignisse"][0]["veraenderung"].get<double>(), r[10].close / r[9].close - 1.0);
    // Japanische Aktie: keine passende Zentralbank
    const json jp = json::parse(get("/api/ereignisstudie", {{"symbol", "7203.T"}}).body);
    EXPECT_TRUE(jp["ereignisse"].empty());
    // Zeitraum, der nur die zweite Entscheidung enthält
    const json feb = json::parse(get("/api/ereignisstudie", {{"symbol", "IBM"}, {"von", "2026-02-01"}}).body);
    ASSERT_EQ(feb["ereignisse"].size(), 1u);
    EXPECT_EQ(feb["ereignisse"][0]["datum"], "2026-02-05");
}
