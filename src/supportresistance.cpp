// Eigene Header-Datei mit der Klasse SupportResistance
#include "supportresistance.hpp"

// std::sort für das Sortieren der Extrema
#include <algorithm>
// std::abs für den Abstand zum Mittelwert
#include <cmath>
// std::invalid_argument für Fehler
#include <stdexcept>
// std::to_string für Fehlermeldungen
#include <string>

// Konstruktor: speichert eine Kopie der Kursreihe (laut E1 bereits gültig)
SupportResistance::SupportResistance(const Kursreihe& reihe) : reihe_(reihe) {}

// Findet lokale Maxima. Idee: ein Plateau (gleiche Werte nebeneinander) wird als ein Punkt behandelt.
// Ein Plateau von a bis b ist ein Maximum, wenn die "fenster" Werte vor a und nach b alle kleiner sind.
std::vector<std::size_t> SupportResistance::findeMaxima(const std::vector<double>& werte, std::size_t fenster) {
    // Liste der gefundenen Indizes
    std::vector<std::size_t> ergebnis;
    // Anzahl der Werte
    const std::size_t anzahl = werte.size();
    // Startindex des aktuellen Plateaus
    std::size_t a = 0;
    // Alle Plateaus von links nach rechts durchgehen
    while (a < anzahl) {
        // Ende des Plateaus suchen: b läuft weiter, solange der nächste Wert gleich ist
        std::size_t b = a;
        // Solange es einen nächsten Wert gibt und er gleich ist, gehört er zum Plateau
        while (b + 1 < anzahl && werte[b + 1] == werte[a]) {
            // Plateau um einen Tag verlängern
            ++b;
        }
        // Volles Fenster links (a - fenster >= 0) und rechts (b + fenster < anzahl) nötig, sonst Rand
        const bool vollesFenster = a >= fenster && b + fenster < anzahl;
        // Nur prüfen, wenn das Fenster vollständig in der Reihe liegt
        if (vollesFenster) {
            // Annahme: es ist ein Maximum, bis ein Nachbar das Gegenteil zeigt
            bool istMaximum = true;
            // Die "fenster" Nachbarn links und rechts des Plateaus prüfen (Abstand 1 bis fenster)
            for (std::size_t d = 1; d <= fenster && istMaximum; ++d) {
                // Linker Nachbar im Abstand d vor dem Plateau muss kleiner sein
                if (werte[a - d] >= werte[a]) {
                    // Nicht kleiner: kein Maximum
                    istMaximum = false;
                }
                // Rechter Nachbar im Abstand d nach dem Plateau muss kleiner sein
                if (werte[b + d] >= werte[a]) {
                    // Nicht kleiner: kein Maximum
                    istMaximum = false;
                }
            }
            // Wenn alle Nachbarn kleiner sind, den ersten Tag des Plateaus merken
            if (istMaximum) {
                // Index anhängen
                ergebnis.push_back(a);
            }
        }
        // Mit dem nächsten Plateau weitermachen (der Tag direkt nach b)
        a = b + 1;
    }
    // Gefundene Indizes zurückgeben (aufsteigend, weil von links nach rechts gesucht wurde)
    return ergebnis;
}

// Lokale Hochs (aus den High-Werten) und Tiefs (aus den Low-Werten)
std::vector<Extremum> SupportResistance::lokaleExtrema(int fenster) const {
    // Fenster muss mindestens 1 Tag sein
    if (fenster <= 0) {
        // Fehler mit dem ungültigen Wert
        throw std::invalid_argument("Extrema: window must be > 0, got " + std::to_string(fenster));
    }
    // Fenster als Index-Typ
    const std::size_t w = static_cast<std::size_t>(fenster);
    // High-Werte aller Kerzen
    std::vector<double> highs;
    // Negierte Low-Werte aller Kerzen (Minimum von Low = Maximum von -Low)
    std::vector<double> negierteLows;
    // Platz reservieren
    highs.reserve(reihe_.groesse());
    negierteLows.reserve(reihe_.groesse());
    // Werte aus den Kerzen holen
    for (std::size_t i = 0; i < reihe_.groesse(); ++i) {
        // High übernehmen
        highs.push_back(reihe_[i].high);
        // Low negiert übernehmen
        negierteLows.push_back(-reihe_[i].low);
    }

    // Ergebnisliste
    std::vector<Extremum> ergebnis;
    // Alle lokalen Hochs eintragen
    for (std::size_t i : findeMaxima(highs, w)) {
        // Hoch mit Index und High-Preis
        ergebnis.push_back({i, reihe_[i].high, ExtremumArt::Hoch});
    }
    // Alle lokalen Tiefs eintragen
    for (std::size_t i : findeMaxima(negierteLows, w)) {
        // Tief mit Index und Low-Preis
        ergebnis.push_back({i, reihe_[i].low, ExtremumArt::Tief});
    }
    // Hochs und Tiefs gemeinsam nach Index sortieren (zeitliche Reihenfolge)
    std::sort(ergebnis.begin(), ergebnis.end(),
              [](const Extremum& x, const Extremum& y) { return x.index < y.index; });
    // Fertige Liste zurückgeben
    return ergebnis;
}

// Fasst Extrema zu Niveaus zusammen und stuft sie als Support oder Resistance ein
std::vector<Niveau> SupportResistance::niveaus(int fenster, double toleranz) const {
    // Eine negative Toleranz ergibt keinen Sinn
    if (toleranz < 0.0) {
        // Fehler mit dem ungültigen Wert
        throw std::invalid_argument("Levels: tolerance must be >= 0, got " + std::to_string(toleranz));
    }
    // Lokale Extrema bestimmen (prüft dabei auch das Fenster)
    const std::vector<Extremum> extrema = lokaleExtrema(fenster);
    // Nur die Preise werden für die Niveaus gebraucht
    std::vector<double> preise;
    // Platz reservieren
    preise.reserve(extrema.size());
    // Preise sammeln
    for (const Extremum& e : extrema) {
        // Preis anhängen
        preise.push_back(e.preis);
    }
    // Aufsteigend sortieren, damit nahe Preise nebeneinander liegen
    std::sort(preise.begin(), preise.end());

    // Ergebnisliste
    std::vector<Niveau> ergebnis;
    // Ohne Extrema gibt es keine Niveaus (z. B. leere oder zu kurze Reihe)
    if (preise.empty()) {
        // Leere Liste zurückgeben, kein Fehler
        return ergebnis;
    }
    // Letzter Schlusskurs: Grenze zwischen Support und Resistance (Reihe ist hier sicher nicht leer)
    const double letzterKurs = reihe_[reihe_.groesse() - 1].close;

    // Summe der Preise im aktuellen Niveau
    double summe = 0.0;
    // Anzahl der Preise im aktuellen Niveau
    int anzahl = 0;
    // Lambda: schließt das aktuelle Niveau ab und hängt es an das Ergebnis an
    auto abschliessen = [&]() {
        // Preis des Niveaus = Mittelwert seiner Preise
        const double mittel = summe / anzahl;
        // Unter oder auf dem letzten Kurs: Support, darüber: Resistance
        const NiveauArt art = mittel <= letzterKurs ? NiveauArt::Support : NiveauArt::Resistance;
        // Niveau anhängen
        ergebnis.push_back({mittel, anzahl, art});
    };
    // Alle Preise der Reihe nach durchgehen (aufsteigend)
    for (double p : preise) {
        // Gibt es schon ein Niveau, prüfen, ob p nahe genug an dessen Mittelwert liegt
        if (anzahl > 0) {
            // Aktueller Mittelwert des Niveaus
            const double mittel = summe / anzahl;
            // Relativer Abstand größer als die Toleranz: aktuelles Niveau abschließen, neues beginnen
            if (std::abs(p - mittel) > toleranz * std::abs(mittel)) {
                // Altes Niveau speichern
                abschliessen();
                // Zähler für das neue Niveau zurücksetzen
                summe = 0.0;
                anzahl = 0;
            }
        }
        // Preis zum (alten oder neuen) Niveau hinzufügen
        summe += p;
        // Berührungszahl erhöhen
        ++anzahl;
    }
    // Das letzte Niveau ist noch offen: abschließen
    abschliessen();
    // Niveaus sind aufsteigend, weil die Preise sortiert durchlaufen wurden
    return ergebnis;
}

// Klassische Pivot-Punkte aus High, Low und Close einer Kerze
PivotPunkte SupportResistance::pivotAus(const Kerze& kerze) {
    // Pivot-Punkt = Durchschnitt aus High, Low und Close
    const double p = (kerze.high + kerze.low + kerze.close) / 3.0;
    // Tagesspanne
    const double spanne = kerze.high - kerze.low;
    // Alle fünf Werte nach den klassischen Formeln zurückgeben
    return PivotPunkte{
        p,                    // P
        2.0 * p - kerze.low,  // R1
        p + spanne,           // R2
        2.0 * p - kerze.high, // S1
        p - spanne,           // S2
    };
}

// Pivot-Punkte aus der letzten Kerze der Reihe
PivotPunkte SupportResistance::pivotPunkte() const {
    // Ohne Kerze gibt es keine Pivot-Punkte
    if (reihe_.groesse() == 0) {
        // Fehler mit verständlicher Meldung
        throw std::invalid_argument("Pivot points: series is empty");
    }
    // Pivot-Punkte der letzten Kerze berechnen
    return pivotAus(reihe_[reihe_.groesse() - 1]);
}
