// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// IHttpClient und HttpAntwort: der Live-Client ist eine Implementierung dieser Schnittstelle
#include "http_client.hpp"

// std::chrono::milliseconds für die Zeitgrenzen
#include <chrono>
// std::string für URLs
#include <string>

// Echter HTTP(S)-Client über das Netz (Live-Zugriff), gebaut mit cpp-httplib.
// Er ist die letzte Stufe der Kette: Datenquelle -> CachingHttpClient (E7) -> LiveHttpClient -> Internet.
//
// - "http://..." funktioniert immer (so testen die Tests ohne Internet gegen einen lokalen Server).
// - "https://..." nur, wenn mit der CMake-Option MARKETLENS_LIVE_HTTP (OpenSSL) gebaut wurde;
//   sonst KonfigurationsFehler mit Hinweis auf die Option.
// - Das Serverzertifikat wird bei HTTPS immer geprüft (Zertifikate des Systems).
//
// Hält den Vertrag von IHttpClient ein:
// - Jede Antwort des Servers wird zurückgegeben, auch mit Fehlerstatus (401, 429, 500 ...).
// - Ohne Antwort: ZeitueberschreitungFehler (zu langsam) bzw. NetzwerkFehler (sonstiges).
// - Fehlermeldungen nennen nur Schema, Rechner und Port, nie die ganze URL, weil darin der API-Key steht.
class LiveHttpClient : public IHttpClient {
public:
    // verbindungsZeit: wie lange der Verbindungsaufbau dauern darf
    // leseZeit:        wie lange auf Daten des Servers gewartet wird
    // Standard: 10 s bzw. 30 s, genug für die Anbieter, aber kein ewiges Hängen.
    explicit LiveHttpClient(std::chrono::milliseconds verbindungsZeit = std::chrono::seconds{10},
                            std::chrono::milliseconds leseZeit = std::chrono::seconds{30});

    // Führt die GET-Anfrage aus (siehe Vertrag oben)
    HttpAntwort get(const std::string& url) override;

    // true, wenn dieses Programm mit HTTPS-Unterstützung (OpenSSL) übersetzt wurde
    static bool unterstuetztHttps();

    // Teile einer URL: "https://api.example.com:8443/pfad?x=1" -> basis "https://api.example.com:8443",
    // pfad "/pfad?x=1". Öffentlich, damit die Zerlegung einzeln getestet werden kann.
    struct UrlTeile {
        // Schema, Rechner und Port (ohne Pfad); so erwartet es httplib::Client
        std::string basis;
        // Pfad mit Abfrage ("?..."), mindestens "/"
        std::string pfad;
        // true bei "https"
        bool https = false;
    };
    // Zerlegt eine URL; wirft KonfigurationsFehler, wenn sie nicht mit http:// oder https:// beginnt
    // oder kein Rechner angegeben ist.
    static UrlTeile zerlege(const std::string& url);

private:
    // Zeitgrenze für den Verbindungsaufbau
    std::chrono::milliseconds verbindungsZeit_;
    // Zeitgrenze für das Lesen der Antwort
    std::chrono::milliseconds leseZeit_;
};
