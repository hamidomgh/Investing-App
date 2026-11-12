// Verhindert, dass der Compiler diese Datei mehrfach einliest, wenn sie mehrfach eingebunden wird
#pragma once

// Eigene Header-Datei mit Kerze und Kursreihe (E1), weil die Indikatoren auf einer Kursreihe rechnen
#include "kursdaten.hpp"

// std::optional: ein Wert, der entweder vorhanden ist oder "nicht verfügbar" (std::nullopt)
#include <optional>
// std::vector: dynamisches Array für die Werte der Indikatoren
#include <vector>

// Typ-Alias: Eine Indikatorreihe hat für jede Kerze genau einen Eintrag.
// Ein Eintrag ist std::nullopt, solange die Einschwingphase noch nicht abgelaufen ist.
using Indikatorreihe = std::vector<std::optional<double>>;

// Ergebnis des MACD: drei Reihen, jeweils so lang wie die Kursreihe
struct MacdErgebnis {
    // MACD-Linie = schnelle EMA minus langsame EMA
    Indikatorreihe linie;
    // Signallinie = EMA der MACD-Linie
    Indikatorreihe signal;
    // Histogramm = MACD-Linie minus Signallinie
    Indikatorreihe histogramm;
};

// Berechnet Trend- und Momentum-Indikatoren (SMA, EMA, MACD, RSI) auf den Schlusskursen einer Kursreihe
class Trendindikatoren {
public:
    // Konstruktor: übernimmt die Schlusskurse der Kursreihe. "explicit" verhindert eine
    // versehentliche automatische Umwandlung einer Kursreihe in ein Trendindikatoren-Objekt.
    explicit Trendindikatoren(const Kursreihe& reihe);

    // Einfacher gleitender Durchschnitt (Simple Moving Average) über "periode" Schlusskurse.
    // Wirft std::invalid_argument bei periode <= 0 oder wenn die Reihe kürzer als die Periode ist.
    Indikatorreihe sma(int periode) const;

    // Exponentieller gleitender Durchschnitt (Exponential Moving Average).
    // Startwert ist der SMA der ersten "periode" Kurse. Gleiche Fehlerfälle wie sma().
    Indikatorreihe ema(int periode) const;

    // MACD mit einstellbaren Perioden (Standard in der Praxis: 12, 26, 9).
    // Wirft std::invalid_argument bei Perioden <= 0, schnell >= langsam
    // oder wenn die Reihe kürzer als langsam + signal - 1 ist.
    MacdErgebnis macd(int schnell, int langsam, int signal) const;

    // Relative Strength Index nach Wilder (Werte 0..100).
    // Wirft std::invalid_argument bei periode <= 0 oder wenn die Reihe weniger als periode + 1 Kurse hat.
    Indikatorreihe rsi(int periode) const;

private:
    // Hilfsfunktion: EMA über beliebige Werte (wird für die Kurse und für die Signallinie des MACD gebraucht).
    // "static", weil sie keine Daten des Objekts benötigt, nur ihre Parameter.
    static Indikatorreihe emaUeber(const std::vector<double>& werte, int periode);

    // Hilfsfunktion: prüft Periode und Länge und wirft bei Fehlern mit verständlicher Meldung
    static void pruefePeriode(int periode, std::size_t benoetigt, std::size_t vorhanden, const char* name);

    // Die Schlusskurse der Kursreihe, in zeitlich aufsteigender Reihenfolge
    std::vector<double> schlusskurse_;
};
