// Die Klasse, die hier getestet wird
#include "risikokennzahlen.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::chrono für die Datumswerte der Test-Kerzen
#include <chrono>
// std::sqrt und std::log für die erwarteten Werte
#include <cmath>
// std::invalid_argument für die erwarteten Fehler
#include <stdexcept>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar (keine Namenskonflikte mit anderen Tests)
namespace {

// Eine Test-Kerze ohne Datum: Open, High, Low, Close
struct Ohlc {
    // Eröffnungskurs
    double open;
    // Höchstkurs
    double high;
    // Tiefstkurs
    double low;
    // Schlusskurs
    double close;
};

// Baut eine Kursreihe aus OHLC-Werten, ein Tag pro Eintrag ab dem 2023-01-01
Kursreihe reiheAusOhlc(const std::vector<Ohlc>& werte) {
    // Leere Kursreihe
    Kursreihe reihe;
    // Startdatum
    const std::chrono::sys_days start = std::chrono::year{2023} / 1 / 1;
    // Für jeden Eintrag eine Kerze anhängen
    for (std::size_t i = 0; i < werte.size(); ++i) {
        // Datum = Start + i Tage (streng aufsteigend)
        const auto datum = start + std::chrono::days{static_cast<int>(i)};
        // Kerze mit Volumen 1 anhängen
        reihe.hinzufuegen(Kerze(datum, werte[i].open, werte[i].high, werte[i].low, werte[i].close, 1.0));
    }
    // Fertige Reihe
    return reihe;
}

// Baut eine Kursreihe nur aus Schlusskursen (Open = High = Low = Close)
Kursreihe reiheAus(const std::vector<double>& schlusskurse) {
    // OHLC-Werte sammeln
    std::vector<Ohlc> werte;
    // Für jeden Schlusskurs
    for (double c : schlusskurse) {
        // Alle vier Preise gleich: immer eine gültige Kerze
        werte.push_back({c, c, c, c});
    }
    // An die allgemeine Hilfsfunktion weitergeben
    return reiheAusOhlc(werte);
}

// Erlaubte Rundungsabweichung
constexpr double eps = 1e-9;

}  // namespace

// ---------------------------------------------------------------- Bollinger-Bänder

// Bollinger(3, k = 2) von 2, 4, 6, 8, 4 (von Hand):
//   Index 2: Fenster 2, 4, 6, Mitte 4, Abweichungen -2, 0, 2, Varianz (4 + 0 + 4) / 3 = 8/3
//   Index 3: Fenster 4, 6, 8, Mitte 6, Varianz ebenfalls 8/3
//   Index 4: Fenster 6, 8, 4, Mitte 6, Abweichungen 0, 2, -2, Varianz 8/3
//   Band = Mitte +/- 2 * Wurzel(8/3) = Mitte +/- 3,2660
TEST(Risikokennzahlen, BollingerReferenzwerte) {
    // Kennzahlen auf der Testreihe
    Risikokennzahlen rk(reiheAus({2, 4, 6, 8, 4}));
    // Bänder mit Periode 3 und Breite 2
    const BollingerErgebnis b = rk.bollinger(3, 2.0);
    // Erwartete Bandbreite
    const double breite = 2.0 * std::sqrt(8.0 / 3.0);
    // Einschwingphase: Index 0 und 1 nicht verfügbar
    EXPECT_FALSE(b.mitte[1].has_value());
    EXPECT_FALSE(b.oben[1].has_value());
    EXPECT_FALSE(b.unten[1].has_value());
    // Index 2
    EXPECT_NEAR(*b.mitte[2], 4.0, eps);
    EXPECT_NEAR(*b.oben[2], 4.0 + breite, eps);
    EXPECT_NEAR(*b.unten[2], 4.0 - breite, eps);
    // Index 4
    EXPECT_NEAR(*b.mitte[4], 6.0, eps);
    EXPECT_NEAR(*b.oben[4], 6.0 + breite, eps);
    EXPECT_NEAR(*b.unten[4], 6.0 - breite, eps);
}

// Sonderfall konstante Kurse: Standardabweichung 0, alle drei Bänder liegen aufeinander
TEST(Risikokennzahlen, BollingerKonstanteKurse) {
    // Vier gleiche Kurse
    Risikokennzahlen rk(reiheAus({5, 5, 5, 5}));
    // Bänder mit Periode 2 und Breite 2
    const BollingerErgebnis b = rk.bollinger(2, 2.0);
    // Oben und unten gleich der Mitte
    EXPECT_NEAR(*b.oben[3], 5.0, eps);
    EXPECT_NEAR(*b.unten[3], 5.0, eps);
}

// Fehlerfälle der Bollinger-Bänder
TEST(Risikokennzahlen, BollingerFehlerfaelle) {
    // Reihe mit drei Kursen
    Risikokennzahlen rk(reiheAus({1, 2, 3}));
    // Periode 0 ist ungültig
    EXPECT_THROW(rk.bollinger(0, 2.0), std::invalid_argument);
    // Negativer Faktor ist ungültig
    EXPECT_THROW(rk.bollinger(2, -1.0), std::invalid_argument);
    // Periode länger als die Reihe
    EXPECT_THROW(rk.bollinger(4, 2.0), std::invalid_argument);
}

// ---------------------------------------------------------------- ATR

// ATR(2) mit Kurslücke (von Hand):
//   Tag 0: H 12, L 9           -> True Range = 12 - 9 = 3 (kein Vortag)
//   Tag 1: H 13, L 10, Vortag 11 -> max(3, |13 - 11| = 2, |10 - 11| = 1) = 3
//   Tag 2: H 16, L 14, Vortag 12 -> max(2, |16 - 12| = 4, |14 - 12| = 2) = 4  (Lücke nach oben)
//   Tag 3: H 14, L 12, Vortag 15 -> max(2, |14 - 15| = 1, |12 - 15| = 3) = 3
//   ATR Index 1: (3 + 3) / 2 = 3
//   ATR Index 2: (3 * 1 + 4) / 2 = 3,5
//   ATR Index 3: (3,5 * 1 + 3) / 2 = 3,25
TEST(Risikokennzahlen, AtrReferenzwerte) {
    // Kerzen mit Open, High, Low, Close
    Risikokennzahlen rk(reiheAusOhlc({
        {10, 12, 9, 11},
        {11, 13, 10, 12},
        {14, 16, 14, 15},
        {13, 14, 12, 13},
    }));
    // ATR mit Periode 2
    const Indikatorreihe atr = rk.atr(2);
    // Einschwingphase: Index 0 nicht verfügbar
    EXPECT_FALSE(atr[0].has_value());
    // Referenzwerte
    EXPECT_NEAR(*atr[1], 3.0, eps);
    EXPECT_NEAR(*atr[2], 3.5, eps);
    EXPECT_NEAR(*atr[3], 3.25, eps);
}

// Sonderfall eine Kerze: ATR(1) ist die Tagesspanne
TEST(Risikokennzahlen, AtrEineKerze) {
    // Eine Kerze mit Spanne 12 - 9 = 3
    Risikokennzahlen rk(reiheAusOhlc({{10, 12, 9, 11}}));
    // ATR(1) am Index 0
    EXPECT_NEAR(*rk.atr(1)[0], 3.0, eps);
}

// Fehlerfälle der ATR
TEST(Risikokennzahlen, AtrFehlerfaelle) {
    // Reihe mit zwei Kursen
    Risikokennzahlen rk(reiheAus({1, 2}));
    // Periode 0 ist ungültig
    EXPECT_THROW(rk.atr(0), std::invalid_argument);
    // Periode 3 bei 2 Kerzen: zu kurz
    EXPECT_THROW(rk.atr(3), std::invalid_argument);
}

// ---------------------------------------------------------------- Renditen

// Renditen von 100, 110, 99 (von Hand):
//   einfach: 110 / 100 - 1 = 0,1   und   99 / 110 - 1 = -0,1
//   log:     ln(1,1) = 0,09531     und   ln(0,9) = -0,10536
TEST(Risikokennzahlen, RenditenReferenzwerte) {
    // Kennzahlen auf drei Kursen
    Risikokennzahlen rk(reiheAus({100, 110, 99}));
    // Einfache Renditen
    const Indikatorreihe einfach = rk.einfacheRenditen();
    // Logarithmische Renditen
    const Indikatorreihe logr = rk.logRenditen();
    // Am ersten Tag gibt es keinen Vortag
    EXPECT_FALSE(einfach[0].has_value());
    EXPECT_FALSE(logr[0].has_value());
    // Einfache Renditen
    EXPECT_NEAR(*einfach[1], 0.1, eps);
    EXPECT_NEAR(*einfach[2], -0.1, eps);
    // Log-Renditen
    EXPECT_NEAR(*logr[1], std::log(1.1), eps);
    EXPECT_NEAR(*logr[2], std::log(0.9), eps);
}

// Sonderfall eine Kerze: keine Rendite verfügbar, aber kein Fehler
TEST(Risikokennzahlen, RenditenEineKerze) {
    // Eine einzige Kerze
    Risikokennzahlen rk(reiheAus({100}));
    // Genau ein Eintrag, und der ist nicht verfügbar
    const Indikatorreihe r = rk.einfacheRenditen();
    ASSERT_EQ(r.size(), 1u);
    EXPECT_FALSE(r[0].has_value());
}

// Kurse <= 0 werden abgelehnt (Division durch 0 bzw. ln(0))
TEST(Risikokennzahlen, RenditenLehnenNichtPositiveKurseAb) {
    // Ein Kurs von 0 ist laut E1 eine gültige Kerze, aber hier nicht verwendbar
    Risikokennzahlen rk(reiheAus({100, 0, 50}));
    // Alle Kennzahlen, die durch Kurse teilen, werfen
    EXPECT_THROW(rk.einfacheRenditen(), std::invalid_argument);
    EXPECT_THROW(rk.logRenditen(), std::invalid_argument);
    EXPECT_THROW(rk.annualisierteVolatilitaet(), std::invalid_argument);
    EXPECT_THROW(rk.maxDrawdown(), std::invalid_argument);
}

// ---------------------------------------------------------------- Volatilität

// Volatilität von 100, 110, 99 mit 252 Handelstagen (von Hand):
//   Log-Renditen a = ln(1,1) = 0,0953102, b = ln(0,9) = -0,1053605
//   Mittelwert m = (a + b) / 2, Stichprobenvarianz = ((a - m)^2 + (b - m)^2) / 1 = (a - b)^2 / 2
//   tägliche Volatilität = |a - b| / Wurzel(2) = 0,2006707 / 1,4142136 = 0,1418971
//   annualisiert = 0,1418971 * Wurzel(252) = 0,1418971 * 15,8745079 = 2,2525...
TEST(Risikokennzahlen, VolatilitaetReferenzwert) {
    // Kennzahlen auf drei Kursen
    Risikokennzahlen rk(reiheAus({100, 110, 99}));
    // Erwarteter Wert nach der Formel oben
    const double erwartet = (std::log(1.1) - std::log(0.9)) / std::sqrt(2.0) * std::sqrt(252.0);
    // Ergebnis muss vorhanden sein
    const std::optional<double> vol = rk.annualisierteVolatilitaet(252);
    ASSERT_TRUE(vol.has_value());
    // Vergleich mit der Formel und mit dem von Hand gerundeten Wert
    EXPECT_NEAR(*vol, erwartet, eps);
    EXPECT_NEAR(*vol, 2.2525, 1e-4);
}

// Sonderfälle: konstante Kurse ergeben 0, zu wenige Kerzen ergeben "nicht verfügbar"
TEST(Risikokennzahlen, VolatilitaetSonderfaelle) {
    // Konstante Kurse: alle Renditen 0, also Volatilität 0
    EXPECT_NEAR(*Risikokennzahlen(reiheAus({5, 5, 5, 5})).annualisierteVolatilitaet(), 0.0, eps);
    // Eine Kerze: keine Rendite
    EXPECT_FALSE(Risikokennzahlen(reiheAus({5})).annualisierteVolatilitaet().has_value());
    // Zwei Kerzen: nur eine Rendite, Stichproben-Standardabweichung nicht definiert
    EXPECT_FALSE(Risikokennzahlen(reiheAus({5, 6})).annualisierteVolatilitaet().has_value());
}

// Ungültige Anzahl Handelstage wird abgelehnt
TEST(Risikokennzahlen, VolatilitaetFehlerfaelle) {
    // Gültige Reihe
    Risikokennzahlen rk(reiheAus({100, 110, 99}));
    // 0 Handelstage ist ungültig
    EXPECT_THROW(rk.annualisierteVolatilitaet(0), std::invalid_argument);
}

// ---------------------------------------------------------------- Maximaler Drawdown

// Drawdown von 100, 120, 90, 110, 60, 130 (von Hand):
//   Höchststand:  100, 120, 120, 120, 120, 130
//   Rückgang:       0,   0, 30/120 = 0,25, 10/120 = 0,083, 60/120 = 0,5, 0
//   Maximum = 0,5 (Verlust von 50 %); der neue Höchststand 130 ändert daran nichts
TEST(Risikokennzahlen, MaxDrawdownReferenzwert) {
    // Kennzahlen auf der Testreihe
    Risikokennzahlen rk(reiheAus({100, 120, 90, 110, 60, 130}));
    // Größter Rückgang
    EXPECT_NEAR(rk.maxDrawdown(), 0.5, eps);
}

// Sonderfälle: konstante Kurse, eine Kerze, leere Reihe und nur steigende Kurse ergeben 0
TEST(Risikokennzahlen, MaxDrawdownSonderfaelle) {
    // Konstante Kurse
    EXPECT_NEAR(Risikokennzahlen(reiheAus({5, 5, 5})).maxDrawdown(), 0.0, eps);
    // Eine Kerze
    EXPECT_NEAR(Risikokennzahlen(reiheAus({5})).maxDrawdown(), 0.0, eps);
    // Leere Reihe
    EXPECT_NEAR(Risikokennzahlen(Kursreihe()).maxDrawdown(), 0.0, eps);
    // Nur steigende Kurse
    EXPECT_NEAR(Risikokennzahlen(reiheAus({1, 2, 3})).maxDrawdown(), 0.0, eps);
}
