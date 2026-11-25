// Eigene Header-Datei mit der Klasse DateiQuelle
#include "dateiquelle.hpp"

// Parser für das Alpha-Vantage-Format
#include "alphavantage_quelle.hpp"
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

// Konstruktor: Ordner und Parser merken; ohne Parser gilt das Format von Alpha Vantage
DateiQuelle::DateiQuelle(std::filesystem::path ordner, Parser parser)
    : ordner_(std::move(ordner)), parser_(parser ? std::move(parser) : Parser(AlphaVantageQuelle::parse)) {}

// Pfad der Datei; prüft vorher das Symbol, damit es nicht aus dem Ordner herausführen kann
std::filesystem::path DateiQuelle::dateiFuer(const std::string& symbol) const {
    // Leeres Symbol oder "." bzw. ".." wären gefährliche Dateinamen
    bool zulaessig = !symbol.empty() && symbol != "." && symbol != "..";
    // Nur Buchstaben, Ziffern und . - _ ^ erlauben (kein "/" und kein "\", also kein Pfad in andere Ordner)
    for (unsigned char c : symbol) {
        // Ein unzulässiges Zeichen genügt
        if (!(std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == '^')) {
            // Merken
            zulaessig = false;
        }
    }
    // Unzulässiges Symbol: Fehler statt eines Pfades
    if (!zulaessig) {
        // QuellenFehler, damit ladeAlleKurse und die API ihn wie andere Quellenfehler behandeln
        throw QuellenFehler("Symbol '" + symbol + "' cannot be used as a file name");
    }
    // Ordner + Symbol + ".json"
    return ordner_ / (symbol + ".json");
}

// Liest die gespeicherte Antwort und wandelt sie in eine Kursreihe um
Kursreihe DateiQuelle::ladeKurse(const Wertpapier& wertpapier) {
    // Pfad bestimmen (prüft das Symbol)
    const std::filesystem::path pfad = dateiFuer(wertpapier.symbol);
    // Datei öffnen
    std::ifstream datei(pfad, std::ios::binary);
    // Fehlt die Datei, gibt es für dieses Wertpapier keine Offline-Daten
    if (!datei) {
        // Fehler mit Pfad, damit man weiß, welche Datei anzulegen wäre
        throw QuellenFehler("No offline data for " + wertpapier.symbol + ": file not found: " + pfad.string());
    }
    // Ganzen Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Mit dem Parser des Anbieters umwandeln (gleiche Prüfungen und Fehler wie bei einer echten Antwort)
    return parser_(puffer.str(), wertpapier.symbol);
}
