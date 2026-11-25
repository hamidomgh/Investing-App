// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kerze und Kursreihe (E1)
#include "kursdaten.hpp"

// std::size_t für Indizes
#include <cstddef>
// std::vector für Ergebnislisten
#include <vector>

// Art eines lokalen Extremums: Hoch (Spitze) oder Tief (Tal)
enum class ExtremumArt { Hoch, Tief };

// Ein lokales Hoch oder Tief in der Kursreihe
struct Extremum {
    // Position in der Kursreihe (bei einem Plateau: erster Tag des Plateaus)
    std::size_t index;
    // Preis des Extremums (High der Kerze bei einem Hoch, Low der Kerze bei einem Tief)
    double preis;
    // Hoch oder Tief
    ExtremumArt art;
};

// Einstufung eines Niveaus relativ zum letzten Schlusskurs
enum class NiveauArt { Support, Resistance };

// Ein Support- oder Resistance-Niveau: zusammengefasste, nahe beieinanderliegende Extrema
struct Niveau {
    // Preis des Niveaus = Mittelwert der zusammengefassten Extrema
    double preis;
    // Berührungszahl: wie viele Extrema zu diesem Niveau gehören
    int beruehrungen;
    // Support (unter oder auf dem letzten Schlusskurs) oder Resistance (darüber)
    NiveauArt art;
};

// Klassische Pivot-Punkte, berechnet aus High, Low und Close einer Kerze
struct PivotPunkte {
    // Pivot-Punkt P = (High + Low + Close) / 3
    double p;
    // Erster Widerstand R1 = 2P - Low
    double r1;
    // Zweiter Widerstand R2 = P + (High - Low)
    double r2;
    // Erste Unterstützung S1 = 2P - High
    double s1;
    // Zweite Unterstützung S2 = P - (High - Low)
    double s2;
};

// Erkennt Support- und Resistance-Niveaus und berechnet Pivot-Punkte (E5)
class SupportResistance {
public:
    // Konstruktor: übernimmt die Kursreihe. "explicit" verhindert versehentliche Umwandlungen.
    explicit SupportResistance(const Kursreihe& reihe);

    // Lokale Hochs und Tiefs: ein Punkt ist ein Hoch, wenn die "fenster" Tage davor und danach
    // alle niedriger sind (beim Tief: alle höher). Tage am Rand ohne volles Fenster werden übersprungen.
    // Ein flaches Plateau gleicher Werte zählt als ein Extremum (am ersten Tag des Plateaus).
    // Ergebnis nach Index sortiert. Wirft std::invalid_argument bei fenster <= 0.
    std::vector<Extremum> lokaleExtrema(int fenster) const;

    // Fasst die Preise aller lokalen Extrema zu Niveaus zusammen: Ein Preis gehört zu einem Niveau,
    // wenn er höchstens "toleranz" (relativ, z. B. 0,02 = 2 %) vom Mittelwert des Niveaus abweicht.
    // Jedes Niveau wird relativ zum letzten Schlusskurs eingestuft. Ergebnis aufsteigend nach Preis.
    // Wirft std::invalid_argument bei fenster <= 0 oder toleranz < 0.
    std::vector<Niveau> niveaus(int fenster, double toleranz) const;

    // Pivot-Punkte aus einer beliebigen Kerze (üblich: die Kerze des Vortags)
    static PivotPunkte pivotAus(const Kerze& kerze);

    // Pivot-Punkte aus der letzten Kerze der Reihe. Wirft std::invalid_argument bei leerer Reihe.
    PivotPunkte pivotPunkte() const;

private:
    // Hilfsfunktion: findet die Indizes der lokalen Maxima in "werte" (Plateau-sicher, Ränder übersprungen).
    // Für Tiefs wird sie mit den negierten Low-Werten aufgerufen: ein Minimum von x ist ein Maximum von -x.
    static std::vector<std::size_t> findeMaxima(const std::vector<double>& werte, std::size_t fenster);

    // Die Kursreihe
    Kursreihe reihe_;
};
