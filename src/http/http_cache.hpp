// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// IHttpClient und HttpAntwort: der Cache ist selbst ein IHttpClient
#include "http_client.hpp"
// IUhr: austauschbare Uhr für Alter und Ablaufzeit
#include "uhr.hpp"

// std::chrono für Zeitpunkte und Dauern
#include <chrono>
// std::filesystem::path für das Cache-Verzeichnis
#include <filesystem>
// std::function für die austauschbare Bewertung der Antworten
#include <functional>
// std::optional für "Eintrag vorhanden oder nicht"
#include <optional>
// std::string
#include <string>

// Wie eine Antwort des Servers für den Cache zu bewerten ist
enum class Antwortbewertung {
    // Gültige Daten: werden gespeichert und zurückgegeben
    Verwertbar,
    // Anfragekontingent erschöpft: nicht speichern; wenn vorhanden, alte Daten mit Hinweis ausliefern
    KontingentErschoepft,
    // Sonstige Fehlerantwort (z. B. 404): nicht speichern, unverändert zurückgeben
    Unbrauchbar,
};

// Funktion, die eine Antwort bewertet. Austauschbar, weil manche Anbieter (Alpha Vantage) Fehler
// mit HTTP 200 melden und nur der Anbieter selbst weiß, ob der Inhalt brauchbar ist.
using Bewerter = std::function<Antwortbewertung(const HttpAntwort&)>;

// Standard-Bewertung nur nach Status: 200..299 verwertbar, 429 Kontingent, alles andere unbrauchbar
Antwortbewertung standardBewertung(const HttpAntwort& antwort);

// Zwischenspeicher für API-Antworten auf der Festplatte (E7), als "Decorator" um einen anderen IHttpClient:
// Er ist selbst ein IHttpClient, gibt Anfragen nur bei Bedarf an den inneren Client weiter und
// kann deshalb ohne Änderung an den Datenquellen dazwischengeschaltet werden.
//
// Ablauf von get(url):
//   1. Gültiger, nicht abgelaufener Eintrag auf der Platte -> sofort zurückgeben (keine Anfrage)
//   2. Sonst den inneren Client fragen:
//      - verwertbare Antwort          -> speichern und zurückgeben
//      - Netzwerkfehler / Kontingent  -> vorhandenen (auch abgelaufenen) Eintrag mit Hinweis zurückgeben,
//                                        ohne Eintrag: Fehler bzw. Antwort unverändert weitergeben
//      - unbrauchbare Antwort         -> unverändert zurückgeben, nicht speichern
// Beschädigte oder unvollständige Dateien werden erkannt und wie "kein Eintrag" behandelt.
// Die Dateien enthalten weder die URL noch den API-Key, nur einen Prüfwert (Hash) der URL.
class CachingHttpClient : public IHttpClient {
public:
    // innen:        der eigentliche Client (Referenz, gehört dem Aufrufer)
    // uhr:          Uhr für Alter und Ablaufzeit (Referenz, gehört dem Aufrufer)
    // verzeichnis:  Ordner für die Cache-Dateien; wird angelegt, falls er fehlt
    // gueltigkeit:  wie lange ein Eintrag als aktuell gilt (z. B. 24 Stunden); 0 = immer neu abrufen
    // bewerter:     entscheidet, ob eine Antwort gespeichert wird (Standard: nach HTTP-Status)
    // Wirft std::invalid_argument bei negativer Gültigkeit, KonfigurationsFehler, wenn der Ordner nicht anlegbar ist.
    CachingHttpClient(IHttpClient& innen, const IUhr& uhr, std::filesystem::path verzeichnis,
                      std::chrono::seconds gueltigkeit, Bewerter bewerter = standardBewertung);

    // Liefert die Antwort aus dem Cache oder vom inneren Client (siehe Ablauf oben)
    HttpAntwort get(const std::string& url) override;

    // Pfad der Cache-Datei für eine URL (öffentlich, damit Tests Dateien gezielt beschädigen können)
    std::filesystem::path dateiFuer(const std::string& url) const;

private:
    // Ein gelesener Cache-Eintrag: wann abgerufen und was der Server geantwortet hat
    struct Eintrag {
        // Zeitpunkt des Abrufs
        std::chrono::system_clock::time_point abgerufen;
        // Die gespeicherte Antwort
        HttpAntwort antwort;
    };

    // Liest den Eintrag für eine URL; leer, wenn es keinen gibt oder die Datei beschädigt ist
    std::optional<Eintrag> lese(const std::string& url) const;

    // Speichert eine Antwort für eine URL. Scheitert das Schreiben, wird das ignoriert:
    // die Antwort wird trotzdem ausgeliefert, nur eben nicht zwischengespeichert.
    void schreibe(const std::string& url, const HttpAntwort& antwort) const;

    // Prüft, ob ein Eintrag noch nicht abgelaufen ist
    bool istAktuell(const Eintrag& eintrag) const;

    // Kopie eines alten Eintrags, markiert als veraltet und mit Hinweis auf Grund und Abrufzeit
    static HttpAntwort alsVeraltet(const Eintrag& eintrag, const std::string& grund);

    // Der innere Client
    IHttpClient& innen_;
    // Die Uhr
    const IUhr& uhr_;
    // Der Cache-Ordner
    std::filesystem::path verzeichnis_;
    // Gültigkeitsdauer eines Eintrags
    std::chrono::seconds gueltigkeit_;
    // Die Bewertung der Antworten
    Bewerter bewerter_;
};
