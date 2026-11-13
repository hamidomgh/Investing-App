// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle, die diese Klasse implementiert
#include "price_source.hpp"

// std::filesystem::path für den Ordner
#include <filesystem>
// std::string
#include <string>

// Kursdaten-Quelle ohne Netzwerk: liest gespeicherte Alpha-Vantage-Antworten aus einem Ordner.
// Für jedes Wertpapier wird die Datei "<ordner>/<symbol>.json" gelesen, z. B. "daten/IBM.json".
// Das Format ist genau das von Alpha Vantage (TIME_SERIES_DAILY); geparst wird mit AlphaVantageQuelle::parse.
//
// Zweck: Der Server (E8) und die Weboberfläche (E10) lassen sich so ohne Live-Zugriff benutzen,
// solange der echte HTTPS-Client (CMake-Option) noch nicht eingebaut ist. Auch nützlich für Vorführungen.
class DateiQuelle : public IPriceSource {
public:
    // Ordner, in dem die Dateien liegen
    explicit DateiQuelle(std::filesystem::path ordner);

    // Liest und parst die Datei des Wertpapiers.
    // Wirft QuellenFehler, wenn das Symbol unzulässige Zeichen enthält oder die Datei fehlt,
    // und ParseFehler/KontingentFehler/HttpFehler wie AlphaVantageQuelle::parse, wenn der Inhalt nicht passt.
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override;

    // Pfad der Datei für ein Symbol (öffentlich für Tests)
    std::filesystem::path dateiFuer(const std::string& symbol) const;

private:
    // Der Ordner mit den Dateien
    std::filesystem::path ordner_;
};
