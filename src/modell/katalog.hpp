// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Wertpapier (E1)
#include "kursdaten.hpp"
// Zentralbank (E6)
#include "zentralbank.hpp"

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
// Optional (E6) zusätzlich die Zentralbanken mit den Ländern, für die sie zuständig sind:
//
//   "zentralbanken": [
//       { "name": "EZB", "laender": ["Deutschland", "Frankreich", ...], "waehrung": "EUR", "fredSerie": "ECBDFR" }
//   ]
//
// "symbol" ist das Symbol, das angezeigt wird. Optional (F3) nennt "symbole" das Kürzel je Anbieter, z. B.
//   "symbole": {"alphavantage": "IBM", "twelvedata": "IBM"}
// Ohne "symbole" gilt "symbol" beim Standardanbieter Alpha Vantage (bei Alpha Vantage z. B. "MBG.DEX").
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

    // Nur die Wertpapiere, die der Anbieter liefert (F3), z. B. fuerAnbieter("twelvedata").
    // Wertpapiere ohne "symbole" gehören zum Standardanbieter "alphavantage".
    std::vector<Wertpapier> fuerAnbieter(const std::string& anbieter) const;

    // Alle fehlerhaften Einträge (leer, wenn alles in Ordnung ist)
    const std::vector<KatalogFehler>& fehler() const;

    // Alle gültigen Zentralbanken (E6); leer, wenn der Katalog keine "zentralbanken" enthält
    const std::vector<Zentralbank>& zentralbanken() const;

    // Fehlerhafte Einträge in "zentralbanken"; "position" zählt in dieser Liste, "symbol" ist der Name der Bank
    const std::vector<KatalogFehler>& zentralbankFehler() const;

    // Sucht ein Wertpapier nach Symbol. Liefert nullptr, wenn es das Symbol nicht gibt.
    // Der Zeiger bleibt gültig, solange der Katalog existiert und nicht verändert wird.
    const Wertpapier* finde(const std::string& symbol) const;

private:
    // Privater Konstruktor: Kataloge entstehen nur über ausJson oder ausDatei
    Katalog() = default;

    // Die gültigen Wertpapiere
    std::vector<Wertpapier> wertpapiere_;
    // Die gültigen Zentralbanken
    std::vector<Zentralbank> zentralbanken_;
    // Die übersprungenen Zentralbanken mit Begründung
    std::vector<KatalogFehler> zentralbankFehler_;
    // Die übersprungenen Einträge mit Begründung
    std::vector<KatalogFehler> fehler_;
};
