// Eigene Header-Datei mit ladeAlleEntscheidungen
#include "zinsabruf.hpp"

// QuellenFehler
#include "quellenfehler.hpp"

// Lädt alle Zentralbanken; Fehler einzelner Banken werden gesammelt
ZinsLadeergebnis ladeAlleEntscheidungen(IRateSource& quelle, const std::vector<Zentralbank>& banken) {
    // Ergebnis mit zwei leeren Listen
    ZinsLadeergebnis ergebnis;
    // Jede Zentralbank einzeln
    for (const Zentralbank& bank : banken) {
        // Fehler dieser Bank abfangen
        try {
            // Laden
            const std::vector<Zinsentscheidung> neu = quelle.ladeEntscheidungen(bank);
            // An die Gesamtliste anhängen
            ergebnis.entscheidungen.insert(ergebnis.entscheidungen.end(), neu.begin(), neu.end());
        } catch (const QuellenFehler& f) {
            // Fehler merken, weitermachen
            ergebnis.fehler.push_back({bank.name, f.what()});
        }
    }
    // Ergebnis
    return ergebnis;
}
