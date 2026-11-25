// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Antwort: die Werte, die gespeichert werden
#include "anfrage.hpp"
// IUhr: austauschbare Uhr für die Gültigkeit
#include "uhr.hpp"

// std::chrono
#include <chrono>
// std::size_t
#include <cstddef>
// std::function für die Berechnung und die Speicher-Regel
#include <functional>
// std::shared_future: mehrere Threads können auf dasselbe Ergebnis warten
#include <future>
// std::map: Schlüssel -> Eintrag
#include <map>
// std::mutex
#include <mutex>
// std::string
#include <string>

// Thread-sicherer Cache für berechnete Ergebnisse (E9).
//
// hole(schluessel, berechne):
//   - Gibt es ein aktuelles Ergebnis, wird es zurückgegeben (ohne Berechnung).
//   - Wird das Ergebnis gerade von einem anderen Thread berechnet, wartet der Aufrufer auf DIESE Berechnung,
//     statt selbst noch einmal zu rechnen ("identische Anfragen werden nicht mehrfach berechnet").
//   - Sonst berechnet der Aufrufer es selbst, und alle, die währenddessen kommen, warten auf ihn.
//
// Kern der Idee: Für jeden Schlüssel wird ein std::shared_future gespeichert, SOBALD die Berechnung beginnt.
// Der Mutex schützt nur die kurze Suche bzw. das Eintragen in der map; die (lange) Berechnung läuft ohne
// gesperrten Mutex, damit verschiedene Schlüssel parallel berechnet werden können.
class ErgebnisCache {
public:
    // Regel, ob eine fertige Antwort aufbewahrt wird
    using SpeicherRegel = std::function<bool(const Antwort&)>;

    // Standard-Regel: alles außer 5xx aufbewahren. 5xx sind vorübergehende Fehler (Anbieter nicht erreichbar,
    // Kontingent erschöpft); beim nächsten Mal soll neu versucht werden. 4xx (z. B. ungültiger Parameter)
    // hängen nur von der Anfrage ab und dürfen gespeichert werden.
    static bool standardRegel(const Antwort& antwort);

    // uhr:          für die Gültigkeit (Referenz, gehört dem Aufrufer)
    // gueltigkeit:  wie lange ein Ergebnis aktuell bleibt (z. B. 5 Minuten)
    // maxEintraege: Obergrenze der gespeicherten Ergebnisse, damit der Speicher nicht unbegrenzt wächst
    // regel:        welche Antworten aufbewahrt werden
    // Wirft std::invalid_argument bei negativer Gültigkeit oder maxEintraege == 0.
    ErgebnisCache(const IUhr& uhr, std::chrono::seconds gueltigkeit, std::size_t maxEintraege,
                  SpeicherRegel regel = standardRegel);

    // Liefert das Ergebnis für den Schlüssel; berechnet es höchstens einmal gleichzeitig (siehe oben).
    // Wirft berechne() eine Exception, bekommen sie der Berechnende und alle Wartenden; nichts wird gespeichert.
    Antwort hole(const std::string& schluessel, const std::function<Antwort()>& berechne);

    // Anzahl der Einträge (laufende und fertige), für Tests
    std::size_t groesse() const;

    // Wie oft bisher wirklich berechnet wurde, für Tests
    std::size_t anzahlBerechnungen() const;

private:
    // Ein Eintrag der map
    struct Eintrag {
        // Ergebnis (oder noch laufende Berechnung); shared_future, damit beliebig viele Threads warten können
        std::shared_future<Antwort> ergebnis;
        // Zeitpunkt, an dem die Berechnung begann
        std::chrono::system_clock::time_point erstellt;
        // false, solange die Berechnung läuft
        bool fertig = false;
    };

    // Ist ein fertiger Eintrag noch gültig? (Mutex muss gesperrt sein)
    bool istAktuell(const Eintrag& eintrag) const;

    // Entfernt Einträge, falls die Obergrenze erreicht ist (Mutex muss gesperrt sein)
    void schaffePlatz();

    // Uhr
    const IUhr& uhr_;
    // Gültigkeit
    std::chrono::seconds gueltigkeit_;
    // Obergrenze
    std::size_t maxEintraege_;
    // Regel zum Aufbewahren
    SpeicherRegel regel_;
    // Schützt eintraege_ und berechnungen_ (geteilter Zustand); mutable, damit const-Methoden sperren können
    mutable std::mutex mutex_;
    // Schlüssel -> Eintrag
    std::map<std::string, Eintrag> eintraege_;
    // Zähler der Berechnungen
    std::size_t berechnungen_ = 0;
};
