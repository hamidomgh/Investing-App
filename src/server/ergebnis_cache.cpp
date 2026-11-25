// Eigene Header-Datei mit der Klasse ErgebnisCache
#include "ergebnis_cache.hpp"

// std::invalid_argument
#include <stdexcept>
// std::move
#include <utility>

// Standard-Regel: 5xx nicht aufbewahren
bool ErgebnisCache::standardRegel(const Antwort& antwort) {
    // Alles unter 500 ist dauerhaft gültig für diese Anfrage
    return antwort.status < 500;
}

// Konstruktor: Werte prüfen und übernehmen
ErgebnisCache::ErgebnisCache(const IUhr& uhr, std::chrono::seconds gueltigkeit, std::size_t maxEintraege,
                             SpeicherRegel regel)
    : uhr_(uhr), gueltigkeit_(gueltigkeit), maxEintraege_(maxEintraege), regel_(std::move(regel)) {
    // Negative Gültigkeit ist sinnlos
    if (gueltigkeit_.count() < 0) {
        // Fehler
        throw std::invalid_argument("ErgebnisCache: validity must be >= 0 seconds");
    }
    // Ohne Platz kann nichts gespeichert werden
    if (maxEintraege_ == 0) {
        // Fehler
        throw std::invalid_argument("ErgebnisCache: maximum number of entries must be > 0");
    }
}

// Gültig, wenn jünger als die Gültigkeitsdauer (und nicht aus der Zukunft)
bool ErgebnisCache::istAktuell(const Eintrag& eintrag) const {
    // Alter des Eintrags
    const auto alter = uhr_.jetzt() - eintrag.erstellt;
    // Zeitpunkt in der Zukunft (Uhr zurückgestellt): nicht mehr verwenden
    return alter >= std::chrono::seconds{0} && alter < gueltigkeit_;
}

// Schafft Platz für einen neuen Eintrag
void ErgebnisCache::schaffePlatz() {
    // Noch Platz: nichts zu tun
    if (eintraege_.size() < maxEintraege_) {
        // Fertig
        return;
    }
    // Schritt 1: alle fertigen, abgelaufenen Einträge entfernen
    for (auto it = eintraege_.begin(); it != eintraege_.end();) {
        // Fertig und abgelaufen?
        if (it->second.fertig && !istAktuell(it->second)) {
            // erase liefert den Iterator auf das nächste Element
            it = eintraege_.erase(it);
        } else {
            // Weiter
            ++it;
        }
    }
    // Schritt 2: solange noch voll, den ältesten fertigen Eintrag entfernen
    while (eintraege_.size() >= maxEintraege_) {
        // Ältesten fertigen Eintrag suchen (laufende Berechnungen werden nie entfernt: andere warten darauf)
        auto aeltester = eintraege_.end();
        // Alle Einträge durchgehen
        for (auto it = eintraege_.begin(); it != eintraege_.end(); ++it) {
            // Nur fertige; älter als der bisher älteste?
            if (it->second.fertig && (aeltester == eintraege_.end() || it->second.erstellt < aeltester->second.erstellt)) {
                // Neuer Kandidat
                aeltester = it;
            }
        }
        // Nur laufende Berechnungen übrig: dann darf die Grenze kurz überschritten werden
        if (aeltester == eintraege_.end()) {
            // Aufhören
            return;
        }
        // Ältesten entfernen
        eintraege_.erase(aeltester);
    }
}

// Ergebnis holen oder höchstens einmal berechnen
Antwort ErgebnisCache::hole(const std::string& schluessel, const std::function<Antwort()>& berechne) {
    // Versprechen für das Ergebnis, falls DIESER Thread rechnen muss
    std::promise<Antwort> versprechen;
    // Ergebnis, auf das gewartet wird (eigenes oder fremdes)
    std::shared_future<Antwort> zukunft;
    // Muss dieser Thread selbst rechnen?
    bool ichRechne = false;

    // Kurzer gesperrter Abschnitt: nachsehen oder Eintrag anlegen
    {
        // map sperren
        std::lock_guard<std::mutex> sperre(mutex_);
        // Eintrag suchen
        const auto it = eintraege_.find(schluessel);
        // Gefunden und entweder noch in Berechnung oder fertig und aktuell
        if (it != eintraege_.end() && (!it->second.fertig || istAktuell(it->second))) {
            // Auf dieses Ergebnis warten (bzw. es sofort abholen)
            zukunft = it->second.ergebnis;
        } else {
            // Abgelaufenen Eintrag entfernen
            if (it != eintraege_.end()) {
                // Entfernen
                eintraege_.erase(it);
            }
            // Platz schaffen, falls nötig
            schaffePlatz();
            // future zum eigenen Versprechen; share() macht daraus ein shared_future für alle
            zukunft = versprechen.get_future().share();
            // Eintrag sofort anlegen: wer jetzt kommt, wartet auf diese Berechnung
            eintraege_[schluessel] = Eintrag{zukunft, uhr_.jetzt(), false};
            // Dieser Thread rechnet
            ichRechne = true;
            // Zählen
            ++berechnungen_;
        }
    }

    // Jemand anderes rechnet oder hat schon gerechnet: warten und Ergebnis zurückgeben.
    // get() blockiert, bis das Ergebnis da ist, und wirft dessen Exception weiter, falls es eine gab.
    if (!ichRechne) {
        // Ergebnis (Kopie)
        return zukunft.get();
    }

    // Dieser Thread rechnet, OHNE gesperrten Mutex (andere Schlüssel können parallel rechnen)
    try {
        // Berechnen
        Antwort antwort = berechne();
        // Ergebnis allen Wartenden bekanntgeben
        versprechen.set_value(antwort);
        // Eintrag aktualisieren
        {
            // map sperren
            std::lock_guard<std::mutex> sperre(mutex_);
            // Eigenen Eintrag suchen (laufende Einträge entfernt niemand sonst, er ist also noch da)
            const auto it = eintraege_.find(schluessel);
            // Sicherheitshalber prüfen
            if (it != eintraege_.end()) {
                // Aufbewahren oder verwerfen?
                if (regel_(antwort)) {
                    // Als fertig markieren; ab jetzt gilt die Gültigkeit
                    it->second.fertig = true;
                } else {
                    // Nicht aufbewahren (z. B. 5xx); Wartende haben ihr Ergebnis trotzdem schon
                    eintraege_.erase(it);
                }
            }
        }
        // Ergebnis zurückgeben
        return antwort;
    } catch (...) {
        // Exception an alle Wartenden weitergeben
        versprechen.set_exception(std::current_exception());
        // Eintrag entfernen, damit es beim nächsten Mal neu versucht wird
        {
            // map sperren
            std::lock_guard<std::mutex> sperre(mutex_);
            // Entfernen
            eintraege_.erase(schluessel);
        }
        // Exception auch an den eigenen Aufrufer weitergeben
        throw;
    }
}

// Anzahl Einträge
std::size_t ErgebnisCache::groesse() const {
    // Sperren, weil andere Threads gleichzeitig ändern könnten
    std::lock_guard<std::mutex> sperre(mutex_);
    // Größe
    return eintraege_.size();
}

// Anzahl Berechnungen
std::size_t ErgebnisCache::anzahlBerechnungen() const {
    // Sperren
    std::lock_guard<std::mutex> sperre(mutex_);
    // Zähler
    return berechnungen_;
}
