// Eigene Header-Datei mit der Klasse Risikokennzahlen
#include "risikokennzahlen.hpp"

// std::max für die True Range und den Höchststand
#include <algorithm>
// std::sqrt, std::log, std::abs
#include <cmath>
// std::invalid_argument für Fehler
#include <stdexcept>
// std::to_string für Fehlermeldungen
#include <string>

// Konstruktor: speichert eine Kopie der Kursreihe (sie ist laut E1 bereits gültig)
Risikokennzahlen::Risikokennzahlen(const Kursreihe& reihe) : reihe_(reihe) {}

// Prüft, dass alle Schlusskurse positiv sind (sonst sind Renditen und Drawdown nicht definiert)
void Risikokennzahlen::pruefePositiveKurse() const {
    // Jede Kerze durchgehen
    for (std::size_t i = 0; i < reihe_.groesse(); ++i) {
        // Ein Kurs von 0 oder darunter würde zu Division durch 0 oder ln(0) führen
        if (reihe_[i].close <= 0.0) {
            // Fehler mit Position und Wert
            throw std::invalid_argument("Close price must be > 0 for returns and drawdown, got " +
                                        std::to_string(reihe_[i].close) + " at index " + std::to_string(i));
        }
    }
}

// Bollinger-Bänder: SMA plus/minus k Standardabweichungen im selben Fenster
BollingerErgebnis Risikokennzahlen::bollinger(int periode, double k) const {
    // Ein negativer Faktor würde oberes und unteres Band vertauschen
    if (k < 0.0) {
        // Fehler mit dem ungültigen Wert
        throw std::invalid_argument("Bollinger: k must be >= 0, got " + std::to_string(k));
    }
    // Mittleres Band = SMA aus E3; prüft dabei auch Periode und Länge und wirft bei Fehlern
    const Indikatorreihe mitte = Trendindikatoren(reihe_).sma(periode);
    // Periode als Index-Typ (ab hier ist sie sicher > 0)
    const std::size_t n = static_cast<std::size_t>(periode);

    // Ergebnisobjekt anlegen
    BollingerErgebnis ergebnis;
    // Mittleres Band übernehmen
    ergebnis.mitte = mitte;
    // Oberes und unteres Band: gleiche Länge, zunächst "nicht verfügbar"
    ergebnis.oben.assign(mitte.size(), std::nullopt);
    ergebnis.unten.assign(mitte.size(), std::nullopt);

    // Für jeden Tag, an dem der SMA verfügbar ist (ab Index n - 1)
    for (std::size_t i = n - 1; i < reihe_.groesse(); ++i) {
        // Mittelwert des Fensters = SMA an dieser Stelle
        const double mittelwert = *mitte[i];
        // Summe der quadrierten Abweichungen vom Mittelwert
        double quadratsumme = 0.0;
        // Die n Kurse des Fensters durchgehen (von i - n + 1 bis i)
        for (std::size_t j = i + 1 - n; j <= i; ++j) {
            // Abweichung des Kurses vom Mittelwert
            const double abweichung = reihe_[j].close - mittelwert;
            // Quadrat aufaddieren
            quadratsumme += abweichung * abweichung;
        }
        // Standardabweichung der Grundgesamtheit (geteilt durch n, Bollinger-Konvention)
        const double stdabw = std::sqrt(quadratsumme / static_cast<double>(n));
        // Oberes Band
        ergebnis.oben[i] = mittelwert + k * stdabw;
        // Unteres Band
        ergebnis.unten[i] = mittelwert - k * stdabw;
    }
    // Alle drei Bänder zurückgeben
    return ergebnis;
}

// ATR: geglätteter Durchschnitt der True Range nach Wilder
Indikatorreihe Risikokennzahlen::atr(int periode) const {
    // Periode muss positiv sein
    if (periode <= 0) {
        // Fehler mit dem ungültigen Wert
        throw std::invalid_argument("ATR: period must be > 0, got " + std::to_string(periode));
    }
    // Periode als Index-Typ
    const std::size_t n = static_cast<std::size_t>(periode);
    // Für den ersten ATR-Wert braucht man n True-Range-Werte, also n Kerzen
    if (reihe_.groesse() < n) {
        // Fehler mit benötigter und vorhandener Anzahl
        throw std::invalid_argument("ATR: series too short, need " + std::to_string(n) + " values, got " +
                                    std::to_string(reihe_.groesse()));
    }
    // Periode als Kommazahl für die Durchschnitte
    const double nd = static_cast<double>(periode);
    // Ergebnis anlegen, zunächst überall "nicht verfügbar"
    Indikatorreihe ergebnis(reihe_.groesse(), std::nullopt);

    // Lambda: True Range der Kerze i
    auto trueRange = [this](std::size_t i) {
        // Aktuelle Kerze
        const Kerze& k = reihe_[i];
        // Spanne des Tages
        const double spanne = k.high - k.low;
        // Am ersten Tag gibt es keinen Vortag: True Range = Spanne
        if (i == 0) {
            // Nur die Tagesspanne
            return spanne;
        }
        // Schlusskurs des Vortags (berücksichtigt Kurslücken über Nacht)
        const double vortag = reihe_[i - 1].close;
        // Größter Wert aus Spanne, |High - Vortag| und |Low - Vortag|
        return std::max({spanne, std::abs(k.high - vortag), std::abs(k.low - vortag)});
    };

    // Summe der ersten n True-Range-Werte
    double summe = 0.0;
    // Index 0 bis n - 1 aufsummieren
    for (std::size_t i = 0; i < n; ++i) {
        // True Range des Tages addieren
        summe += trueRange(i);
    }
    // Erster ATR-Wert am Index n - 1: einfacher Durchschnitt
    double atr = summe / nd;
    // Eintragen
    ergebnis[n - 1] = atr;
    // Danach Wilder-Glättung
    for (std::size_t i = n; i < reihe_.groesse(); ++i) {
        // Neuer ATR = (alter ATR * (n - 1) + heutige True Range) / n
        atr = (atr * (nd - 1.0) + trueRange(i)) / nd;
        // Eintragen
        ergebnis[i] = atr;
    }
    // Fertige ATR-Reihe zurückgeben
    return ergebnis;
}

// Einfache Renditen: prozentuale Änderung gegenüber dem Vortag (als Dezimalzahl, 0,1 = +10 %)
Indikatorreihe Risikokennzahlen::einfacheRenditen() const {
    // Kurse müssen positiv sein, sonst Division durch 0
    pruefePositiveKurse();
    // Ergebnis mit einem Eintrag pro Kerze, zunächst "nicht verfügbar"
    Indikatorreihe ergebnis(reihe_.groesse(), std::nullopt);
    // Ab dem zweiten Tag gibt es einen Vortag
    for (std::size_t i = 1; i < reihe_.groesse(); ++i) {
        // Rendite = heute / gestern - 1
        ergebnis[i] = reihe_[i].close / reihe_[i - 1].close - 1.0;
    }
    // Index 0 bleibt nullopt; bei einer einzigen Kerze ist die ganze Reihe nullopt
    return ergebnis;
}

// Logarithmische Renditen: natürlicher Logarithmus des Kursverhältnisses
Indikatorreihe Risikokennzahlen::logRenditen() const {
    // Kurse müssen positiv sein, sonst ist der Logarithmus nicht definiert
    pruefePositiveKurse();
    // Ergebnis mit einem Eintrag pro Kerze, zunächst "nicht verfügbar"
    Indikatorreihe ergebnis(reihe_.groesse(), std::nullopt);
    // Ab dem zweiten Tag
    for (std::size_t i = 1; i < reihe_.groesse(); ++i) {
        // Log-Rendite = ln(heute / gestern)
        ergebnis[i] = std::log(reihe_[i].close / reihe_[i - 1].close);
    }
    // Fertige Reihe zurückgeben
    return ergebnis;
}

// Annualisierte Volatilität aus den Log-Renditen
std::optional<double> Risikokennzahlen::annualisierteVolatilitaet(int handelstage) const {
    // Die Anzahl der Handelstage pro Jahr muss positiv sein
    if (handelstage <= 0) {
        // Fehler mit dem ungültigen Wert
        throw std::invalid_argument("Volatility: trading days must be > 0, got " + std::to_string(handelstage));
    }
    // Log-Renditen berechnen (prüft dabei die Kurse)
    const Indikatorreihe renditen = logRenditen();
    // Die verfügbaren Renditen ohne die nullopt-Einträge sammeln
    std::vector<double> werte;
    // Alle Einträge durchgehen
    for (const auto& r : renditen) {
        // Nur vorhandene Werte übernehmen
        if (r.has_value()) {
            // Wert anhängen
            werte.push_back(*r);
        }
    }
    // Die Stichproben-Standardabweichung braucht mindestens 2 Werte (Division durch n - 1)
    if (werte.size() < 2) {
        // Nicht definiert: z. B. bei einer oder zwei Kerzen
        return std::nullopt;
    }
    // Summe der Renditen für den Mittelwert
    double summe = 0.0;
    // Alle Renditen aufaddieren
    for (double w : werte) {
        // Addieren
        summe += w;
    }
    // Mittelwert der Renditen
    const double mittelwert = summe / static_cast<double>(werte.size());
    // Summe der quadrierten Abweichungen
    double quadratsumme = 0.0;
    // Für jede Rendite
    for (double w : werte) {
        // Abweichung quadrieren und aufaddieren
        quadratsumme += (w - mittelwert) * (w - mittelwert);
    }
    // Stichproben-Standardabweichung (geteilt durch n - 1) = tägliche Volatilität
    const double taeglich = std::sqrt(quadratsumme / static_cast<double>(werte.size() - 1));
    // Hochrechnen auf ein Jahr: Volatilität wächst mit der Wurzel der Zeit
    return taeglich * std::sqrt(static_cast<double>(handelstage));
}

// Maximaler Drawdown: größter Rückgang vom bisherigen Höchststand
double Risikokennzahlen::maxDrawdown() const {
    // Kurse müssen positiv sein, weil durch den Höchststand geteilt wird
    pruefePositiveKurse();
    // Leere Reihe: kein Verlust möglich
    if (reihe_.groesse() == 0) {
        // Definiert als 0
        return 0.0;
    }
    // Bisheriger Höchststand, beginnt mit dem ersten Kurs
    double hoechststand = reihe_[0].close;
    // Größter bisher gefundener Drawdown
    double maximal = 0.0;
    // Alle Kurse durchgehen
    for (std::size_t i = 0; i < reihe_.groesse(); ++i) {
        // Höchststand aktualisieren, falls der Kurs höher ist
        hoechststand = std::max(hoechststand, reihe_[i].close);
        // Relativer Rückgang vom Höchststand (0, wenn der Kurs auf dem Höchststand ist)
        const double drawdown = (hoechststand - reihe_[i].close) / hoechststand;
        // Den größten Rückgang merken
        maximal = std::max(maximal, drawdown);
    }
    // Ergebnis zwischen 0 und 1 (z. B. 0,5 = Verlust von 50 %)
    return maximal;
}
