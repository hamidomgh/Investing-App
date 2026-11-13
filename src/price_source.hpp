// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kursreihe und Wertpapier (E1)
#include "kursdaten.hpp"

// Schnittstelle (Interface) für Kursdaten-Quellen (E2).
// Eine Implementierung (z. B. für Alpha Vantage) holt die Daten über einen IHttpClient,
// wandelt die JSON-Antwort in validierte Kerzen um und gibt sie als Kursreihe zurück.
// Die Analyse (E3-E6) kennt nur diese Schnittstelle, nicht den Anbieter.
class IPriceSource {
public:
    // Virtueller Destruktor, damit abgeleitete Klassen über einen Basiszeiger richtig gelöscht werden
    virtual ~IPriceSource() = default;

    // Lädt die täglichen Kurse eines Wertpapiers.
    // Wirft eine Unterklasse von QuellenFehler (siehe quellenfehler.hpp), wenn das nicht gelingt.
    virtual Kursreihe ladeKurse(const Wertpapier& wertpapier) = 0;
};
