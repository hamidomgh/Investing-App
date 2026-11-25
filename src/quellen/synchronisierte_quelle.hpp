// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle, die diese Klasse implementiert und umhüllt
#include "price_source.hpp"

// std::mutex und std::lock_guard
#include <mutex>

// Decorator (E9): macht eine beliebige Kursquelle thread-sicher, indem immer nur ein Thread gleichzeitig lädt.
//
// Warum nötig: Die Quellen und darunter der Festplatten-Cache (E7) haben Zustand, der nicht für mehrere
// Threads gedacht ist (Dateien schreiben, die Anfrageliste des Fake-Clients, später eine HTTPS-Verbindung).
// Zwei gleichzeitige Anfragen für dasselbe Symbol (z. B. /api/kurse und /api/indikatoren) würden sonst
// dieselbe Cache-Datei gleichzeitig schreiben.
//
// Abwägung: Das Laden läuft nacheinander, das Berechnen der Indikatoren aber weiter parallel.
// Beim kostenlosen Anbieter-Kontingent (wenige Anfragen pro Minute) kostet das praktisch nichts.
class SynchronisierteQuelle : public IPriceSource {
public:
    // innen: die eigentliche Quelle (Referenz, gehört dem Aufrufer)
    explicit SynchronisierteQuelle(IPriceSource& innen) : innen_(innen) {}

    // Lädt unter gesperrtem Mutex
    Kursreihe ladeKurse(const Wertpapier& wertpapier) override {
        // Sperre bis zum Ende der Funktion (auch bei einer Exception wird sie freigegeben: RAII)
        std::lock_guard<std::mutex> sperre(mutex_);
        // Weitergeben
        return innen_.ladeKurse(wertpapier);
    }

    // Lädt mit Hinweis unter gesperrtem Mutex
    Kursdaten ladeKursdaten(const Wertpapier& wertpapier) override {
        // Sperre bis zum Ende der Funktion
        std::lock_guard<std::mutex> sperre(mutex_);
        // Weitergeben
        return innen_.ladeKursdaten(wertpapier);
    }

private:
    // Die eigentliche Quelle
    IPriceSource& innen_;
    // Erlaubt immer nur einem Thread den Zugriff
    std::mutex mutex_;
};
