// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Antwortbewertung für den Festplatten-Cache (E7)
#include "http_cache.hpp"
// Schnittstelle für den HTTP-Zugriff
#include "http_client.hpp"
// Schnittstelle, die diese Klasse implementiert
#include "rate_source.hpp"

// std::chrono::sys_days
#include <chrono>
// std::string
#include <string>
// std::vector
#include <vector>

// Ein Tageswert einer FRED-Zeitreihe
struct FredBeobachtung {
    // Tag
    std::chrono::sys_days datum;
    // Wert (Leitzins in Prozent)
    double wert;
};

// Zinsdaten-Quelle für FRED (Federal Reserve Bank of St. Louis), Funktion "series/observations".
//
// FRED liefert keine Liste von "Entscheidungen", sondern den Leitzins für jeden Tag. Eine Entscheidung wird
// daraus abgeleitet: an jedem Tag, an dem sich der Wert gegenüber dem vorherigen Tag ÄNDERT.
// Wichtig (dokumentierte Abweichung, siehe docs/E6_Teil2_Zinsquelle.md):
//   - Das Datum ist der Tag, an dem der neue Zins WIRKSAM wird, nicht der Tag der Sitzung
//     (bei der Fed meist der Tag danach, bei der EZB bis zu einer Woche später).
//   - Entscheidungen, die den Zins unverändert lassen, sind in den Daten nicht sichtbar.
class FredQuelle : public IRateSource {
public:
    // Name der Umgebungsvariable, aus der der API-Key standardmäßig gelesen wird
    static constexpr const char* standardVariable = "FRED_API_KEY";

    // Konstruktor: merkt sich den Client und liest SOFORT den API-Key (fehlt er: KonfigurationsFehler,
    // also vor der ersten Anfrage, wie in E2). "ab": erster Tag der geladenen Daten (Format YYYY-MM-DD).
    explicit FredQuelle(IHttpClient& client, const std::string& schluesselVariable = standardVariable,
                        std::string ab = "2015-01-01");

    // Lädt die Zeitreihe der Zentralbank und leitet die Entscheidungen ab.
    // Wirft NetzwerkFehler, HttpFehler (z. B. 400 ungültiger Key, 404 unbekannte Zeitreihe),
    // KontingentFehler (429) oder ParseFehler.
    std::vector<Zinsentscheidung> ladeEntscheidungen(const Zentralbank& bank) override;

    // Anfrage-URL für eine Zeitreihe (öffentlich für Tests)
    std::string url(const std::string& serie) const;

    // Liest die Tageswerte aus einer FRED-Antwort (JSON-Text), aufsteigend nach Datum.
    // Tage ohne Wert (FRED schreibt "." ) werden übersprungen. Wirft ParseFehler bei ungültigem Inhalt.
    // "serie" nur für die Fehlermeldungen.
    static std::vector<FredBeobachtung> leseBeobachtungen(const std::string& json, const std::string& serie);

    // Leitet aus den Tageswerten die Entscheidungen ab: ein Eintrag pro Änderung und pro Land der Zentralbank.
    // Der erste Tag zählt nicht als Entscheidung (ob sich dort etwas geändert hat, ist unbekannt).
    static std::vector<Zinsentscheidung> leiteEntscheidungenAb(const std::vector<FredBeobachtung>& beobachtungen,
                                                               const Zentralbank& bank);

    // Bewertet eine Antwort für den Festplatten-Cache (E7): 429 -> Kontingent, gültige Daten -> verwertbar
    static Antwortbewertung bewerte(const HttpAntwort& antwort);

private:
    // Der HTTP-Client (gehört dem Aufrufer)
    IHttpClient& client_;
    // Der API-Key
    std::string schluessel_;
    // Erster Tag der geladenen Daten
    std::string ab_;
};
