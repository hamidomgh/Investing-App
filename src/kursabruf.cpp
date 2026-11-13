// Eigene Header-Datei mit ladeAlleKurse
#include "kursabruf.hpp"

// QuellenFehler: Basisklasse aller Fehler der Datenquellen
#include "quellenfehler.hpp"

// std::move
#include <utility>

// Lädt die Kurse aller Wertpapiere; Fehler einzelner Wertpapiere werden gesammelt
Ladeergebnis ladeAlleKurse(IPriceSource& quelle, const std::vector<Wertpapier>& wertpapiere) {
    // Ergebnis mit zwei leeren Listen
    Ladeergebnis ergebnis;
    // Jedes Wertpapier einzeln laden
    for (const Wertpapier& wp : wertpapiere) {
        // Fehler dieses Wertpapiers abfangen
        try {
            // Kurse über die Schnittstelle laden (welcher Anbieter, ist hier egal)
            Kursreihe kurse = quelle.ladeKurse(wp);
            // Erfolg merken; std::move übernimmt die Kursreihe ohne Kopie
            ergebnis.erfolgreich.push_back({wp, std::move(kurse)});
        } catch (const QuellenFehler& f) {
            // Fehler merken und mit dem nächsten Wertpapier weitermachen
            ergebnis.fehler.push_back({wp.symbol, f.what()});
        }
    }
    // Beide Listen zurückgeben
    return ergebnis;
}
