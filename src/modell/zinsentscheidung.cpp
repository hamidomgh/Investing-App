// Eigene Header-Datei mit der Struktur Zinsentscheidung
#include "zinsentscheidung.hpp"

// std::isfinite: prüft, ob eine Zahl weder unendlich noch NaN ist
#include <cmath>
// std::invalid_argument für Fehler
#include <stdexcept>
// std::to_string für Zahlen in Fehlermeldungen
#include <string>
// std::move: übernimmt Strings ohne Kopie
#include <utility>

// Anonymer Namespace: Hilfsfunktion nur in dieser Datei sichtbar
namespace {
// Formatiert einen Kalendertag als YYYY-MM-DD für Fehlermeldungen
std::string datumText(std::chrono::sys_days tag) {
    // Tag in Jahr, Monat und Tag zerlegen
    const std::chrono::year_month_day ymd{tag};
    // Jahr als Zahl
    const int jahr = static_cast<int>(ymd.year());
    // Monat als Zahl 1..12
    const unsigned monat = static_cast<unsigned>(ymd.month());
    // Tag als Zahl 1..31
    const unsigned t = static_cast<unsigned>(ymd.day());
    // Monat und Tag zweistellig mit führender Null zusammensetzen
    return std::to_string(jahr) + "-" + (monat < 10 ? "0" : "") + std::to_string(monat) + "-" +
           (t < 10 ? "0" : "") + std::to_string(t);
}
}  // namespace

// Konstruktor: übernimmt die Werte (Strings per std::move, also ohne Kopie) und prüft sie
Zinsentscheidung::Zinsentscheidung(std::string zentralbank, std::string land, std::chrono::sys_days datum,
                                   double leitzins)
    : zentralbank(std::move(zentralbank)), land(std::move(land)), datum(datum), leitzins(leitzins) {
    // Sofort prüfen: eine ungültige Zinsentscheidung soll gar nicht erst entstehen
    validieren();
}

// Prüft alle Felder der Zinsentscheidung
void Zinsentscheidung::validieren() const {
    // Ohne Namen der Zentralbank ist die Entscheidung nicht zuzuordnen
    if (zentralbank.empty()) {
        // Fehler mit Datum
        throw std::invalid_argument("Invalid rate decision on " + datumText(datum) + ": central bank must not be empty");
    }
    // Ohne Land kann keinem Wertpapier eine Zentralbank zugeordnet werden
    if (land.empty()) {
        // Fehler mit Zentralbank und Datum
        throw std::invalid_argument("Invalid rate decision of " + zentralbank + " on " + datumText(datum) +
                                    ": country must not be empty");
    }
    // Der Leitzins muss eine echte Zahl sein (kein NaN und nicht unendlich, z. B. nach fehlerhaftem Parsen)
    if (!std::isfinite(leitzins)) {
        // Fehler mit Zentralbank und Datum
        throw std::invalid_argument("Invalid rate decision of " + zentralbank + " on " + datumText(datum) +
                                    ": rate must be a finite number");
    }
}
