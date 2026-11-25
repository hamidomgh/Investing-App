// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Kursreihe und Wertpapier (E1)
#include "kursdaten.hpp"

// std::string für den Hinweis
#include <string>
// std::move
#include <utility>

// Kurse eines Wertpapiers zusammen mit der Information, ob sie aktuell sind (E7)
struct Kursdaten {
    // Leere Kursdaten (leere Reihe, aktuell)
    Kursdaten() = default;
    // Kursdaten aus einer Reihe; veraltet und hinweis sind optional (wie bei HttpAntwort, vermeidet
    // Warnungen über fehlende Initialisierer bei Kursdaten{reihe})
    Kursdaten(Kursreihe reihe, bool veraltet = false, std::string hinweis = "")
        : reihe(std::move(reihe)), veraltet(veraltet), hinweis(std::move(hinweis)) {}

    // Die Kurse
    Kursreihe reihe;
    // true, wenn die Kurse aus einem abgelaufenen Cache-Eintrag stammen (Netz weg oder Kontingent erschöpft)
    bool veraltet = false;
    // Hinweis für den Benutzer, z. B. "Stale data from 2026-11-12 14:03 UTC (network error: ...)"; sonst leer
    std::string hinweis;
};

// Schnittstelle (Interface) für Kursdaten-Quellen (E2).
// Eine Implementierung (z. B. für Alpha Vantage) holt die Daten über einen IHttpClient,
// wandelt die JSON-Antwort in validierte Kerzen um und gibt sie als Kursreihe zurück.
// Die Analyse (E3-E6) kennt nur diese Schnittstelle, nicht den Anbieter.
class IPriceSource {
public:
    // Virtueller Destruktor, damit abgeleitete Klassen über einen Basiszeiger richtig gelöscht werden
    virtual ~IPriceSource() = default;

    // Lädt die täglichen Kurse eines Wertpapiers.
    // Wirft eine Unterklasse von QuellenFehler (siehe quellenfehler.hpp), wenn das nicht gelingt.
    virtual Kursreihe ladeKurse(const Wertpapier& wertpapier) = 0;

    // Wie ladeKurse, liefert aber zusätzlich, ob die Daten veraltet sind, und den Hinweis dazu (für E8).
    // Nicht rein virtuell: Die Standard-Implementierung ruft ladeKurse auf und meldet "aktuell".
    // So müssen bestehende Quellen (und Test-Fakes) nichts ändern; nur Quellen, die einen Cache
    // verwenden (AlphaVantageQuelle), überschreiben diese Methode.
    virtual Kursdaten ladeKursdaten(const Wertpapier& wertpapier) { return Kursdaten{ladeKurse(wertpapier)}; }
};
