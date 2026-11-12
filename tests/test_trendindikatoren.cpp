// Die Klasse, die hier getestet wird
#include "trendindikatoren.hpp"

// GoogleTest-Framework (TEST, EXPECT_..., ASSERT_...)
#include <gtest/gtest.h>

// std::chrono für die Datumswerte der Test-Kerzen
#include <chrono>
// std::invalid_argument für die erwarteten Fehler
#include <stdexcept>
// std::vector für die Schlusskurse
#include <vector>

// Anonymer Namespace: die Hilfsfunktionen sind nur in dieser Datei sichtbar.
// Wichtig, weil test_kursdaten.cpp schon eine Funktion make_time hat (sonst doppelte Definition beim Linken).
namespace {

// Baut eine Kursreihe aus Schlusskursen: ein Tag pro Kurs ab dem 2023-01-01.
// Open, High und Low werden gleich dem Close gesetzt, damit jede Kerze gültig ist (E1).
Kursreihe reiheAus(const std::vector<double>& schlusskurse) {
    // Leere Kursreihe anlegen
    Kursreihe reihe;
    // Startdatum 2023-01-01 als Tageszeitpunkt (C++20-Kalender aus <chrono>)
    const std::chrono::sys_days start = std::chrono::year{2023} / 1 / 1;
    // Für jeden Schlusskurs eine Kerze erzeugen
    for (std::size_t i = 0; i < schlusskurse.size(); ++i) {
        // Datum = Startdatum + i Tage, damit die Daten streng aufsteigend sind
        const auto datum = start + std::chrono::days{static_cast<int>(i)};
        // Der Kurs des Tages
        const double c = schlusskurse[i];
        // Kerze mit Open = High = Low = Close = c und Volumen 1 anhängen
        reihe.hinzufuegen(Kerze(datum, c, c, c, c, 1.0));
    }
    // Fertige Reihe zurückgeben
    return reihe;
}

// Erlaubte Rundungsabweichung beim Vergleich von Kommazahlen
constexpr double eps = 1e-9;

}  // namespace

// ---------------------------------------------------------------- SMA

// SMA(3) von 2, 4, 6, 8, 4 (von Hand):
//   Index 0, 1: nicht verfügbar (noch keine 3 Kurse)
//   Index 2: (2 + 4 + 6) / 3 = 4
//   Index 3: (4 + 6 + 8) / 3 = 6
//   Index 4: (6 + 8 + 4) / 3 = 6
TEST(Trendindikatoren, SmaReferenzwerte) {
    // Indikatoren auf der Testreihe anlegen
    Trendindikatoren ind(reiheAus({2, 4, 6, 8, 4}));
    // SMA mit Periode 3 berechnen
    const Indikatorreihe sma = ind.sma(3);
    // Eine Ausgabe pro Kurs
    ASSERT_EQ(sma.size(), 5u);
    // Einschwingphase: die ersten beiden Werte sind nicht verfügbar
    EXPECT_FALSE(sma[0].has_value());
    EXPECT_FALSE(sma[1].has_value());
    // Referenzwerte vergleichen
    EXPECT_NEAR(*sma[2], 4.0, eps);
    EXPECT_NEAR(*sma[3], 6.0, eps);
    EXPECT_NEAR(*sma[4], 6.0, eps);
}

// SMA(1) ist der Kurs selbst und ab dem ersten Tag verfügbar
TEST(Trendindikatoren, SmaPeriodeEinsIstDerKurs) {
    // Indikatoren auf drei Kursen
    Trendindikatoren ind(reiheAus({5, 7, 9}));
    // SMA mit Periode 1
    const Indikatorreihe sma = ind.sma(1);
    // Jeder Wert ist genau der Schlusskurs
    EXPECT_NEAR(*sma[0], 5.0, eps);
    EXPECT_NEAR(*sma[1], 7.0, eps);
    EXPECT_NEAR(*sma[2], 9.0, eps);
}

// Ungültige Perioden und zu kurze Reihen werden abgelehnt
TEST(Trendindikatoren, SmaFehlerfaelle) {
    // Reihe mit drei Kursen
    Trendindikatoren ind(reiheAus({1, 2, 3}));
    // Periode 0 ist ungültig
    EXPECT_THROW(ind.sma(0), std::invalid_argument);
    // Negative Periode ist ungültig
    EXPECT_THROW(ind.sma(-2), std::invalid_argument);
    // Periode 4 bei nur 3 Kursen: Reihe zu kurz
    EXPECT_THROW(ind.sma(4), std::invalid_argument);
}

// ---------------------------------------------------------------- EMA

// EMA(3) von 2, 4, 6, 8, 4 (von Hand), alpha = 2 / (3 + 1) = 0,5:
//   Index 2: Startwert = SMA = (2 + 4 + 6) / 3 = 4
//   Index 3: 0,5 * 8 + 0,5 * 4 = 6
//   Index 4: 0,5 * 4 + 0,5 * 6 = 5   (der SMA wäre hier 6: die EMA reagiert schneller)
TEST(Trendindikatoren, EmaReferenzwerte) {
    // Indikatoren auf der Testreihe anlegen
    Trendindikatoren ind(reiheAus({2, 4, 6, 8, 4}));
    // EMA mit Periode 3 berechnen
    const Indikatorreihe ema = ind.ema(3);
    // Eine Ausgabe pro Kurs
    ASSERT_EQ(ema.size(), 5u);
    // Einschwingphase
    EXPECT_FALSE(ema[0].has_value());
    EXPECT_FALSE(ema[1].has_value());
    // Referenzwerte
    EXPECT_NEAR(*ema[2], 4.0, eps);
    EXPECT_NEAR(*ema[3], 6.0, eps);
    EXPECT_NEAR(*ema[4], 5.0, eps);
}

// Fehlerfälle der EMA
TEST(Trendindikatoren, EmaFehlerfaelle) {
    // Reihe mit zwei Kursen
    Trendindikatoren ind(reiheAus({1, 2}));
    // Periode 0 ist ungültig
    EXPECT_THROW(ind.ema(0), std::invalid_argument);
    // Periode 3 bei nur 2 Kursen: Reihe zu kurz
    EXPECT_THROW(ind.ema(3), std::invalid_argument);
}

// ---------------------------------------------------------------- MACD

// MACD(schnell 2, langsam 3, signal 2) von 2, 4, 6, 8, 4, 10 (von Hand):
//   EMA(2), alpha = 2/3: Index 1: 3, Index 2: 5, Index 3: 7, Index 4: 5, Index 5: 25/3
//   EMA(3), alpha = 1/2: Index 2: 4, Index 3: 6, Index 4: 5, Index 5: 7,5
//   Linie = EMA(2) - EMA(3): Index 2: 1, Index 3: 1, Index 4: 0, Index 5: 25/3 - 7,5 = 5/6
//   Signal = EMA(2) der Linie: Index 3: (1 + 1) / 2 = 1, Index 4: 2/3 * 0 + 1/3 * 1 = 1/3,
//            Index 5: 2/3 * 5/6 + 1/3 * 1/3 = 2/3
//   Histogramm = Linie - Signal: Index 3: 0, Index 4: -1/3, Index 5: 5/6 - 2/3 = 1/6
TEST(Trendindikatoren, MacdReferenzwerte) {
    // Indikatoren auf der Testreihe anlegen
    Trendindikatoren ind(reiheAus({2, 4, 6, 8, 4, 10}));
    // MACD mit kleinen Perioden, damit man von Hand rechnen kann
    const MacdErgebnis m = ind.macd(2, 3, 2);
    // Alle drei Reihen haben einen Eintrag pro Kurs
    ASSERT_EQ(m.linie.size(), 6u);
    ASSERT_EQ(m.signal.size(), 6u);
    ASSERT_EQ(m.histogramm.size(), 6u);
    // Linie erst ab Index 2 (langsam - 1) verfügbar
    EXPECT_FALSE(m.linie[1].has_value());
    EXPECT_NEAR(*m.linie[2], 1.0, eps);
    EXPECT_NEAR(*m.linie[3], 1.0, eps);
    EXPECT_NEAR(*m.linie[4], 0.0, eps);
    EXPECT_NEAR(*m.linie[5], 5.0 / 6.0, eps);
    // Signal erst ab Index 3 (langsam + signal - 2) verfügbar
    EXPECT_FALSE(m.signal[2].has_value());
    EXPECT_NEAR(*m.signal[3], 1.0, eps);
    EXPECT_NEAR(*m.signal[4], 1.0 / 3.0, eps);
    EXPECT_NEAR(*m.signal[5], 2.0 / 3.0, eps);
    // Histogramm ab demselben Index wie das Signal
    EXPECT_FALSE(m.histogramm[2].has_value());
    EXPECT_NEAR(*m.histogramm[3], 0.0, eps);
    EXPECT_NEAR(*m.histogramm[4], -1.0 / 3.0, eps);
    EXPECT_NEAR(*m.histogramm[5], 1.0 / 6.0, eps);
}

// Fehlerfälle des MACD
TEST(Trendindikatoren, MacdFehlerfaelle) {
    // Reihe mit vier Kursen
    Trendindikatoren ind(reiheAus({1, 2, 3, 4}));
    // Perioden <= 0 sind ungültig
    EXPECT_THROW(ind.macd(0, 3, 2), std::invalid_argument);
    EXPECT_THROW(ind.macd(2, 3, 0), std::invalid_argument);
    // Schnelle Periode muss kleiner als die langsame sein
    EXPECT_THROW(ind.macd(3, 3, 2), std::invalid_argument);
    EXPECT_THROW(ind.macd(4, 3, 2), std::invalid_argument);
    // Benötigt langsam + signal - 1 = 3 + 2 - 1 = 4 Kurse: genau passend, kein Fehler
    EXPECT_NO_THROW(ind.macd(2, 3, 2));
    // Benötigt 3 + 3 - 1 = 5 Kurse, vorhanden sind 4: Reihe zu kurz
    EXPECT_THROW(ind.macd(2, 3, 3), std::invalid_argument);
}

// ---------------------------------------------------------------- RSI

// RSI(3) von 10, 11, 12, 11, 13, 12 (von Hand):
//   Änderungen: +1, +1, -1, +2, -1
//   Index 3: Gewinn-Ø = (1 + 1 + 0) / 3 = 2/3, Verlust-Ø = (0 + 0 + 1) / 3 = 1/3
//            RS = 2, RSI = 100 - 100 / 3 = 66,666...
//   Index 4: Gewinn-Ø = (2/3 * 2 + 2) / 3 = 10/9, Verlust-Ø = (1/3 * 2 + 0) / 3 = 2/9
//            RS = 5, RSI = 100 - 100 / 6 = 83,333...
//   Index 5: Gewinn-Ø = (10/9 * 2 + 0) / 3 = 20/27, Verlust-Ø = (2/9 * 2 + 1) / 3 = 13/27
//            RS = 20/13, RSI = 100 * 20 / 33 = 60,6060...
TEST(Trendindikatoren, RsiReferenzwerte) {
    // Indikatoren auf der Testreihe anlegen
    Trendindikatoren ind(reiheAus({10, 11, 12, 11, 13, 12}));
    // RSI mit Periode 3
    const Indikatorreihe rsi = ind.rsi(3);
    // Eine Ausgabe pro Kurs
    ASSERT_EQ(rsi.size(), 6u);
    // Einschwingphase: Index 0 bis 2 nicht verfügbar
    EXPECT_FALSE(rsi[0].has_value());
    EXPECT_FALSE(rsi[1].has_value());
    EXPECT_FALSE(rsi[2].has_value());
    // Referenzwerte
    EXPECT_NEAR(*rsi[3], 200.0 / 3.0, eps);
    EXPECT_NEAR(*rsi[4], 250.0 / 3.0, eps);
    EXPECT_NEAR(*rsi[5], 2000.0 / 33.0, eps);
}

// Sonderfälle: nur steigende Kurse ergeben 100, konstante Kurse ergeben 50
TEST(Trendindikatoren, RsiSonderfaelle) {
    // Nur Gewinne, keine Verluste
    Trendindikatoren steigend(reiheAus({1, 2, 3, 4}));
    // RSI(3) hat genau einen Wert, am Index 3
    EXPECT_NEAR(*steigend.rsi(3)[3], 100.0, eps);
    // Keine Änderungen
    Trendindikatoren konstant(reiheAus({5, 5, 5, 5}));
    // Neutraler Wert 50
    EXPECT_NEAR(*konstant.rsi(3)[3], 50.0, eps);
}

// Fehlerfälle des RSI
TEST(Trendindikatoren, RsiFehlerfaelle) {
    // Reihe mit drei Kursen
    Trendindikatoren ind(reiheAus({1, 2, 3}));
    // Periode 0 ist ungültig
    EXPECT_THROW(ind.rsi(0), std::invalid_argument);
    // RSI(3) braucht 4 Kurse, vorhanden sind 3
    EXPECT_THROW(ind.rsi(3), std::invalid_argument);
    // RSI(2) braucht 3 Kurse: passt
    EXPECT_NO_THROW(ind.rsi(2));
}
