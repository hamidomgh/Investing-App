// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kursreihe und Wertpapier (E1)
#include "kursdaten.hpp"
// Schnittstelle der Kursdaten-Quelle (E2)
#include "price_source.hpp"

// std::string
#include <string>
// std::vector
#include <vector>

// Ein Wertpapier mit seinen erfolgreich geladenen Kursen
struct GeladenesWertpapier {
    // Das Wertpapier aus dem Katalog
    Wertpapier wertpapier;
    // Seine Kurse
    Kursreihe kurse;
};

// Ein Wertpapier, dessen Kurse nicht geladen werden konnten
struct Ladefehler {
    // Symbol des Wertpapiers
    std::string symbol;
    // Fehlermeldung der Quelle (z. B. "HTTP 404", "quota exhausted", "invalid JSON")
    std::string meldung;
};

// Ergebnis des Ladens für viele Wertpapiere: was geklappt hat und was nicht
struct Ladeergebnis {
    // Erfolgreich geladene Wertpapiere, in der Reihenfolge der Eingabe
    std::vector<GeladenesWertpapier> erfolgreich;
    // Fehlgeschlagene Wertpapiere mit Begründung
    std::vector<Ladefehler> fehler;
};

// Lädt die Kurse aller Wertpapiere über die Quelle.
// Scheitert ein Wertpapier mit einem QuellenFehler (Netz, HTTP, Kontingent, JSON ...), wird das in "fehler"
// vermerkt und mit dem nächsten weitergemacht: ein fehlerhaftes Wertpapier verhindert nicht die übrigen (DoD E2).
// Andere Exceptions (Programmierfehler) werden NICHT abgefangen, damit sie nicht unbemerkt bleiben.
Ladeergebnis ladeAlleKurse(IPriceSource& quelle, const std::vector<Wertpapier>& wertpapiere);
