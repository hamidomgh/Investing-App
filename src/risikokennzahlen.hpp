// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kerze und Kursreihe (E1): Grundlage aller Berechnungen
#include "kursdaten.hpp"
// Indikatorreihe (vector<optional<double>>) und SMA aus E3 werden wiederverwendet
#include "trendindikatoren.hpp"

// std::optional für Werte, die es nicht immer gibt (z. B. Volatilität bei zu wenigen Kursen)
#include <optional>
// std::vector für die Kerzen und Ergebnisreihen
#include <vector>

// Ergebnis der Bollinger-Bänder: drei Reihen, jeweils so lang wie die Kursreihe
struct BollingerErgebnis {
    // Mittleres Band = SMA der Schlusskurse
    Indikatorreihe mitte;
    // Oberes Band = Mitte + k * Standardabweichung
    Indikatorreihe oben;
    // Unteres Band = Mitte - k * Standardabweichung
    Indikatorreihe unten;
};

// Berechnet Volatilitäts- und Risikokennzahlen einer Kursreihe (E4)
class Risikokennzahlen {
public:
    // Konstruktor: übernimmt die Kursreihe. "explicit" verhindert versehentliche Umwandlungen.
    explicit Risikokennzahlen(const Kursreihe& reihe);

    // Bollinger-Bänder über "periode" Schlusskurse mit Breite "k" Standardabweichungen (üblich: 20, 2).
    // Wirft std::invalid_argument bei periode <= 0, k < 0 oder zu kurzer Reihe.
    BollingerErgebnis bollinger(int periode, double k) const;

    // Average True Range nach Wilder über "periode" Tage (üblich: 14).
    // Wirft std::invalid_argument bei periode <= 0 oder zu kurzer Reihe.
    Indikatorreihe atr(int periode) const;

    // Einfache Tagesrenditen: r[i] = Close[i] / Close[i-1] - 1. Index 0 ist nicht verfügbar.
    // Wirft std::invalid_argument, wenn ein Schlusskurs <= 0 ist.
    Indikatorreihe einfacheRenditen() const;

    // Logarithmische Tagesrenditen: r[i] = ln(Close[i] / Close[i-1]). Index 0 ist nicht verfügbar.
    // Wirft std::invalid_argument, wenn ein Schlusskurs <= 0 ist.
    Indikatorreihe logRenditen() const;

    // Annualisierte Volatilität = Stichproben-Standardabweichung der Log-Renditen * Wurzel(handelstage).
    // Liefert std::nullopt bei weniger als 2 Renditen (weniger als 3 Kerzen).
    // Wirft std::invalid_argument bei handelstage <= 0 oder Schlusskursen <= 0.
    std::optional<double> annualisierteVolatilitaet(int handelstage = 252) const;

    // Maximaler Drawdown: größter relativer Verlust vom bisherigen Höchststand (0 = kein Verlust, 0,5 = -50 %).
    // Leere Reihe und Reihe mit einer Kerze ergeben 0. Wirft std::invalid_argument bei Schlusskursen <= 0.
    double maxDrawdown() const;

private:
    // Hilfsfunktion: wirft, wenn ein Schlusskurs <= 0 ist (Renditen und Drawdown teilen durch Kurse)
    void pruefePositiveKurse() const;

    // Die Kursreihe selbst (ATR braucht High, Low und Close, nicht nur den Schlusskurs)
    Kursreihe reihe_;
};
