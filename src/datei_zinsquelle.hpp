// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle, die diese Klasse implementiert
#include "rate_source.hpp"

// std::filesystem::path
#include <filesystem>

// Zinsdaten-Quelle ohne Netzwerk: liest gespeicherte FRED-Antworten aus einem Ordner,
// "<ordner>/<FRED-Serie>.json", z. B. "daten/zinsen/DFEDTARU.json". Gegenstück zu DateiQuelle für Kurse.
// Inhalt und Auswertung sind genau wie bei einer echten FRED-Antwort (FredQuelle::leseBeobachtungen).
class DateiZinsquelle : public IRateSource {
public:
    // Ordner, in dem die Dateien liegen
    explicit DateiZinsquelle(std::filesystem::path ordner);

    // Liest die Datei der Zeitreihe und leitet die Entscheidungen ab.
    // Wirft QuellenFehler, wenn die Datei fehlt oder der Name unzulässig ist, ParseFehler bei ungültigem Inhalt.
    std::vector<Zinsentscheidung> ladeEntscheidungen(const Zentralbank& bank) override;

private:
    // Ordner
    std::filesystem::path ordner_;
};
