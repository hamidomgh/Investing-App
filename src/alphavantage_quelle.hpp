// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle für den HTTP-Zugriff
#include "http_client.hpp"
// Schnittstelle, die diese Klasse implementiert
#include "price_source.hpp"

// std::string für API-Key und Texte
#include <string>

// Kursdaten-Quelle für Alpha Vantage (Funktion TIME_SERIES_DAILY, tägliche Kurse).
// Implementiert IPriceSource und greift nur über IHttpClient auf das Netz zu,
// deshalb kann sie in Tests mit dem FakeHttpClient ohne Netzwerk geprüft werden.
//
// Besonderheit von Alpha Vantage: Fehler kommen oft mit HTTP-Status 200 und einem Hinweis im JSON:
//   {"Error Message": "..."}  -> unbekanntes Symbol oder ungültige Anfrage (wird wie HTTP 404 behandelt)
//   {"Note": "..."}           -> Anfragekontingent erschöpft
//   {"Information": "..."}    -> Kontingent erschöpft (wenn von "rate limit" die Rede ist) oder sonstiger Hinweis
class AlphaVantageQuelle : public IPriceSource {
public:
    // Name der Umgebungsvariable, aus der der API-Key standardmäßig gelesen wird
    static constexpr const char* standardVariable = "ALPHAVANTAGE_API_KEY";

    // Konstruktor: merkt sich den HTTP-Client und liest SOFORT den API-Key aus der Umgebung.
    // Fehlt der Key, wirft schon der Konstruktor KonfigurationsFehler, also vor der ersten Anfrage (DoD E2).
    // Der Client wird als Referenz gespeichert: Wer die Quelle anlegt, muss den Client länger am Leben halten.
    explicit AlphaVantageQuelle(IHttpClient& client, const std::string& schluesselVariable = standardVariable);

    // Lädt die täglichen Kurse (die letzten ca. 100 Handelstage, "compact") eines Wertpapiers.
    // Wirft NetzwerkFehler, HttpFehler, KontingentFehler oder ParseFehler (alle von QuellenFehler abgeleitet).
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override;

    // Baut die Anfrage-URL für ein Symbol (öffentlich, damit Tests die erwartete URL kennen)
    std::string url(const std::string& symbol) const;

    // Wandelt eine Alpha-Vantage-Antwort (JSON-Text) in eine validierte Kursreihe um.
    // "static", weil dafür kein Client und kein Key nötig sind; so ist das Parsen einzeln testbar.
    // "symbol" wird nur für die Fehlermeldungen verwendet.
    static Kursreihe parse(const std::string& json, const std::string& symbol);

private:
    // Der HTTP-Client, über den alle Anfragen laufen (gehört dem Aufrufer, nicht dieser Klasse)
    IHttpClient& client_;
    // Der API-Key aus der Umgebungsvariable
    std::string schluessel_;
};
