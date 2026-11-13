// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Zentralbank aus dem Katalog
#include "zentralbank.hpp"
// Zinsentscheidung (E6)
#include "zinsentscheidung.hpp"

// std::vector
#include <vector>

// Schnittstelle (Interface) für Zinsdaten-Quellen (E6), das Gegenstück zu IPriceSource für Kurse.
// Eine Implementierung (z. B. FredQuelle) lädt die Zinsdaten über einen IHttpClient von einer Web-API.
// Die Ereignisstudie kennt nur die gelieferten Zinsentscheidungen, nicht den Anbieter.
class IRateSource {
public:
    // Virtueller Destruktor, damit abgeleitete Klassen über einen Basiszeiger richtig gelöscht werden
    virtual ~IRateSource() = default;

    // Lädt alle Zinsentscheidungen einer Zentralbank, aufsteigend nach Datum.
    // Für jede Entscheidung gibt es einen Eintrag PRO LAND der Zentralbank (bei der EZB also für jedes
    // Euro-Land), damit die Ereignisstudie sie über das Land des Wertpapiers zuordnen kann.
    // Wirft eine Unterklasse von QuellenFehler, wenn das nicht gelingt (wie IPriceSource).
    virtual std::vector<Zinsentscheidung> ladeEntscheidungen(const Zentralbank& bank) = 0;
};
