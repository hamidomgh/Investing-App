// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::runtime_error: Basisklasse der Standardbibliothek für Fehler, die erst zur Laufzeit erkennbar sind
#include <stdexcept>
// std::string für Fehlermeldungen
#include <string>

// Fehler-Hierarchie für alle Datenquellen (E2, E6). Aufbau:
//
//   QuellenFehler                      (Basis: "irgendetwas mit der Datenquelle ging schief")
//   ├── NetzwerkFehler                 (keine Verbindung, DNS, TLS ...)
//   │   └── ZeitueberschreitungFehler  (Server antwortet nicht rechtzeitig)
//   ├── HttpFehler                     (Server antwortet mit Fehlerstatus, z. B. 401, 404, 500)
//   ├── KontingentFehler               (Anfragekontingent erschöpft, z. B. 429)
//   ├── ParseFehler                    (ungültiges JSON oder fehlende Felder)
//   └── KonfigurationsFehler           (z. B. fehlender API-Key)
//
// Vorteil: Wer nur "irgendein Quellenfehler" behandeln will, fängt QuellenFehler.
// Wer gezielt reagieren will (E7: bei Netzausfall alte Daten ausliefern), fängt z. B. nur NetzwerkFehler.

// Basisklasse aller Fehler der Datenquellen
class QuellenFehler : public std::runtime_error {
public:
    // Konstruktor von std::runtime_error übernehmen: QuellenFehler("Meldung") funktioniert damit direkt
    using std::runtime_error::runtime_error;
};

// Keine Verbindung zum Server möglich (Netzwerk weg, Name unbekannt, TLS-Fehler)
class NetzwerkFehler : public QuellenFehler {
public:
    // Konstruktor der Basisklasse übernehmen
    using QuellenFehler::QuellenFehler;
};

// Der Server hat nicht rechtzeitig geantwortet. Ist ein Sonderfall des Netzwerkfehlers,
// deshalb wird er von NetzwerkFehler abgeleitet (ein catch für NetzwerkFehler fängt ihn mit).
class ZeitueberschreitungFehler : public NetzwerkFehler {
public:
    // Konstruktor der Basisklasse übernehmen
    using NetzwerkFehler::NetzwerkFehler;
};

// Der Server hat geantwortet, aber mit einem Fehlerstatus (z. B. 401 ungültiger Schlüssel, 404 unbekanntes Symbol)
class HttpFehler : public QuellenFehler {
public:
    // Konstruktor mit HTTP-Status und Meldung
    HttpFehler(int status, const std::string& meldung) : QuellenFehler(meldung), status_(status) {}

    // Liefert den HTTP-Status, damit der Aufrufer z. B. 401 und 404 unterscheiden kann
    int status() const { return status_; }

private:
    // Der HTTP-Statuscode der Antwort
    int status_;
};

// Das Anfragekontingent des Anbieters ist erschöpft (HTTP 429 oder ein Hinweis im Antworttext)
class KontingentFehler : public QuellenFehler {
public:
    // Konstruktor der Basisklasse übernehmen
    using QuellenFehler::QuellenFehler;
};

// Die Antwort ist kein gültiges JSON, oder ein benötigtes Feld fehlt bzw. hat den falschen Typ
class ParseFehler : public QuellenFehler {
public:
    // Konstruktor der Basisklasse übernehmen
    using QuellenFehler::QuellenFehler;
};

// Die Konfiguration ist unvollständig, z. B. fehlt der API-Key in der Umgebungsvariable
class KonfigurationsFehler : public QuellenFehler {
public:
    // Konstruktor der Basisklasse übernehmen
    using QuellenFehler::QuellenFehler;
};
