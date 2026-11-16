// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle, die diese Klasse implementiert
#include "price_source.hpp"

// std::filesystem::path für den Ordner
#include <filesystem>
// std::function für den Parser des jeweiligen Anbieters (F3)
#include <functional>
// std::string
#include <string>

// Kursdaten-Quelle ohne Netzwerk: liest gespeicherte Antworten eines Anbieters aus einem Ordner.
// Für jedes Wertpapier wird die Datei "<ordner>/<symbol>.json" gelesen, z. B. "daten/alphavantage/IBM.json".
// Das Format bestimmt der Parser (F3): standardmäßig Alpha Vantage (TIME_SERIES_DAILY, AlphaVantageQuelle::parse),
// für Twelve Data TwelveDataQuelle::parse. Der Dateiname ist das angezeigte Symbol, in jedem Ordner gleich.
//
// Zweck: Der Server (E8) und die Weboberfläche (E10) lassen sich so ohne Live-Zugriff benutzen,
// solange der echte HTTPS-Client (CMake-Option) noch nicht eingebaut ist. Auch nützlich für Vorführungen.
class DateiQuelle : public IPriceSource {
public:
    // Funktion, die den Inhalt einer Datei (Text) in eine Kursreihe umwandelt; zweiter Parameter: Symbol für Meldungen
    using Parser = std::function<Kursreihe(const std::string&, const std::string&)>;

    // ordner: Ordner, in dem die Dateien liegen
    // parser: Format der Dateien (F3); Standard ist das Format von Alpha Vantage (AlphaVantageQuelle::parse),
    //         für Twelve Data TwelveDataQuelle::parse
    explicit DateiQuelle(std::filesystem::path ordner, Parser parser = {});

    // Liest und parst die Datei des Wertpapiers.
    // Wirft QuellenFehler, wenn das Symbol unzulässige Zeichen enthält oder die Datei fehlt,
    // und ParseFehler/KontingentFehler/HttpFehler wie AlphaVantageQuelle::parse, wenn der Inhalt nicht passt.
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override;

    // Pfad der Datei für ein Symbol (öffentlich für Tests)
    std::filesystem::path dateiFuer(const std::string& symbol) const;

private:
    // Der Ordner mit den Dateien
    std::filesystem::path ordner_;
    // Der Parser für das Format der Dateien
    Parser parser_;
};
