// Eigene Header-Datei mit der Klasse LiveHttpClient
#include "live_http_client.hpp"

// Fehlerklassen: NetzwerkFehler, ZeitueberschreitungFehler, KonfigurationsFehler
#include "quellenfehler.hpp"

// cpp-httplib (third_party/httplib.h); HTTPS ist nur mit CPPHTTPLIB_OPENSSL_SUPPORT enthalten (CMake-Option)
#include <httplib.h>

// std::min
#include <algorithm>
// std::chrono::steady_clock zum Messen, wie lange die Anfrage gedauert hat
#include <chrono>
// std::move
#include <utility>

// Zeitgrenzen merken; die Verbindung selbst wird erst in get() aufgebaut
LiveHttpClient::LiveHttpClient(std::chrono::milliseconds verbindungsZeit, std::chrono::milliseconds leseZeit)
    : verbindungsZeit_(verbindungsZeit), leseZeit_(leseZeit) {}

// Mit OpenSSL übersetzt? Das Makro setzt CMake nur bei MARKETLENS_LIVE_HTTP=ON.
bool LiveHttpClient::unterstuetztHttps() {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    // HTTPS ist eingebaut
    return true;
#else
    // Nur HTTP
    return false;
#endif
}

// Zerlegt die URL in "Schema://Rechner[:Port]" und "/Pfad?Abfrage"
LiveHttpClient::UrlTeile LiveHttpClient::zerlege(const std::string& url) {
    // Ergebnis, wird unten gefüllt
    UrlTeile teile;
    // Länge des Schemas samt "://"
    std::size_t schemaLaenge = 0;
    // Beginnt die URL mit "https://"?
    if (url.rfind("https://", 0) == 0) {
        // HTTPS: 8 Zeichen
        teile.https = true;
        schemaLaenge = 8;
    } else if (url.rfind("http://", 0) == 0) {
        // HTTP: 7 Zeichen
        schemaLaenge = 7;
    } else {
        // Anderes oder kein Schema: nicht unterstützt. Die URL selbst nicht ausgeben (API-Key)
        throw KonfigurationsFehler("URL must start with http:// or https://");
    }
    // Erstes "/" nach dem Rechnernamen: dort beginnt der Pfad
    const std::size_t pfadAnfang = url.find('/', schemaLaenge);
    // Kein "/" gefunden: erstes "?" (Abfrage ohne Pfad, z. B. "http://host?x=1")
    const std::size_t abfrageAnfang = url.find('?', schemaLaenge);
    // Der Pfad beginnt beim früheren der beiden Zeichen
    const std::size_t trenner = std::min(pfadAnfang, abfrageAnfang);
    // Basis: alles bis zum Pfad (oder die ganze URL)
    teile.basis = url.substr(0, trenner);
    // Ohne Rechnernamen ist die URL unbrauchbar
    if (teile.basis.size() == schemaLaenge) {
        // z. B. "https:///pfad"
        throw KonfigurationsFehler("URL has no host name");
    }
    // Pfad: Rest der URL; ohne Pfad "/"
    if (trenner == std::string::npos) {
        // Nur Rechner angegeben
        teile.pfad = "/";
    } else if (url[trenner] == '?') {
        // Abfrage ohne Pfad: "/" davorsetzen
        teile.pfad = "/" + url.substr(trenner);
    } else {
        // Normalfall: Pfad samt Abfrage
        teile.pfad = url.substr(trenner);
    }
    // Fertig
    return teile;
}

// Führt die GET-Anfrage über das Netz aus
HttpAntwort LiveHttpClient::get(const std::string& url) {
    // URL zerlegen (wirft bei unbrauchbarer URL)
    const UrlTeile teile = zerlege(url);
    // HTTPS ohne OpenSSL: klare Meldung statt einer unverständlichen Fehlermeldung von httplib
    if (teile.https && !unterstuetztHttps()) {
        // Hinweis, wie man es behebt
        throw KonfigurationsFehler("HTTPS is not available: build with cmake -DMARKETLENS_LIVE_HTTP=ON");
    }

    // Ein neuer Client je Anfrage: kein gemeinsamer Zustand, daher auch aus mehreren Threads sicher
    httplib::Client client(teile.basis);
    // Zeitgrenzen setzen, damit eine Anfrage nie ewig hängt
    client.set_connection_timeout(verbindungsZeit_);
    client.set_read_timeout(leseZeit_);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    // Zertifikat des Servers prüfen (Schutz gegen gefälschte Server); httplib nutzt die Zertifikate des Systems
    client.enable_server_certificate_verification(true);
#endif

    // Startzeit merken, um eine Zeitüberschreitung beim Lesen zu erkennen
    const auto start = std::chrono::steady_clock::now();
    // Anfrage senden und auf die Antwort warten
    httplib::Result ergebnis = client.Get(teile.pfad);
    // Dauer der Anfrage
    const auto dauer = std::chrono::steady_clock::now() - start;

    // Antwort erhalten: unverändert zurückgeben, auch bei Fehlerstatus (Vertrag von IHttpClient)
    if (ergebnis) {
        // Status und Inhalt übernehmen
        return HttpAntwort{ergebnis->status, std::move(ergebnis->body)};
    }

    // Keine Antwort: Grund von httplib
    const httplib::Error fehler = ergebnis.error();
    // Nur Schema, Rechner und Port nennen, nie den Pfad mit dem API-Key
    const std::string ziel = teile.basis;
    // Verbindungsaufbau dauerte zu lange
    if (fehler == httplib::Error::ConnectionTimeout) {
        // Sonderfall des Netzwerkfehlers
        throw ZeitueberschreitungFehler("Connection to " + ziel + " timed out");
    }
    // httplib meldet eine Zeitüberschreitung beim Lesen nur als "Read". Hat das Warten mindestens
    // (fast) die Lesezeit gedauert, war es eine Zeitüberschreitung; ein früher Lesefehler ist ein Netzwerkfehler.
    if (fehler == httplib::Error::Read && dauer >= leseZeit_ * 9 / 10) {
        // Server hat nicht rechtzeitig geantwortet
        throw ZeitueberschreitungFehler("No answer from " + ziel + " within " +
                                        std::to_string(leseZeit_.count()) + " ms");
    }
    // Alles andere: Verbindung abgelehnt, Name unbekannt, Zertifikat ungültig ...
    throw NetzwerkFehler("Request to " + ziel + " failed: " + httplib::to_string(fehler));
}
