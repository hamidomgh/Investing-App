// Einbindung der Header-Datei für die Kursdaten-Klassen
#include "kursdaten.hpp"

// Einbindung des Google Test Frameworks für Unit-Tests
#include <gtest/gtest.h>

// Einbindung von Standardbibliothekskomponenten
#include <stdexcept>     // Für Exception-Klassen wie std::invalid_argument
#include <chrono>        // Für Zeitpunkte (Datum/Zeit)
#include <vector>        // Für std::vector

// Hilfsfunktion zum Erstellen eines Zeitpunkts aus Jahr, Monat, Tag
// Diese Funktion wird verwendet, um Zeitpunkte in den Tests zu erzeugen
std::chrono::system_clock::time_point make_time(int year, int month, int day) {
    // Erstellt eine Zeitstruktur (tm) und initialisiert sie
    std::tm t = {};
    // Konvertiert das Jahr in das erforderliche Format (Jahre seit 1900)
    t.tm_year = year - 1900;
    // Konvertiert den Monat (1-12) in das erforderliche Format (0-11)
    t.tm_mon = month - 1;
    // Setzt den Tag
    t.tm_mday = day;
    // Konvertiert die Zeitstruktur in einen Zeitpunkt (time_t) und wandelt ihn in system_clock::time_point um
    return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

// Test für die Kerze-Klasse - prüft, dass gültige Kerzen korrekt erstellt werden
TEST(Kerze, GueltigeKerze) {
    // Erstellt eine gültige Kerze mit einem Datum und Kurswerten
    Kerze k(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, 1000.0);
    // Ruft die Validierung auf (sollte keine Exception werfen)
    k.validieren();
}

// Test für die Kerze-Klasse - prüft, dass ungültige High/Low-Werte erkannt werden
TEST(Kerze, UngueltigeHighLow) {
    // Erstellt eine Kerze mit High < Low (ungültig) und prüft, dass eine Exception geworfen wird
    EXPECT_THROW((Kerze(make_time(2023, 1, 1), 100.0, 90.0, 110.0, 105.0, 1000.0).validieren()), std::invalid_argument);
}

// Test für die Kerze-Klasse - prüft, dass ungültige Open-Werte erkannt werden
TEST(Kerze, UngueltigeOpen) {
    // Erstellt eine Kerze mit Open außerhalb von Low..High (ungültig) und prüft, dass eine Exception geworfen wird
    EXPECT_THROW((Kerze(make_time(2023, 1, 1), 120.0, 110.0, 90.0, 105.0, 1000.0).validieren()), std::invalid_argument);
}

// Test für die Kerze-Klasse - prüft, dass ungültige Close-Werte erkannt werden
TEST(Kerze, UngueltigeClose) {
    // Erstellt eine Kerze mit Close außerhalb von Low..High (ungültig) und prüft, dass eine Exception geworfen wird
    EXPECT_THROW((Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 120.0, 1000.0).validieren()), std::invalid_argument);
}

// Test für die Kerze-Klasse - prüft, dass ungültiges Volumen erkannt wird
TEST(Kerze, UngueltigesVolumen) {
    // Erstellt eine Kerze mit negativem Volumen (ungültig) und prüft, dass eine Exception geworfen wird
    EXPECT_THROW((Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, -100.0).validieren()), std::invalid_argument);
}

// Test für die Kursreihe-Klasse - prüft, dass gültige Reihen korrekt erstellt werden
TEST(Kursreihe, GueltigeReihe) {
    // Erstellt einen Vector von Kerzen
    std::vector<Kerze> kerzen;
    // Fügt zwei gültige Kerzen hinzu
    kerzen.push_back(Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, 1000.0));
    kerzen.push_back(Kerze(make_time(2023, 1, 2), 105.0, 115.0, 95.0, 110.0, 1200.0));
    
    // Erstellt eine Kursreihe mit diesen Kerzen
    Kursreihe r(kerzen);
    // Ruft die Validierung auf (sollte keine Exception werfen)
    r.validieren();
}

// Test für die Kursreihe-Klasse - prüft, dass ungültige chronologische Reihenfolge erkannt wird
TEST(Kursreihe, UngueltigeChronologie) {
    // Erstellt einen Vector von Kerzen mit falscher Reihenfolge (späteres Datum vor früherem)
    std::vector<Kerze> kerzen;
    kerzen.push_back(Kerze(make_time(2023, 1, 2), 100.0, 110.0, 90.0, 105.0, 1000.0));
    kerzen.push_back(Kerze(make_time(2023, 1, 1), 105.0, 115.0, 95.0, 110.0, 1200.0));  // früheres Datum
    
    // Prüft, dass beim Erstellen der Kursreihe eine Exception geworfen wird
    EXPECT_THROW(Kursreihe r(kerzen), std::invalid_argument);
}

// Test für die Kursreihe-Klasse - prüft, dass ungültige Kerzen in einer Reihe erkannt werden
TEST(Kursreihe, UngueltigeKerzeInReihe) {
    // Eine ungültige Kerze (Open außerhalb von Low..High) kann gar nicht erst existieren:
    // Der Konstruktor von Kerze lehnt sie ab, also kommt sie nie in eine Kursreihe.
    EXPECT_THROW(Kerze(make_time(2023, 1, 2), 120.0, 115.0, 95.0, 110.0, 1200.0), std::invalid_argument);
}

// Test für die Kursreihe-Klasse - prüft, dass leere Reihen korrekt funktionieren
TEST(Kursreihe, LeereReihe) {
    // Erstellt eine leere Kursreihe
    Kursreihe r;
    // Prüft, dass die Größe 0 ist
    EXPECT_EQ(r.groesse(), 0u);
    // Ruft die Validierung auf (sollte keine Exception werfen)
    r.validieren();  // Sollte keine Ausnahme werfen
}

// Test für die Kursreihe-Klasse - prüft das Hinzufügen von Kerzen und Zugriff
TEST(Kursreihe, HinzufuegenKerze) {
    // Erstellt eine leere Kursreihe
    Kursreihe r;
    // Fügt eine Kerze hinzu
    r.hinzufuegen(Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, 1000.0));
    // Prüft, dass die Größe nun 1 ist
    EXPECT_EQ(r.groesse(), 1u);
    
    // Fügt eine zweite Kerze hinzu
    r.hinzufuegen(Kerze(make_time(2023, 1, 2), 105.0, 115.0, 95.0, 110.0, 1200.0));
    // Prüft, dass die Größe nun 2 ist
    EXPECT_EQ(r.groesse(), 2u);
    
    // Zugriff auf die erste Kerze über den Operator[]
    const Kerze& k = r[0];
    // Prüft, dass der Open-Preis korrekt ist
    EXPECT_DOUBLE_EQ(k.open, 100.0);
}

// Test für die Wertpapier-Klasse - prüft, dass gültige Wertpapiere korrekt erstellt werden
TEST(Wertpapier, Erstellung) {
    // Erstellt ein Wertpapier mit Symbol, Name, Land und Währung
    Wertpapier w("AAPL", "Apple Inc.", "USA", "USD");
    // Prüft, dass alle Attribute korrekt gesetzt wurden
    EXPECT_EQ(w.symbol, "AAPL");
    EXPECT_EQ(w.name, "Apple Inc.");
    EXPECT_EQ(w.land, "USA");
    EXPECT_EQ(w.waehrung, "USD");
}

// --- E1: strenge Reihenfolge, Datum in Fehlermeldungen, Validierung in hinzufuegen, Wertpapier ---

namespace {
// Liefert die Meldung der Exception, die f geworfen hat (leer, wenn keine geworfen wurde)
template <class F>
std::string meldung(F f) {
    try { f(); } catch (const std::invalid_argument& e) { return e.what(); }
    return "";
}
}  // namespace

TEST(Kerze, FehlermeldungNenntDatum) {
    // Open außerhalb von Low..High am 2023-03-05
    std::string m = meldung([] { Kerze(make_time(2023, 3, 5), 120.0, 115.0, 95.0, 110.0, 1.0); });
    EXPECT_NE(m.find("2023-03-05"), std::string::npos) << m;
}

TEST(Kerze, NegativesVolumenWirdAbgelehnt) {
    EXPECT_THROW(Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, -1.0), std::invalid_argument);
}

TEST(Kursreihe, DoppeltesDatumImKonstruktorWirdAbgelehnt) {
    std::vector<Kerze> kerzen;
    kerzen.push_back(Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, 1000.0));
    kerzen.push_back(Kerze(make_time(2023, 1, 1), 105.0, 115.0, 95.0, 110.0, 1200.0));
    std::string m = meldung([&] { Kursreihe r(kerzen); });
    EXPECT_NE(m.find("2023-01-01"), std::string::npos) << m;
}

TEST(Kursreihe, NichtAufsteigendImKonstruktorNenntDatum) {
    std::vector<Kerze> kerzen;
    kerzen.push_back(Kerze(make_time(2023, 1, 5), 100.0, 110.0, 90.0, 105.0, 1000.0));
    kerzen.push_back(Kerze(make_time(2023, 1, 3), 105.0, 115.0, 95.0, 110.0, 1200.0));
    std::string m = meldung([&] { Kursreihe r(kerzen); });
    EXPECT_NE(m.find("2023-01-03"), std::string::npos) << m;
}

TEST(Kursreihe, HinzufuegenLehntDoppeltesDatumAb) {
    Kursreihe r;
    r.hinzufuegen(Kerze(make_time(2023, 1, 1), 100.0, 110.0, 90.0, 105.0, 1000.0));
    std::string m = meldung([&] { r.hinzufuegen(Kerze(make_time(2023, 1, 1), 105.0, 115.0, 95.0, 110.0, 1.0)); });
    EXPECT_NE(m.find("2023-01-01"), std::string::npos) << m;
    EXPECT_EQ(r.groesse(), 1u);  // Reihe bleibt unverändert
}

TEST(Kursreihe, HinzufuegenLehntAelteresDatumAb) {
    Kursreihe r;
    r.hinzufuegen(Kerze(make_time(2023, 1, 5), 100.0, 110.0, 90.0, 105.0, 1000.0));
    EXPECT_THROW(r.hinzufuegen(Kerze(make_time(2023, 1, 4), 105.0, 115.0, 95.0, 110.0, 1.0)), std::invalid_argument);
    EXPECT_EQ(r.groesse(), 1u);
}

TEST(Wertpapier, LeereFelderWerdenAbgelehnt) {
    EXPECT_THROW(Wertpapier("", "Apple Inc.", "USA", "USD"), std::invalid_argument);
    EXPECT_THROW(Wertpapier("AAPL", "", "USA", "USD"), std::invalid_argument);
    EXPECT_THROW(Wertpapier("AAPL", "Apple Inc.", "", "USD"), std::invalid_argument);
    EXPECT_THROW(Wertpapier("AAPL", "Apple Inc.", "USA", ""), std::invalid_argument);
}
