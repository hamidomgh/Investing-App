// Eigene Header-Datei mit der Klasse LiveKursquelle
#include "live_kursquelle.hpp"

// Die beiden Anbieter (Quelle und Bewertung der Antworten für den Cache)
#include "alphavantage_quelle.hpp"
#include "twelvedata_quelle.hpp"
// KonfigurationsFehler
#include "quellenfehler.hpp"

// std::move
#include <utility>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Name des Standard-Anbieters, wie er auf der Kommandozeile steht
const std::string alphaVantageId = "alphavantage";

// true, wenn der Anbieter bekannt ist
bool istBekannt(const std::string& anbieter) {
    // Nur die beiden Anbieter aus F3
    return anbieter == alphaVantageId || anbieter == TwelveDataQuelle::anbieterId;
}

// Prüft den Anbieter; wird im Konstruktor schon VOR dem Anlegen des Caches gebraucht
const std::string& pruefeAnbieter(const std::string& anbieter) {
    // Unbekannter Anbieter: klare Meldung mit den erlaubten Namen
    if (!istBekannt(anbieter)) {
        throw KonfigurationsFehler("unknown provider '" + anbieter + "' (allowed: alphavantage, twelvedata)");
    }
    // Unverändert zurückgeben, damit der Aufruf in der Initialisierungsliste stehen kann
    return anbieter;
}

// Bewertung der Antworten für den Cache: jeder Anbieter meldet Fehler anders (Alpha Vantage z. B. mit HTTP 200)
Bewerter bewerterFuer(const std::string& anbieter) {
    // Twelve Data
    if (anbieter == TwelveDataQuelle::anbieterId) return TwelveDataQuelle::bewerte;
    // Sonst Alpha Vantage (der Anbieter wurde vorher geprüft)
    return AlphaVantageQuelle::bewerte;
}

}  // namespace

// Empfohlene Gültigkeit je Anbieter
std::chrono::hours LiveKursquelle::standardGueltigkeit(const std::string& anbieter) {
    // Twelve Data: großes Kontingent, stündlich neu
    if (pruefeAnbieter(anbieter) == TwelveDataQuelle::anbieterId) return std::chrono::hours{1};
    // Alpha Vantage: nur ca. 25 Abrufe pro Tag, bei 6 Wertpapieren höchstens zweimal am Tag neu
    return std::chrono::hours{12};
}

// Baut die Kette: erst den Cache um das Netz, dann die Quelle des Anbieters über dem Cache
LiveKursquelle::LiveKursquelle(const std::string& anbieter, IHttpClient& netz, const IUhr& uhr,
                               std::filesystem::path cacheOrdner, std::chrono::seconds gueltigkeit)
    // Anbieter zuerst prüfen, damit bei einem Tippfehler kein Cache-Ordner angelegt wird
    : cache_(netz, uhr, std::move(cacheOrdner), gueltigkeit, bewerterFuer(pruefeAnbieter(anbieter))) {
    // Quelle des Anbieters anlegen; der Konstruktor liest den API-Key und wirft, wenn er fehlt
    if (anbieter == TwelveDataQuelle::anbieterId) {
        // Twelve Data (Umgebungsvariable TWELVEDATA_API_KEY)
        quelle_ = std::make_unique<TwelveDataQuelle>(cache_);
    } else {
        // Alpha Vantage (Umgebungsvariable ALPHAVANTAGE_API_KEY)
        quelle_ = std::make_unique<AlphaVantageQuelle>(cache_);
    }
}

// Weitergeben an die Quelle des Anbieters
Kursreihe LiveKursquelle::ladeKurse(const Wertpapier& wertpapier) {
    // Die Quelle fragt über den Cache an
    return quelle_->ladeKurse(wertpapier);
}

// Weitergeben, mit Markierung des Caches
Kursdaten LiveKursquelle::ladeKursdaten(const Wertpapier& wertpapier) {
    // Die Quelle übernimmt "veraltet" und den Hinweis aus der Antwort des Caches
    return quelle_->ladeKursdaten(wertpapier);
}
