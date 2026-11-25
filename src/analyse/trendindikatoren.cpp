// Eigene Header-Datei mit den Deklarationen der Klasse Trendindikatoren
#include "trendindikatoren.hpp"

// std::invalid_argument für Fehler bei ungültigen Perioden oder zu kurzen Reihen
#include <stdexcept>
// std::to_string, um Zahlen in Fehlermeldungen einzubauen
#include <string>

// Konstruktor: liest einmal alle Schlusskurse aus der Kursreihe, damit jede Berechnung darauf zugreifen kann
Trendindikatoren::Trendindikatoren(const Kursreihe& reihe) {
    // Platz für alle Schlusskurse im Voraus reservieren (vermeidet wiederholtes Vergrößern des Vectors)
    schlusskurse_.reserve(reihe.groesse());
    // Jede Kerze der Reihe durchgehen (Index von 0 bis Anzahl - 1)
    for (std::size_t i = 0; i < reihe.groesse(); ++i) {
        // Den Schlusskurs der Kerze an das Ende des Vectors anhängen
        schlusskurse_.push_back(reihe[i].close);
    }
}

// Prüft eine Periode und die Länge der Reihe; "name" ist der Indikator für die Fehlermeldung
void Trendindikatoren::pruefePeriode(int periode, std::size_t benoetigt, std::size_t vorhanden, const char* name) {
    // Eine Periode muss mindestens 1 sein, sonst gibt es nichts zu mitteln
    if (periode <= 0) {
        // Fehler werfen und den ungültigen Wert nennen
        throw std::invalid_argument(std::string(name) + ": period must be > 0, got " + std::to_string(periode));
    }
    // Die Reihe muss genug Kurse enthalten, damit mindestens ein Wert berechnet werden kann
    if (vorhanden < benoetigt) {
        // Fehler werfen und benötigte sowie vorhandene Anzahl nennen
        throw std::invalid_argument(std::string(name) + ": series too short, need " + std::to_string(benoetigt) +
                                    " values, got " + std::to_string(vorhanden));
    }
}

// SMA: Durchschnitt der letzten "periode" Schlusskurse für jeden Tag
Indikatorreihe Trendindikatoren::sma(int periode) const {
    // Periode und Länge prüfen: Für einen SMA(n) braucht man mindestens n Kurse
    pruefePeriode(periode, static_cast<std::size_t>(periode > 0 ? periode : 0), schlusskurse_.size(), "SMA");
    // Periode einmal in den Typ für Indizes umwandeln (size_t), damit Vergleiche ohne Warnung funktionieren
    const std::size_t n = static_cast<std::size_t>(periode);
    // Ergebnis mit genauso vielen Einträgen wie Kurse anlegen; alle sind zunächst "nicht verfügbar"
    Indikatorreihe ergebnis(schlusskurse_.size(), std::nullopt);
    // Laufende Summe der Kurse im aktuellen Fenster (gleitendes Fenster, damit nicht jedes Mal neu summiert wird)
    double summe = 0.0;
    // Alle Kurse von vorne nach hinten durchgehen
    for (std::size_t i = 0; i < schlusskurse_.size(); ++i) {
        // Den neuen Kurs zum Fenster hinzufügen
        summe += schlusskurse_[i];
        // Ist das Fenster größer als n geworden, den ältesten Kurs wieder herausnehmen
        if (i >= n) {
            // Der Kurs, der gerade aus dem Fenster fällt, liegt n Positionen zurück
            summe -= schlusskurse_[i - n];
        }
        // Erst ab Index n - 1 enthält das Fenster n Kurse: dann ist der SMA verfügbar
        if (i + 1 >= n) {
            // Durchschnitt = Summe geteilt durch Anzahl der Kurse im Fenster
            ergebnis[i] = summe / static_cast<double>(n);
        }
    }
    // Fertige SMA-Reihe zurückgeben
    return ergebnis;
}

// EMA über beliebige Werte: Startwert = SMA der ersten n Werte, danach exponentielle Glättung
Indikatorreihe Trendindikatoren::emaUeber(const std::vector<double>& werte, int periode) {
    // Periode und Länge prüfen: Für den Startwert braucht man mindestens n Werte
    pruefePeriode(periode, static_cast<std::size_t>(periode > 0 ? periode : 0), werte.size(), "EMA");
    // Periode als Index-Typ
    const std::size_t n = static_cast<std::size_t>(periode);
    // Glättungsfaktor alpha = 2 / (n + 1); je größer n, desto langsamer reagiert die EMA
    const double alpha = 2.0 / (static_cast<double>(periode) + 1.0);
    // Ergebnis anlegen, zunächst überall "nicht verfügbar"
    Indikatorreihe ergebnis(werte.size(), std::nullopt);
    // Summe der ersten n Werte für den Startwert
    double summe = 0.0;
    // Die ersten n Werte aufsummieren
    for (std::size_t i = 0; i < n; ++i) {
        // Wert i zur Summe addieren
        summe += werte[i];
    }
    // Startwert der EMA (Index n - 1) ist der einfache Durchschnitt der ersten n Werte
    double ema = summe / static_cast<double>(n);
    // Startwert an der Stelle n - 1 eintragen
    ergebnis[n - 1] = ema;
    // Ab dem nächsten Wert die EMA rekursiv fortschreiben
    for (std::size_t i = n; i < werte.size(); ++i) {
        // Neue EMA = alpha * neuer Wert + (1 - alpha) * alte EMA
        ema = alpha * werte[i] + (1.0 - alpha) * ema;
        // Wert eintragen
        ergebnis[i] = ema;
    }
    // Fertige EMA-Reihe zurückgeben
    return ergebnis;
}

// EMA auf den Schlusskursen: ruft die allgemeine Hilfsfunktion auf
Indikatorreihe Trendindikatoren::ema(int periode) const {
    // Die eigentliche Berechnung steckt in emaUeber, damit der MACD sie wiederverwenden kann
    return emaUeber(schlusskurse_, periode);
}

// MACD: Differenz zweier EMAs, dazu deren geglättete Signallinie und das Histogramm
MacdErgebnis Trendindikatoren::macd(int schnell, int langsam, int signal) const {
    // Alle drei Perioden müssen positiv sein
    if (schnell <= 0 || langsam <= 0 || signal <= 0) {
        // Fehler mit allen drei Werten, damit der Aufrufer sieht, welcher falsch ist
        throw std::invalid_argument("MACD: periods must be > 0, got " + std::to_string(schnell) + ", " +
                                    std::to_string(langsam) + ", " + std::to_string(signal));
    }
    // Die schnelle EMA muss eine kürzere Periode haben als die langsame, sonst ist der MACD sinnlos
    if (schnell >= langsam) {
        // Fehler werfen und beide Perioden nennen
        throw std::invalid_argument("MACD: fast period (" + std::to_string(schnell) +
                                    ") must be smaller than slow period (" + std::to_string(langsam) + ")");
    }
    // Die MACD-Linie gibt es ab Index langsam - 1; die Signallinie braucht danach noch signal Werte.
    // Also braucht man mindestens langsam + signal - 1 Kurse, damit alle drei Reihen einen Wert haben.
    const std::size_t benoetigt = static_cast<std::size_t>(langsam + signal - 1);
    // Länge prüfen (die Periode selbst wurde oben schon geprüft)
    pruefePeriode(langsam, benoetigt, schlusskurse_.size(), "MACD");

    // Schnelle EMA der Schlusskurse berechnen
    const Indikatorreihe emaSchnell = emaUeber(schlusskurse_, schnell);
    // Langsame EMA der Schlusskurse berechnen
    const Indikatorreihe emaLangsam = emaUeber(schlusskurse_, langsam);

    // Ergebnisobjekt anlegen
    MacdErgebnis ergebnis;
    // Alle drei Reihen bekommen so viele Einträge wie Kurse, zunächst "nicht verfügbar"
    ergebnis.linie.assign(schlusskurse_.size(), std::nullopt);
    ergebnis.signal.assign(schlusskurse_.size(), std::nullopt);
    ergebnis.histogramm.assign(schlusskurse_.size(), std::nullopt);

    // Ab diesem Index existiert die langsame EMA und damit die MACD-Linie
    const std::size_t start = static_cast<std::size_t>(langsam - 1);
    // Die verfügbaren Werte der MACD-Linie ohne Lücken sammeln (Eingabe für die Signal-EMA)
    std::vector<double> linienwerte;
    // Platz reservieren: ab start bis zum Ende
    linienwerte.reserve(schlusskurse_.size() - start);
    // Für jeden Tag ab start die MACD-Linie berechnen
    for (std::size_t i = start; i < schlusskurse_.size(); ++i) {
        // MACD-Linie = schnelle EMA minus langsame EMA (beide sind ab start sicher vorhanden)
        const double wert = *emaSchnell[i] - *emaLangsam[i];
        // In die Ergebnisreihe an der richtigen Stelle eintragen
        ergebnis.linie[i] = wert;
        // Zusätzlich in die lückenlose Liste für die Signallinie aufnehmen
        linienwerte.push_back(wert);
    }

    // Signallinie = EMA der MACD-Linie; Index 0 dieser Reihe entspricht dem Index start der Kursreihe
    const Indikatorreihe signalwerte = emaUeber(linienwerte, signal);
    // Die Signalwerte zurück auf die Positionen der Kursreihe übertragen
    for (std::size_t j = 0; j < signalwerte.size(); ++j) {
        // Nur verfügbare Signalwerte übernehmen (vor der Einschwingphase bleibt nullopt stehen)
        if (signalwerte[j].has_value()) {
            // Position in der Kursreihe = start + j
            const std::size_t i = start + j;
            // Signalwert eintragen
            ergebnis.signal[i] = signalwerte[j];
            // Histogramm = MACD-Linie minus Signallinie
            ergebnis.histogramm[i] = *ergebnis.linie[i] - *signalwerte[j];
        }
    }
    // Alle drei Reihen zurückgeben
    return ergebnis;
}

// RSI nach Wilder: Verhältnis der durchschnittlichen Gewinne zu den durchschnittlichen Verlusten
Indikatorreihe Trendindikatoren::rsi(int periode) const {
    // Für n Kursänderungen braucht man n + 1 Kurse
    pruefePeriode(periode, static_cast<std::size_t>(periode > 0 ? periode + 1 : 0), schlusskurse_.size(), "RSI");
    // Periode als Index-Typ
    const std::size_t n = static_cast<std::size_t>(periode);
    // Periode als Kommazahl für die Durchschnittsrechnung
    const double nd = static_cast<double>(periode);
    // Ergebnis anlegen, zunächst überall "nicht verfügbar"
    Indikatorreihe ergebnis(schlusskurse_.size(), std::nullopt);

    // Hilfsfunktion (Lambda): rechnet die Durchschnitte in einen RSI-Wert um
    auto rsiAus = [](double avgGewinn, double avgVerlust) {
        // Keine Gewinne und keine Verluste (konstante Kurse): neutral, per Definition 50
        if (avgGewinn == 0.0 && avgVerlust == 0.0) {
            // Neutraler Wert
            return 50.0;
        }
        // Keine Verluste: RS wäre unendlich, der RSI ist dann 100
        if (avgVerlust == 0.0) {
            // Maximaler Wert
            return 100.0;
        }
        // RS = durchschnittlicher Gewinn / durchschnittlicher Verlust
        const double rs = avgGewinn / avgVerlust;
        // RSI = 100 - 100 / (1 + RS), liegt immer zwischen 0 und 100
        return 100.0 - 100.0 / (1.0 + rs);
    };

    // Summe der Gewinne in den ersten n Änderungen
    double summeGewinn = 0.0;
    // Summe der Verluste (als positive Zahl) in den ersten n Änderungen
    double summeVerlust = 0.0;
    // Die ersten n Kursänderungen (von Kurs 1 bis Kurs n) durchgehen
    for (std::size_t i = 1; i <= n; ++i) {
        // Änderung gegenüber dem Vortag
        const double aenderung = schlusskurse_[i] - schlusskurse_[i - 1];
        // Positive Änderung zählt als Gewinn
        if (aenderung > 0.0) {
            // Gewinn aufaddieren
            summeGewinn += aenderung;
        } else {
            // Negative Änderung zählt als Verlust; das Minus macht daraus eine positive Zahl
            summeVerlust -= aenderung;
        }
    }
    // Erster Durchschnittsgewinn: einfacher Durchschnitt der ersten n Änderungen
    double avgGewinn = summeGewinn / nd;
    // Erster Durchschnittsverlust: ebenso
    double avgVerlust = summeVerlust / nd;
    // Der erste RSI-Wert steht am Index n (nach n Änderungen)
    ergebnis[n] = rsiAus(avgGewinn, avgVerlust);

    // Ab dem nächsten Tag nach Wilder glätten
    for (std::size_t i = n + 1; i < schlusskurse_.size(); ++i) {
        // Änderung gegenüber dem Vortag
        const double aenderung = schlusskurse_[i] - schlusskurse_[i - 1];
        // Gewinn dieses Tages (0, wenn der Kurs gefallen ist)
        const double gewinn = aenderung > 0.0 ? aenderung : 0.0;
        // Verlust dieses Tages als positive Zahl (0, wenn der Kurs gestiegen ist)
        const double verlust = aenderung < 0.0 ? -aenderung : 0.0;
        // Wilder-Glättung: neuer Durchschnitt = (alter Durchschnitt * (n - 1) + neuer Wert) / n
        avgGewinn = (avgGewinn * (nd - 1.0) + gewinn) / nd;
        // Dieselbe Glättung für die Verluste
        avgVerlust = (avgVerlust * (nd - 1.0) + verlust) / nd;
        // RSI für diesen Tag eintragen
        ergebnis[i] = rsiAus(avgGewinn, avgVerlust);
    }
    // Fertige RSI-Reihe zurückgeben
    return ergebnis;
}
