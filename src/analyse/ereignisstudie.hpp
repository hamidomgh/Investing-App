// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kursreihe und Wertpapier (E1)
#include "kursdaten.hpp"
// Zinsentscheidung (E6)
#include "zinsentscheidung.hpp"

// std::chrono::sys_days für den Reaktionstag
#include <chrono>
// std::vector für Listen
#include <vector>

// Ergebnis für eine Zinsentscheidung: wie hat der Kurs am folgenden Handelstag reagiert?
struct Ereignis {
    // Die Zinsentscheidung, um die es geht
    Zinsentscheidung entscheidung;
    // Erster Handelstag nach dem Tag der Entscheidung (fällt sie auf einen Nicht-Handelstag: der nächste Handelstag)
    std::chrono::sys_days reaktionstag;
    // Schlusskurs am Handelstag vor dem Reaktionstag (Ausgangswert)
    double kursVorher;
    // Schlusskurs am Reaktionstag
    double kursNachher;
    // Einfache Rendite des Reaktionstags: kursNachher / kursVorher - 1 (0,01 = +1 %)
    double veraenderung;
};

// Ereignisstudie (E6): Kursreaktion eines Wertpapiers nach den Zinsentscheidungen der Zentralbank seines Landes
class Ereignisstudie {
public:
    // Konstruktor: übernimmt alle Zinsentscheidungen (beliebig viele Zentralbanken) und sortiert sie nach Datum.
    // Ein Objekt kann danach für beliebig viele Wertpapiere verwendet werden.
    explicit Ereignisstudie(std::vector<Zinsentscheidung> entscheidungen);

    // Wertet ein Wertpapier aus: berücksichtigt nur Entscheidungen, deren Land dem Land des Wertpapiers entspricht.
    // - Reaktionstag = erster Handelstag nach dem Entscheidungstag (Nicht-Handelstage -> nächster Handelstag)
    // - Entscheidungen ohne Handelstag davor oder danach in der Kursreihe werden übersprungen
    // - Kein passendes Land: leere Liste statt Fehler
    // Ergebnis nach Datum der Entscheidung sortiert. Wirft std::invalid_argument, wenn ein benötigter Kurs <= 0 ist.
    std::vector<Ereignis> auswerten(const Wertpapier& wertpapier, const Kursreihe& reihe) const;

private:
    // Alle Zinsentscheidungen, aufsteigend nach Datum
    std::vector<Zinsentscheidung> entscheidungen_;
};
