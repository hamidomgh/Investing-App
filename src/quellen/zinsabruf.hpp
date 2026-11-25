// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle der Zinsdaten-Quelle
#include "rate_source.hpp"

// std::string
#include <string>
// std::vector
#include <vector>

// Eine Zentralbank, deren Zinsdaten nicht geladen werden konnten
struct ZinsLadefehler {
    // Name der Zentralbank
    std::string zentralbank;
    // Fehlermeldung der Quelle
    std::string meldung;
};

// Ergebnis des Ladens für alle Zentralbanken
struct ZinsLadeergebnis {
    // Alle Entscheidungen aller erfolgreich geladenen Zentralbanken
    std::vector<Zinsentscheidung> entscheidungen;
    // Zentralbanken, bei denen es nicht geklappt hat
    std::vector<ZinsLadefehler> fehler;
};

// Lädt die Entscheidungen aller Zentralbanken. Scheitert eine Zentralbank mit einem QuellenFehler,
// wird das vermerkt und mit der nächsten weitergemacht (wie ladeAlleKurse in E2).
ZinsLadeergebnis ladeAlleEntscheidungen(IRateSource& quelle, const std::vector<Zentralbank>& banken);
