// Eigene Header-Datei mit der Klasse DateiZinsquelle
#include "datei_zinsquelle.hpp"

// Auswertung des FRED-Formats
#include "fred_quelle.hpp"
// QuellenFehler
#include "quellenfehler.hpp"

// std::isalnum
#include <cctype>
// std::ifstream
#include <fstream>
// std::stringstream
#include <sstream>
// std::move
#include <utility>

// Konstruktor: Ordner merken
DateiZinsquelle::DateiZinsquelle(std::filesystem::path ordner) : ordner_(std::move(ordner)) {}

// Datei lesen und auswerten
std::vector<Zinsentscheidung> DateiZinsquelle::ladeEntscheidungen(const Zentralbank& bank) {
    // FRED-Kennungen bestehen nur aus Buchstaben, Ziffern und _ ; alles andere könnte aus dem Ordner führen
    bool zulaessig = !bank.fredSerie.empty();
    // Jedes Zeichen prüfen
    for (unsigned char c : bank.fredSerie) {
        // Unzulässiges Zeichen?
        if (!(std::isalnum(c) || c == '_')) {
            // Merken
            zulaessig = false;
        }
    }
    // Unzulässiger Name
    if (!zulaessig) {
        // Fehler
        throw QuellenFehler("FRED series '" + bank.fredSerie + "' cannot be used as a file name");
    }
    // Pfad der Datei
    const std::filesystem::path pfad = ordner_ / (bank.fredSerie + ".json");
    // Datei öffnen
    std::ifstream datei(pfad, std::ios::binary);
    // Fehlt sie: keine Offline-Daten für diese Zentralbank
    if (!datei) {
        // Fehler mit Pfad
        throw QuellenFehler("No offline rate data for " + bank.name + ": file not found: " + pfad.string());
    }
    // Ganzen Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Wie eine echte FRED-Antwort auswerten
    return FredQuelle::leiteEntscheidungenAb(FredQuelle::leseBeobachtungen(puffer.str(), bank.fredSerie), bank);
}
