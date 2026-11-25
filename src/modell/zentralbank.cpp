// Eigene Header-Datei mit der Struktur Zentralbank
#include "zentralbank.hpp"

// std::find
#include <algorithm>
// std::invalid_argument
#include <stdexcept>
// std::move
#include <utility>

// Konstruktor: Werte übernehmen und prüfen
Zentralbank::Zentralbank(std::string name, std::vector<std::string> laender, std::string waehrung,
                         std::string fredSerie)
    : name(std::move(name)), laender(std::move(laender)), waehrung(std::move(waehrung)),
      fredSerie(std::move(fredSerie)) {
    // Ohne Namen ist die Zentralbank nicht zuzuordnen
    if (this->name.empty()) {
        // Fehler
        throw std::invalid_argument("Invalid central bank: name must not be empty");
    }
    // Ohne Länder wäre sie für kein Wertpapier zuständig
    if (this->laender.empty()) {
        // Fehler mit Namen
        throw std::invalid_argument("Invalid central bank " + this->name + ": list of countries must not be empty");
    }
    // Jedes Land muss einen Namen haben
    for (const std::string& land : this->laender) {
        // Leeres Land
        if (land.empty()) {
            // Fehler mit Namen
            throw std::invalid_argument("Invalid central bank " + this->name + ": country must not be empty");
        }
    }
    // Währung
    if (this->waehrung.empty()) {
        // Fehler mit Namen
        throw std::invalid_argument("Invalid central bank " + this->name + ": currency must not be empty");
    }
    // Ohne Zeitreihe können keine Zinsdaten geladen werden
    if (this->fredSerie.empty()) {
        // Fehler mit Namen
        throw std::invalid_argument("Invalid central bank " + this->name + ": FRED series must not be empty");
    }
}

// Zuständig, wenn das Land in der Liste steht
bool Zentralbank::zustaendigFuer(const std::string& land) const {
    // In der Liste suchen
    return std::find(laender.begin(), laender.end(), land) != laender.end();
}
