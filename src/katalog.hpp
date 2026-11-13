// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Wertpapier (E1)
#include "kursdaten.hpp"

// std::size_t für die Position im Katalog
#include <cstddef>
// std::string
#include <string>
// std::vector
#include <vector>

// Ein fehlerhafter Eintrag im Katalog: wo er steht und was nicht stimmt
struct KatalogFehler {
    // Position des Eintrags in der Liste "wertpapiere" (0 = erster Eintrag)
    std::size_t position;
    // Symbol des Eintrags, falls lesbar, sonst leer
    std::string symbol;
    // Verständliche Beschreibung des Fehlers
    std::string meldung;
};

// Katalog der Wertpapiere (E2), gelesen aus einer JSON-Konfigurationsdatei. Aufbau der Datei:
//
//   { "wertpapiere": [
//       { "symbol": "IBM", "name": "International Business Machines", "land": "USA", "waehrung": "USD" },
//       ...
//   ] }
//
// "symbol" ist das Symbol, das der Kursdaten-Anbieter erwartet (bei Alpha Vantage z. B. "MBG.DEX").
// Ein neues Wertpapier oder Land wird durch einen neuen Eintrag ergänzt, ohne den Code zu ändern.
// Ein fehlerhafter Eintrag wird übersprungen und in fehler() gemeldet; die übrigen bleiben erhalten.
class Katalog {
public:
    // Liest den Katalog aus einem JSON-Text.
    // Wirft ParseFehler nur, wenn der GANZE Katalog unbrauchbar ist (kein gültiges JSON, kein Feld "wertpapiere").
    static Katalog ausJson(const std::string& text);

    // Liest den Katalog aus einer Datei. Wirft KonfigurationsFehler, wenn die Datei nicht lesbar ist,
    // und ParseFehler wie ausJson.
    static Katalog ausDatei(const std::string& pfad);

    // Alle gültigen Wertpapiere, in der Reihenfolge der Datei
    const std::vector<Wertpapier>& wertpapiere() const;

    // Alle fehlerhaften Einträge (leer, wenn alles in Ordnung ist)
    const std::vector<KatalogFehler>& fehler() const;

    // Sucht ein Wertpapier nach Symbol. Liefert nullptr, wenn es das Symbol nicht gibt.
    // Der Zeiger bleibt gültig, solange der Katalog existiert und nicht verändert wird.
    const Wertpapier* finde(const std::string& symbol) const;

private:
    // Privater Konstruktor: Kataloge entstehen nur über ausJson oder ausDatei
    Katalog() = default;

    // Die gültigen Wertpapiere
    std::vector<Wertpapier> wertpapiere_;
    // Die übersprungenen Einträge mit Begründung
    std::vector<KatalogFehler> fehler_;
};
