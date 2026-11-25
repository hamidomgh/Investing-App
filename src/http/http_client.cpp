// Eigene Header-Datei mit HttpAntwort, IHttpClient und pruefeStatus
#include "http_client.hpp"

// Die Fehlerklassen, die hier geworfen werden
#include "quellenfehler.hpp"

// Wertet den HTTP-Status aus und wirft bei Fehlern die passende Exception
void pruefeStatus(const HttpAntwort& antwort, const std::string& kontext) {
    // Alle Statuscodes von 200 bis 299 bedeuten Erfolg
    if (antwort.status >= 200 && antwort.status <= 299) {
        // Kein Fehler: einfach zurückkehren
        return;
    }
    // 401 (nicht autorisiert) oder 403 (verboten): fast immer ein falscher oder fehlender API-Key
    if (antwort.status == 401 || antwort.status == 403) {
        // Fehler mit Status und Hinweis auf den Schlüssel
        throw HttpFehler(antwort.status, "Request for " + kontext + " was rejected (HTTP " +
                                             std::to_string(antwort.status) + "): API key invalid or not permitted");
    }
    // 404: die angefragte Ressource gibt es nicht, z. B. ein unbekanntes Symbol
    if (antwort.status == 404) {
        // Fehler mit Status
        throw HttpFehler(404, "Request for " + kontext + " failed (HTTP 404): not found, check the symbol");
    }
    // 429: zu viele Anfragen, das kostenlose Kontingent ist aufgebraucht
    if (antwort.status == 429) {
        // Eigene Fehlerklasse, damit E7 gezielt auf alte Daten im Cache ausweichen kann
        throw KontingentFehler("Request for " + kontext + " failed (HTTP 429): request quota exhausted");
    }
    // Alle anderen Fehlerstatus (z. B. 500 Serverfehler): allgemeiner HTTP-Fehler mit dem Code
    throw HttpFehler(antwort.status,
                     "Request for " + kontext + " failed (HTTP " + std::to_string(antwort.status) + ")");
}
