// Einbindung der Header-Datei für die Kursdaten-Klassen
#include "kursdaten.hpp"
#include <utility>     // Für std::move (F3)
#include <algorithm>     // Für std::sort (nicht verwendet, aber eingebunden)
#include <stdexcept>     // Für Exception-Klassen wie std::invalid_argument
#include <ctime>         // Für localtime_r und strftime (Datumsformatierung)

namespace {
// Formatiert einen Zeitpunkt als Datum YYYY-MM-DD (für Fehlermeldungen)
std::string datumText(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&t, &tm);
    char puffer[16];
    std::strftime(puffer, sizeof(puffer), "%Y-%m-%d", &tm);
    return puffer;
}
}  // namespace

// Konstruktor für Kerze - initialisiert alle Werte und führt sofortige Validierung durch
Kerze::Kerze(std::chrono::system_clock::time_point datum, double open, double high, double low, double close, double volumen)
    : datum(datum), open(open), high(high), low(low), close(close), volumen(volumen) {
    // Sofortige Validierung beim Erstellen der Kerze
    validieren();
}

// Validierung der Kerze - prüft alle Werte auf Gültigkeit gemäß den Anforderungen
void Kerze::validieren() const {
    // Prüfung 1: High muss >= Low sein (ein gültiger Preisbereich)
    if (high < low) {
        // Wenn nicht erfüllt, wird eine Exception geworfen mit detaillierter Fehlermeldung
        throw std::invalid_argument("Invalid candle on " + datumText(datum) + ": High (" + std::to_string(high) + ") must be >= Low (" + std::to_string(low) + ")");
    }
    
    // Prüfung 2: Open-Preis muss innerhalb des gültigen Bereichs (Low..High) liegen
    if (open < low || open > high) {
        throw std::invalid_argument("Invalid candle on " + datumText(datum) + ": Open (" + std::to_string(open) + ") must be within Low (" + std::to_string(low) + ")..High (" + std::to_string(high) + ")");
    }
    
    // Prüfung 3: Close-Preis muss innerhalb des gültigen Bereichs (Low..High) liegen
    if (close < low || close > high) {
        throw std::invalid_argument("Invalid candle on " + datumText(datum) + ": Close (" + std::to_string(close) + ") must be within Low (" + std::to_string(low) + ")..High (" + std::to_string(high) + ")");
    }
    
    // Prüfung 4: Volumen muss nicht negativ sein (nicht negativ, aber kann 0 sein)
    if (volumen < 0) {
        throw std::invalid_argument("Invalid candle on " + datumText(datum) + ": Volume (" + std::to_string(volumen) + ") must be non-negative");
    }
}

// Konstruktor für Kursreihe - initialisiert mit einem Vector von Kerzen und führt Validierung durch
Kursreihe::Kursreihe(const std::vector<Kerze>& kerzen) : kerzen_(kerzen) {
    // Validierung aller Kerzen in der Reihe beim Erstellen
    validieren();
}

// Methode zum Hinzufügen einer Kerze zur Reihe
void Kursreihe::hinzufuegen(const Kerze& kerze) {
    // Die Kerze selbst muss gültig sein
    kerze.validieren();
    // Das Datum muss streng größer sein als das der letzten Kerze
    if (!kerzen_.empty() && kerze.datum <= kerzen_.back().datum) {
        throw std::invalid_argument("Invalid series: Candle on " + datumText(kerze.datum) +
                                    " must be later than previous candle on " + datumText(kerzen_.back().datum));
    }
    // Fügt die Kerze am Ende des Vectors hinzu
    kerzen_.push_back(kerze);
}

// Methode zum Abrufen der Anzahl der Kerzen in der Reihe
std::size_t Kursreihe::groesse() const {
    // Gibt die Größe des Vectors zurück
    return kerzen_.size();
}

// Operator zum direkten Zugriff auf eine Kerze über ihren Index
const Kerze& Kursreihe::operator[](std::size_t index) const {
    // Gibt eine Referenz auf die Kerze am angegebenen Index zurück
    // (benutzt at() für Bounds-Check)
    return kerzen_.at(index);
}

// Methode zum Abrufen aller Kerzen als Vector
std::vector<Kerze> Kursreihe::getKerzen() const {
    // Gibt eine Kopie aller Kerzen zurück
    return kerzen_;
}

// Validierung der Kursreihe - prüft die Reihenfolge und alle Kerzen
void Kursreihe::validieren() const {
    // Leere Reihe ist erlaubt (keine Validierung notwendig)
    if (kerzen_.empty()) {
        return;
    }
    
    // Prüfung 1: Kerzen müssen chronologisch aufsteigend sein
    for (std::size_t i = 1; i < kerzen_.size(); ++i) {
        // Vergleicht Datum der aktuellen Kerze mit dem vorherigen
        if (kerzen_[i].datum <= kerzen_[i-1].datum) {
            // Wenn nicht streng aufsteigend (auch doppelte Daten), wird eine Exception geworfen
            throw std::invalid_argument("Invalid series: Candle on " + datumText(kerzen_[i].datum) +
                                        " must be later than previous candle on " + datumText(kerzen_[i-1].datum));
        }
    }
    
    // Prüfung 2: Jede einzelne Kerze muss gültig sein
    for (const auto& kerze : kerzen_) {
        // Ruft die Validierung jeder Kerze auf
        kerze.validieren();
    }
}

// Konstruktor für Wertpapier - initialisiert alle Metadaten
Wertpapier::Wertpapier(const std::string& symbol, const std::string& name, const std::string& land, const std::string& waehrung,
                       std::map<std::string, std::string> symbole)
    : symbol(symbol), name(name), land(land), waehrung(waehrung), symbole(std::move(symbole)) {
    // Alle Metadaten müssen gesetzt sein
    if (symbol.empty()) throw std::invalid_argument("Invalid security: symbol must not be empty");
    if (name.empty()) throw std::invalid_argument("Invalid security " + symbol + ": name must not be empty");
    if (land.empty()) throw std::invalid_argument("Invalid security " + symbol + ": country must not be empty");
    if (waehrung.empty()) throw std::invalid_argument("Invalid security " + symbol + ": currency must not be empty");
    // Anbieter und Kürzel in "symbole" dürfen nicht leer sein (F3)
    for (const auto& [anbieter, kuerzel] : this->symbole) {
        // Leerer Anbieter oder leeres Kürzel
        if (anbieter.empty() || kuerzel.empty()) {
            // Fehler mit Symbol
            throw std::invalid_argument("Invalid security " + symbol + ": provider and provider symbol must not be empty");
        }
    }
}

// Liefert der Anbieter das Wertpapier? (F3)
bool Wertpapier::bietet(const std::string& anbieter) const {
    // Ohne "symbole" nur der Standardanbieter
    if (symbole.empty()) return anbieter == standardAnbieter;
    // Sonst nur die eingetragenen Anbieter
    return symbole.count(anbieter) > 0;
}

// Kürzel beim Anbieter (F3)
std::string Wertpapier::symbolFuer(const std::string& anbieter) const {
    // Anbieter liefert es nicht: Programmierfehler des Aufrufers (er hätte vorher filtern müssen)
    if (!bietet(anbieter)) {
        // Fehler mit Symbol und Anbieter
        throw std::invalid_argument("Security " + symbol + " is not available at provider " + anbieter);
    }
    // Ohne "symbole": das eigene Symbol; sonst der Eintrag des Anbieters
    return symbole.empty() ? symbol : symbole.at(anbieter);
}