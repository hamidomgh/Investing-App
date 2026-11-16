// Ein eindeutiges Header-Guard-Mechanismus, um mehrfaches Einbinden zu verhindern
#pragma once

// Include-Dateien für benötigte Standardbibliotheken
#include <string>        // Für std::string
#include <vector>        // Für std::vector
#include <chrono>        // Für Zeitpunkte (Datum/Zeit)
#include <stdexcept>     // Für Exception-Klassen
#include <map>           // Für std::map (Symbol je Anbieter, F3)

// Einzelne Kurskerze (Candlestick) - repräsentiert einen einzelnen Handelstag
struct Kerze {
    // Datum und Uhrzeit des Handelstages
    std::chrono::system_clock::time_point datum;
    
    // Open-Preis (Eröffnungspreis)
    double open = 0.0;
    
    // High-Preis (Höchstpreis des Tages)
    double high = 0.0;
    
    // Low-Preis (Tiefstpreis des Tages)
    double low = 0.0;
    
    // Close-Preis (Schlusspreis)
    double close = 0.0;
    
    // Handelsvolumen (Anzahl der gehandelten Aktien)
    double volumen = 0.0;

    // Konstruktor für Kerze - initialisiert alle Werte und führt Validierung durch
    Kerze(std::chrono::system_clock::time_point datum, double open, double high, double low, double close, double volumen);
    
    // Validierungsprüfung der Kerze - prüft ob alle Werte gültig sind
    void validieren() const;
};

// Kursreihe - repräsentiert eine Reihe von Kerzen für ein Wertpapier
class Kursreihe {
public:
    // Standard-Konstruktor - erzeugt eine leere Kursreihe
    Kursreihe() = default;
    
    // Konstruktor mit Initialisierung durch Vector von Kerzen
    explicit Kursreihe(const std::vector<Kerze>& kerzen);

    // Methode zum Hinzufügen einer Kerze zur Reihe mit Validierung
    void hinzufuegen(const Kerze& kerze);

    // Zugriff auf die Anzahl der Kerzen in der Reihe
    std::size_t groesse() const;
    
    // Operator zum Zugriff auf eine spezifische Kerze über ihren Index
    const Kerze& operator[](std::size_t index) const;
    
    // Methode zum Abrufen aller Kerzen als Vector
    std::vector<Kerze> getKerzen() const;

    // Validierungsprüfung der gesamten Kursreihe
    void validieren() const;

private:
    // Interne Speicherung aller Kerzen in der Reihe
    std::vector<Kerze> kerzen_;
};

// Wertpapier-Informationen - repräsentiert Metadaten eines Wertpapiers
struct Wertpapier {
    // Symbol des Wertpapiers (z.B. AAPL für Apple)
    std::string symbol;
    
    // Name des Wertpapiers
    std::string name;
    
    // Land des Wertpapiers
    std::string land;
    
    // Währung des Wertpapiers
    std::string waehrung;

    // Kürzel je Kursdaten-Anbieter (F3), z. B. {"alphavantage": "MBG.DEX"} oder {"alphavantage": "IBM", "twelvedata": "IBM"}.
    // Leer bedeutet: nur der Standardanbieter "alphavantage", mit "symbol" als Kürzel (wie vor F3).
    // Steht ein Anbieter nicht darin, liefert dieser Anbieter das Wertpapier nicht.
    std::map<std::string, std::string> symbole;

    // Konstruktor für Wertpapier - initialisiert alle Metadaten; "symbole" ist optional (F3).
    // Wirft std::invalid_argument bei leeren Feldern oder einem leeren Anbieter bzw. Kürzel in "symbole".
    Wertpapier(const std::string& symbol, const std::string& name, const std::string& land, const std::string& waehrung,
               std::map<std::string, std::string> symbole = {});

    // Liefert dieser Anbieter das Wertpapier? (F3)
    bool bietet(const std::string& anbieter) const;

    // Kürzel, unter dem der Anbieter das Wertpapier führt (F3); ohne "symbole" ist es "symbol".
    // Wirft std::invalid_argument, wenn der Anbieter das Wertpapier nicht liefert.
    std::string symbolFuer(const std::string& anbieter) const;
};

// Name des Standardanbieters, der ohne Angabe von "symbole" gilt (F3)
inline const std::string standardAnbieter = "alphavantage";