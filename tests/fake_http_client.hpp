// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Die Schnittstelle, die der Fake erfüllt
#include "http_client.hpp"
// NetzwerkFehler und ZeitueberschreitungFehler zum Simulieren von Ausfällen
#include "quellenfehler.hpp"

// std::map: ordnet jeder URL eine Antwort oder einen Fehler zu
#include <map>
// std::string
#include <string>
// std::vector für die Liste der gestellten Anfragen
#include <vector>

// Fake-Implementierung von IHttpClient, nur für Tests (liegt deshalb in tests/, nicht in src/).
// Statt ins Netz zu gehen, liefert sie Antworten, die der Test vorher festgelegt hat.
// So laufen alle Tests offline und liefern immer dasselbe Ergebnis (deterministisch).
class FakeHttpClient : public IHttpClient {
public:
    // Welcher Ausfall simuliert werden soll
    enum class Ausfall { Netzwerk, Zeitueberschreitung };

    // Legt fest: Eine Anfrage an "url" liefert Status und Inhalt
    void setzeAntwort(const std::string& url, int status, const std::string& body) {
        // Antwort für diese URL speichern (überschreibt eine frühere)
        antworten_[url] = HttpAntwort{status, body};
    }

    // Legt fest: Eine Anfrage an "url" scheitert mit einem Netzwerkfehler oder einer Zeitüberschreitung
    void setzeAusfall(const std::string& url, Ausfall ausfall) {
        // Ausfall für diese URL speichern
        ausfaelle_[url] = ausfall;
    }

    // Alle URLs, die bisher angefragt wurden, in Reihenfolge. Damit kann ein Test prüfen,
    // ob (und wie oft) eine Anfrage gestellt wurde, z. B. "ohne API-Key keine Anfrage".
    const std::vector<std::string>& anfragen() const { return anfragen_; }

    // Implementierung der Schnittstelle: "override" lässt den Compiler prüfen, dass die Signatur passt
    HttpAntwort get(const std::string& url) override {
        // Jede Anfrage protokollieren
        anfragen_.push_back(url);
        // Ist für diese URL ein Ausfall festgelegt?
        const auto ausfall = ausfaelle_.find(url);
        // Wenn ja: den passenden Fehler werfen, wie es ein echter Client täte
        if (ausfall != ausfaelle_.end()) {
            // Zeitüberschreitung simulieren
            if (ausfall->second == Ausfall::Zeitueberschreitung) {
                // Fehler werfen
                throw ZeitueberschreitungFehler("Fake: timeout for " + url);
            }
            // Sonst allgemeinen Netzwerkfehler simulieren
            throw NetzwerkFehler("Fake: network error for " + url);
        }
        // Ist für diese URL eine Antwort festgelegt?
        const auto antwort = antworten_.find(url);
        // Wenn ja: diese Antwort zurückgeben
        if (antwort != antworten_.end()) {
            // Gespeicherte Antwort
            return antwort->second;
        }
        // Unbekannte URL: wie ein Server, der nicht erreichbar ist. Macht Tippfehler in URLs im Test sichtbar.
        throw NetzwerkFehler("Fake: no response configured for " + url);
    }

private:
    // Festgelegte Antworten pro URL
    std::map<std::string, HttpAntwort> antworten_;
    // Festgelegte Ausfälle pro URL
    std::map<std::string, Ausfall> ausfaelle_;
    // Protokoll aller Anfragen
    std::vector<std::string> anfragen_;
};
