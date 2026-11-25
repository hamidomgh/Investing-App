// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Antwortbewertung für den Festplatten-Cache (E7)
#include "http_cache.hpp"
// Schnittstelle für den HTTP-Zugriff
#include "http_client.hpp"
// Schnittstelle, die diese Klasse implementiert
#include "price_source.hpp"

// std::string
#include <string>

// Kursdaten-Quelle für Twelve Data, Funktion "time_series" mit Tageskursen (Feature F3).
// Gleiches Muster wie AlphaVantageQuelle: Zugriff nur über IHttpClient, Fehler als QuellenFehler-Unterklassen,
// Ergebnis als validierte Kursreihe (E1). Im kostenlosen Tarif liefert Twelve Data nur US-Wertpapiere.
//
// Unterschiede zu Alpha Vantage:
//   - Tage als Liste "values" (neuester zuerst), Felder "datetime", "open", "high", "low", "close", "volume"
//   - Fehler als {"code": 404, "message": "...", "status": "error"} im Inhalt; ob zusätzlich der HTTP-Status
//     gesetzt ist, ist unerheblich, weil beides ausgewertet wird
class TwelveDataQuelle : public IPriceSource {
public:
    // Kennung des Anbieters im Katalog ("symbole") und bei --quelle
    static constexpr const char* anbieterId = "twelvedata";
    // Name für Anzeige und Meldungen
    static constexpr const char* anzeigeName = "Twelve Data";
    // Name der Umgebungsvariable, aus der der API-Key standardmäßig gelesen wird
    static constexpr const char* standardVariable = "TWELVEDATA_API_KEY";

    // Konstruktor: merkt sich den Client und liest SOFORT den API-Key (fehlt er: KonfigurationsFehler,
    // also vor der ersten Anfrage, wie in E2). Der Client gehört dem Aufrufer.
    explicit TwelveDataQuelle(IHttpClient& client, const std::string& schluesselVariable = standardVariable);

    // Lädt die täglichen Kurse (die letzten 100 Handelstage) unter dem Twelve-Data-Kürzel des Wertpapiers
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override;

    // Wie ladeKurse, gibt aber den Hinweis des Caches (E7) weiter, wenn die Daten veraltet sind
    Kursdaten ladeKursdaten(const Wertpapier& wertpapier) override;

    // Anfrage-URL für ein Kürzel (öffentlich für Tests)
    std::string url(const std::string& kuerzel) const;

    // Wandelt eine Twelve-Data-Antwort (JSON-Text) in eine validierte Kursreihe um.
    // Wertet auch Fehler im Inhalt aus ("status": "error"). "symbol" nur für die Meldungen.
    static Kursreihe parse(const std::string& json, const std::string& symbol);

    // Bewertet eine Antwort für den Festplatten-Cache (E7)
    static Antwortbewertung bewerte(const HttpAntwort& antwort);

private:
    // Der HTTP-Client
    IHttpClient& client_;
    // Der API-Key
    std::string schluessel_;
};
