// Eigene Header-Datei mit der Klasse Ereignisstudie
#include "ereignisstudie.hpp"

// std::stable_sort und std::upper_bound
#include <algorithm>
// std::invalid_argument für Fehler
#include <stdexcept>
// std::to_string für Fehlermeldungen
#include <string>
// std::move
#include <utility>

// Konstruktor: übernimmt die Entscheidungen und sortiert sie nach Datum
Ereignisstudie::Ereignisstudie(std::vector<Zinsentscheidung> entscheidungen)
    : entscheidungen_(std::move(entscheidungen)) {
    // Nach Datum sortieren; stable_sort lässt Entscheidungen am selben Tag in der ursprünglichen Reihenfolge
    std::stable_sort(entscheidungen_.begin(), entscheidungen_.end(),
                     [](const Zinsentscheidung& a, const Zinsentscheidung& b) { return a.datum < b.datum; });
}

// Wertet die Kursreaktion eines Wertpapiers auf die Zinsentscheidungen seines Landes aus
std::vector<Ereignis> Ereignisstudie::auswerten(const Wertpapier& wertpapier, const Kursreihe& reihe) const {
    // Handelstage der Kursreihe als reine Kalendertage (die Uhrzeit der Kerze wird abgeschnitten)
    std::vector<std::chrono::sys_days> handelstage;
    // Platz reservieren
    handelstage.reserve(reihe.groesse());
    // Für jede Kerze den Tag bestimmen
    for (std::size_t i = 0; i < reihe.groesse(); ++i) {
        // floor<days> rundet den Zeitpunkt auf den Beginn des Tages ab
        handelstage.push_back(std::chrono::floor<std::chrono::days>(reihe[i].datum));
    }

    // Ergebnisliste
    std::vector<Ereignis> ergebnis;
    // Alle Entscheidungen der Reihe nach (sie sind nach Datum sortiert)
    for (const Zinsentscheidung& e : entscheidungen_) {
        // Nur die Zentralbank des Landes, in dem das Wertpapier gehandelt wird
        if (e.land != wertpapier.land) {
            // Andere Zentralbank: überspringen
            continue;
        }
        // Erster Handelstag, der STRENG NACH dem Entscheidungstag liegt.
        // upper_bound sucht in der sortierten Liste per Binärsuche (O(log N)).
        // Fällt die Entscheidung auf einen Samstag, ist das automatisch der Montag (nächster Handelstag).
        const auto it = std::upper_bound(handelstage.begin(), handelstage.end(), e.datum);
        // Kein Handelstag danach: Entscheidung liegt am oder nach dem Ende der Daten -> überspringen
        if (it == handelstage.end()) {
            // Außerhalb des Datenzeitraums
            continue;
        }
        // Position des Reaktionstags in der Kursreihe
        const std::size_t r = static_cast<std::size_t>(it - handelstage.begin());
        // Reaktionstag ist der erste Tag der Reihe: kein Kurs davor -> Entscheidung liegt vor den Daten
        if (r == 0) {
            // Außerhalb des Datenzeitraums
            continue;
        }
        // Schlusskurs am Handelstag vor dem Reaktionstag (bei einem Handelstag: am Tag der Entscheidung)
        const double vorher = reihe[r - 1].close;
        // Schlusskurs am Reaktionstag
        const double nachher = reihe[r].close;
        // Durch 0 oder einen negativen Kurs kann keine Rendite berechnet werden
        if (vorher <= 0.0) {
            // Fehler mit Wertpapier und Wert
            throw std::invalid_argument("Event study for " + wertpapier.symbol + ": close price must be > 0, got " +
                                        std::to_string(vorher));
        }
        // Ereignis mit allen Angaben anhängen
        ergebnis.push_back(Ereignis{e, handelstage[r], vorher, nachher, nachher / vorher - 1.0});
    }
    // Ergebnis zurückgeben (leer, wenn es keine passende Zentralbank gibt)
    return ergebnis;
}
