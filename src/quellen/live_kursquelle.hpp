// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// CachingHttpClient (E7): Zwischenspeicher auf der Festplatte
#include "http_cache.hpp"
// IHttpClient: der Zugang zum Netz (im Programm LiveHttpClient, im Test FakeHttpClient)
#include "http_client.hpp"
// IPriceSource, Kursreihe, Kursdaten, Wertpapier
#include "price_source.hpp"
// IUhr für die Gültigkeit der Cache-Einträge
#include "uhr.hpp"

// std::chrono für die Gültigkeit
#include <chrono>
// std::filesystem::path für den Cache-Ordner
#include <filesystem>
// std::unique_ptr für die Quelle des Anbieters
#include <memory>
// std::string für den Namen des Anbieters
#include <string>

// Kursquelle für den Live-Betrieb (F5): baut für EINEN Anbieter die ganze Kette zusammen
//
//   LiveKursquelle -> AlphaVantageQuelle oder TwelveDataQuelle -> CachingHttpClient (E7) -> netz -> Internet
//
// Abruf bei Bedarf: Erst wenn ein Wertpapier angefragt wird und sein Cache-Eintrag älter als die Gültigkeit
// ist, geht eine Anfrage ins Netz. Jede Antwort enthält die letzten 100 Handelstage, deshalb fehlen nach
// einer Pause (Laptop aus) keine Tage.
//
// Eine eigene Klasse statt Code in main, damit die Verdrahtung mit FakeHttpClient und FakeUhr testbar ist.
class LiveKursquelle : public IPriceSource {
public:
    // Empfohlene Gültigkeit je Anbieter, passend zum kostenlosen Kontingent:
    //   alphavantage: 12 Stunden (nur ca. 25 Abrufe pro Tag)
    //   twelvedata:    1 Stunde  (ca. 800 Abrufe pro Tag)
    // Wirft KonfigurationsFehler bei einem unbekannten Anbieter.
    static std::chrono::hours standardGueltigkeit(const std::string& anbieter);

    // anbieter:    "alphavantage" oder "twelvedata"
    // netz:        Client für das Netz (Referenz, gehört dem Aufrufer)
    // uhr:         Uhr für den Cache (Referenz, gehört dem Aufrufer)
    // cacheOrdner: Ordner für die Cache-Dateien, z. B. cache/alphavantage (wird angelegt)
    // gueltigkeit: wie lange ein Eintrag als aktuell gilt
    // Wirft KonfigurationsFehler bei unbekanntem Anbieter oder fehlendem API-Key (schon beim Start, DoD E2).
    LiveKursquelle(const std::string& anbieter, IHttpClient& netz, const IUhr& uhr,
                   std::filesystem::path cacheOrdner, std::chrono::seconds gueltigkeit);

    // Lädt die Kurse über die Kette (siehe oben)
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override;
    // Wie ladeKurse, mit Markierung "veraltet" und Hinweis, wenn der Cache einspringen musste
    Kursdaten ladeKursdaten(const Wertpapier& wertpapier) override;

private:
    // Der Cache um das Netz. Muss VOR quelle_ stehen: Mitglieder werden in dieser Reihenfolge angelegt,
    // und quelle_ bekommt eine Referenz auf cache_.
    CachingHttpClient cache_;
    // Die Quelle des Anbieters (fragt über cache_ an)
    std::unique_ptr<IPriceSource> quelle_;
};
