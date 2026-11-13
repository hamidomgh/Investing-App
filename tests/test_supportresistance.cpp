// Die Klasse, die hier getestet wird
#include "supportresistance.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::chrono für die Datumswerte der Test-Kerzen
#include <chrono>
// std::invalid_argument für die erwarteten Fehler
#include <stdexcept>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Baut eine Kursreihe aus Schlusskursen (Open = High = Low = Close), ein Tag pro Kurs ab 2023-01-01
Kursreihe reiheAus(const std::vector<double>& kurse) {
    // Leere Kursreihe
    Kursreihe reihe;
    // Startdatum
    const std::chrono::sys_days start = std::chrono::year{2023} / 1 / 1;
    // Für jeden Kurs eine Kerze anhängen
    for (std::size_t i = 0; i < kurse.size(); ++i) {
        // Datum = Start + i Tage
        const auto datum = start + std::chrono::days{static_cast<int>(i)};
        // Alle vier Preise gleich, Volumen 1
        reihe.hinzufuegen(Kerze(datum, kurse[i], kurse[i], kurse[i], kurse[i], 1.0));
    }
    // Fertige Reihe
    return reihe;
}

// Sammelt die Indizes aller Extrema einer bestimmten Art (macht die Tests kürzer)
std::vector<std::size_t> indizes(const std::vector<Extremum>& extrema, ExtremumArt art) {
    // Ergebnisliste
    std::vector<std::size_t> ergebnis;
    // Alle Extrema durchgehen
    for (const Extremum& e : extrema) {
        // Nur die gesuchte Art übernehmen
        if (e.art == art) {
            // Index anhängen
            ergebnis.push_back(e.index);
        }
    }
    // Liste zurückgeben
    return ergebnis;
}

// Erlaubte Rundungsabweichung
constexpr double eps = 1e-9;

// Kurzschreibweise für eine Liste von Indizes
using Indizes = std::vector<std::size_t>;

}  // namespace

// ---------------------------------------------------------------- Lokale Extrema

// Fenster 1 auf 1, 3, 2, 5, 4:
//   Index 1 (3): Nachbarn 1 und 2 kleiner -> Hoch
//   Index 2 (2): Nachbarn 3 und 5 größer  -> Tief
//   Index 3 (5): Nachbarn 2 und 4 kleiner -> Hoch
//   Index 0 und 4: Rand, kein volles Fenster -> übersprungen
TEST(SupportResistance, ExtremaFensterEins) {
    // Extrema mit Fenster 1
    const auto e = SupportResistance(reiheAus({1, 3, 2, 5, 4})).lokaleExtrema(1);
    // Hochs bei Index 1 und 3
    EXPECT_EQ(indizes(e, ExtremumArt::Hoch), (Indizes{1, 3}));
    // Tief bei Index 2
    EXPECT_EQ(indizes(e, ExtremumArt::Tief), (Indizes{2}));
    // Zeitlich sortiert: 1 (Hoch), 2 (Tief), 3 (Hoch)
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[0].index, 1u);
    EXPECT_EQ(e[1].index, 2u);
    EXPECT_EQ(e[2].index, 3u);
    // Preis des Hochs bei Index 3
    EXPECT_NEAR(e[2].preis, 5.0, eps);
}

// Fenster 2 auf 1, 2, 5, 3, 2, 4, 6:
//   Index 2 (5): links 1, 2 und rechts 3, 2 kleiner -> Hoch
//   Index 4 (2): links 5, 3 und rechts 4, 6 größer -> Tief
//   Index 0, 1, 5, 6: Rand (weniger als 2 Tage auf einer Seite)
TEST(SupportResistance, ExtremaFensterZwei) {
    // Extrema mit Fenster 2
    const auto e = SupportResistance(reiheAus({1, 2, 5, 3, 2, 4, 6})).lokaleExtrema(2);
    // Ein Hoch bei Index 2
    EXPECT_EQ(indizes(e, ExtremumArt::Hoch), (Indizes{2}));
    // Ein Tief bei Index 4
    EXPECT_EQ(indizes(e, ExtremumArt::Tief), (Indizes{4}));
}

// Ränder: ein Höchstwert am ersten oder letzten Tag hat kein volles Fenster und zählt nicht
TEST(SupportResistance, ExtremaRaenderWerdenUebersprungen) {
    // Höchster Wert am Anfang, tiefster am Ende
    const auto e = SupportResistance(reiheAus({9, 5, 7, 5, 1})).lokaleExtrema(1);
    // Nur Index 2 (7) ist ein Hoch; die 9 bei Index 0 liegt am Rand
    EXPECT_EQ(indizes(e, ExtremumArt::Hoch), (Indizes{2}));
    // Nur Index 1 (5, Nachbarn 9 und 7) ist ein Tief. Index 3 (5) nicht, weil der rechte Nachbar 1 tiefer ist.
    // Die 1 bei Index 4 wäre das tiefste Tief, liegt aber am Rand.
    EXPECT_EQ(indizes(e, ExtremumArt::Tief), (Indizes{1}));
}

// Flaches Plateau: 1, 3, 3, 3, 1 hat genau ein Hoch, am ersten Tag des Plateaus (Index 1)
TEST(SupportResistance, ExtremaPlateauZaehltEinmal) {
    // Plateau aus drei gleichen Werten
    const auto e = SupportResistance(reiheAus({1, 3, 3, 3, 1})).lokaleExtrema(1);
    // Nur ein Hoch
    EXPECT_EQ(indizes(e, ExtremumArt::Hoch), (Indizes{1}));
}

// Plateau, nach dem der Kurs weiter steigt, ist kein Hoch: 1, 3, 3, 4, 2 -> nur die 4 (Index 3)
TEST(SupportResistance, ExtremaPlateauOhneAbfallIstKeinHoch) {
    // Das Plateau 3, 3 wird von der 4 überragt
    const auto e = SupportResistance(reiheAus({1, 3, 3, 4, 2})).lokaleExtrema(1);
    // Nur die 4 ist ein Hoch
    EXPECT_EQ(indizes(e, ExtremumArt::Hoch), (Indizes{3}));
}

// Plateau bei Tiefs: 5, 2, 2, 5 hat genau ein Tief bei Index 1
TEST(SupportResistance, ExtremaPlateauBeiTiefs) {
    // Plateau aus zwei gleichen Tiefstwerten
    const auto e = SupportResistance(reiheAus({5, 2, 2, 5})).lokaleExtrema(1);
    // Ein Tief am ersten Tag des Plateaus
    EXPECT_EQ(indizes(e, ExtremumArt::Tief), (Indizes{1}));
}

// Konstante Kurse und zu kurze Reihen haben keine Extrema
TEST(SupportResistance, ExtremaSonderfaelle) {
    // Konstante Kurse: ein einziges Plateau, das an beiden Rändern liegt
    EXPECT_TRUE(SupportResistance(reiheAus({5, 5, 5, 5})).lokaleExtrema(1).empty());
    // Leere Reihe
    EXPECT_TRUE(SupportResistance(Kursreihe()).lokaleExtrema(1).empty());
    // Fenster größer als die halbe Reihe: kein Tag hat ein volles Fenster
    EXPECT_TRUE(SupportResistance(reiheAus({1, 3, 1})).lokaleExtrema(2).empty());
}

// Fenster <= 0 wird abgelehnt
TEST(SupportResistance, ExtremaFehlerfaelle) {
    // Gültige Reihe
    SupportResistance sr(reiheAus({1, 3, 1}));
    // Fenster 0 und negativ sind ungültig
    EXPECT_THROW(sr.lokaleExtrema(0), std::invalid_argument);
    EXPECT_THROW(sr.lokaleExtrema(-1), std::invalid_argument);
}

// ---------------------------------------------------------------- Niveaus

// Kurse 10, 20, 11, 20.2, 10.1, 15 mit Fenster 1:
//   Hochs: 20 (Index 1), 20.2 (Index 3); Tiefs: 11 (Index 2), 10.1 (Index 4)
//   Sortierte Preise: 10.1, 11, 20, 20.2
//   Toleranz 2 %:
//     11 gegen Mittel 10.1: |11 - 10.1| / 10.1 = 8,9 % > 2 %  -> neues Niveau
//     20 gegen Mittel 11: weit entfernt                        -> neues Niveau
//     20.2 gegen Mittel 20: 0.2 / 20 = 1 % <= 2 %              -> gleiches Niveau, Mittel 20.1
//   Ergebnis: 10.1 (1 Berührung), 11 (1), 20.1 (2)
//   Letzter Kurs 15: 10.1 und 11 darunter -> Support, 20.1 darüber -> Resistance
TEST(SupportResistance, NiveausReferenzwerte) {
    // Niveaus mit Fenster 1 und 2 % Toleranz
    const auto n = SupportResistance(reiheAus({10, 20, 11, 20.2, 10.1, 15})).niveaus(1, 0.02);
    // Drei Niveaus
    ASSERT_EQ(n.size(), 3u);
    // Niveau 1
    EXPECT_NEAR(n[0].preis, 10.1, eps);
    EXPECT_EQ(n[0].beruehrungen, 1);
    EXPECT_EQ(n[0].art, NiveauArt::Support);
    // Niveau 2
    EXPECT_NEAR(n[1].preis, 11.0, eps);
    EXPECT_EQ(n[1].beruehrungen, 1);
    EXPECT_EQ(n[1].art, NiveauArt::Support);
    // Niveau 3: zusammengefasst aus 20 und 20.2
    EXPECT_NEAR(n[2].preis, 20.1, eps);
    EXPECT_EQ(n[2].beruehrungen, 2);
    EXPECT_EQ(n[2].art, NiveauArt::Resistance);
}

// Größere Toleranz (10 %) fasst auch 10.1 und 11 zusammen: Mittel 10.55 mit 2 Berührungen
TEST(SupportResistance, NiveausGroessereToleranz) {
    // Gleiche Reihe, 10 % Toleranz
    const auto n = SupportResistance(reiheAus({10, 20, 11, 20.2, 10.1, 15})).niveaus(1, 0.10);
    // Zwei Niveaus
    ASSERT_EQ(n.size(), 2u);
    // Unteres Niveau
    EXPECT_NEAR(n[0].preis, 10.55, eps);
    EXPECT_EQ(n[0].beruehrungen, 2);
    // Oberes Niveau
    EXPECT_NEAR(n[1].preis, 20.1, eps);
    EXPECT_EQ(n[1].beruehrungen, 2);
    // Aufsteigend sortiert
    EXPECT_LT(n[0].preis, n[1].preis);
}

// Toleranz 0: nur exakt gleiche Preise werden zusammengefasst
TEST(SupportResistance, NiveausToleranzNull) {
    // Zwei Hochs mit genau gleichem Preis 20 (Index 1 und 3), ein Tief 10 (Index 2)
    const auto n = SupportResistance(reiheAus({5, 20, 10, 20, 5})).niveaus(1, 0.0);
    // Niveaus 10 (1 Berührung) und 20 (2 Berührungen)
    ASSERT_EQ(n.size(), 2u);
    EXPECT_NEAR(n[0].preis, 10.0, eps);
    EXPECT_EQ(n[0].beruehrungen, 1);
    EXPECT_NEAR(n[1].preis, 20.0, eps);
    EXPECT_EQ(n[1].beruehrungen, 2);
}

// Ein Niveau genau auf dem letzten Schlusskurs gilt als Support
TEST(SupportResistance, NiveauAufLetztemKursIstSupport) {
    // Tief 10 bei Index 1, letzter Kurs ebenfalls 10
    const auto n = SupportResistance(reiheAus({20, 10, 20, 10})).niveaus(1, 0.0);
    // Hoch 20 (Index 2) und Tief 10 (Index 1)
    ASSERT_EQ(n.size(), 2u);
    // 10 = letzter Kurs -> Support
    EXPECT_EQ(n[0].art, NiveauArt::Support);
    // 20 > letzter Kurs -> Resistance
    EXPECT_EQ(n[1].art, NiveauArt::Resistance);
}

// Ohne Extrema gibt es keine Niveaus, aber auch keinen Fehler
TEST(SupportResistance, NiveausOhneExtremaSindLeer) {
    // Konstante Kurse
    EXPECT_TRUE(SupportResistance(reiheAus({5, 5, 5})).niveaus(1, 0.02).empty());
    // Leere Reihe
    EXPECT_TRUE(SupportResistance(Kursreihe()).niveaus(1, 0.02).empty());
}

// Ungültige Parameter werden abgelehnt
TEST(SupportResistance, NiveausFehlerfaelle) {
    // Gültige Reihe
    SupportResistance sr(reiheAus({1, 3, 1}));
    // Negative Toleranz
    EXPECT_THROW(sr.niveaus(1, -0.01), std::invalid_argument);
    // Fenster 0
    EXPECT_THROW(sr.niveaus(0, 0.02), std::invalid_argument);
}

// ---------------------------------------------------------------- Pivot-Punkte

// Kerze mit High 110, Low 90, Close 105 (von Hand):
//   P  = (110 + 90 + 105) / 3 = 305 / 3 = 101,667
//   R1 = 2 * 101,667 - 90  = 113,333
//   S1 = 2 * 101,667 - 110 =  93,333
//   R2 = 101,667 + (110 - 90) = 121,667
//   S2 = 101,667 - (110 - 90) =  81,667
TEST(SupportResistance, PivotReferenzwerte) {
    // Einzelne Kerze (Datum spielt hier keine Rolle)
    const Kerze k(std::chrono::sys_days{std::chrono::year{2023} / 1 / 1}, 100.0, 110.0, 90.0, 105.0, 1.0);
    // Pivot-Punkte berechnen
    const PivotPunkte pp = SupportResistance::pivotAus(k);
    // Pivot-Punkt
    const double p = 305.0 / 3.0;
    // Alle fünf Werte vergleichen
    EXPECT_NEAR(pp.p, p, eps);
    EXPECT_NEAR(pp.r1, 2.0 * p - 90.0, eps);
    EXPECT_NEAR(pp.s1, 2.0 * p - 110.0, eps);
    EXPECT_NEAR(pp.r2, p + 20.0, eps);
    EXPECT_NEAR(pp.s2, p - 20.0, eps);
    // Zusätzlich die von Hand gerundeten Werte
    EXPECT_NEAR(pp.r1, 113.333, 1e-3);
    EXPECT_NEAR(pp.s2, 81.667, 1e-3);
}

// pivotPunkte() nimmt die letzte Kerze der Reihe; leere Reihe wird abgelehnt
TEST(SupportResistance, PivotAusLetzterKerze) {
    // Reihe mit Kursen 10, 20, 30: letzte Kerze hat High = Low = Close = 30
    const PivotPunkte pp = SupportResistance(reiheAus({10, 20, 30})).pivotPunkte();
    // P = (30 + 30 + 30) / 3 = 30, Spanne 0, also alle Werte 30
    EXPECT_NEAR(pp.p, 30.0, eps);
    EXPECT_NEAR(pp.r2, 30.0, eps);
    EXPECT_NEAR(pp.s2, 30.0, eps);
    // Leere Reihe: Fehler
    EXPECT_THROW(SupportResistance(Kursreihe()).pivotPunkte(), std::invalid_argument);
}
