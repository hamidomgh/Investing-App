// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::string für URL und Antworttext
#include <string>
// std::move
#include <utility>

// Antwort eines HTTP-Servers: Statuscode und Inhalt
struct HttpAntwort {
    // Leere Antwort (Status 0), z. B. als Platzhalter vor einem Abruf
    HttpAntwort() = default;
    // Antwort mit Status und Inhalt, z. B. HttpAntwort{200, "..."}. Die Felder veraltet und hinweis
    // behalten ihre Standardwerte; nur der Cache (E7) setzt sie.
    HttpAntwort(int status, std::string body) : status(status), body(std::move(body)) {}

    // HTTP-Statuscode, z. B. 200 (OK), 401 (nicht autorisiert), 404 (nicht gefunden), 429 (zu viele Anfragen)
    int status = 0;
    // Inhalt der Antwort (bei den Anbietern meist JSON)
    std::string body;
    // true, wenn die Antwort aus dem Cache (E7) stammt und abgelaufen ist, weil kein neuer Abruf möglich war
    bool veraltet = false;
    // Hinweis für den Benutzer, z. B. "Stale data from 2026-11-12 14:03 UTC (network error: ...)"; sonst leer
    std::string hinweis;
};

// Schnittstelle (Interface) für HTTP-Zugriffe. Alle Datenquellen greifen nur hierüber auf das Netz zu.
// Dadurch kann in Tests ein FakeHttpClient eingesetzt werden, der ohne Netzwerk feste Antworten liefert,
// und im echten Betrieb ein HTTPS-Client (nur mit der CMake-Option für den Live-Zugriff).
//
// Vertrag (gilt für jede Implementierung):
// - Antwortet der Server, wird die Antwort zurückgegeben, AUCH bei Fehlerstatus (401, 404, 429, 500 ...).
//   Die Auswertung des Status übernimmt pruefeStatus() unten.
// - Kommt keine Antwort zustande, wird NetzwerkFehler bzw. ZeitueberschreitungFehler geworfen.
class IHttpClient {
public:
    // Virtueller Destruktor: nötig, damit ein Objekt über einen Zeiger auf die Basisklasse richtig gelöscht wird
    virtual ~IHttpClient() = default;

    // Führt eine GET-Anfrage auf die URL aus. "= 0" heißt: rein virtuell, jede Implementierung muss sie definieren.
    virtual HttpAntwort get(const std::string& url) = 0;
};

// Prüft den Status einer Antwort und wirft bei Fehlern eine passende, verständliche Exception:
// - 200..299: in Ordnung, keine Exception
// - 401, 403: HttpFehler ("API-Key ungültig oder ohne Berechtigung")
// - 404:      HttpFehler ("nicht gefunden", z. B. unbekanntes Symbol)
// - 429:      KontingentFehler ("Anfragekontingent erschöpft")
// - sonst:    HttpFehler mit dem Statuscode
// "kontext" beschreibt die Anfrage für die Meldung, z. B. "prices for AAPL".
void pruefeStatus(const HttpAntwort& antwort, const std::string& kontext);
