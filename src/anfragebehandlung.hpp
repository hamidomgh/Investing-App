// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Anfrage und Antwort des eigenen Servers
#include "anfrage.hpp"
// Wertpapier (E1)
#include "kursdaten.hpp"
// Schnittstelle der Kursdaten-Quelle (E2)
#include "price_source.hpp"
// Zinsentscheidung (E6)
#include "zinsentscheidung.hpp"

// std::vector
#include <vector>

// Behandelt Anfragen an die HTTP-JSON-Schnittstelle (E8), ohne selbst etwas mit dem Netzwerk zu tun zu haben.
// Eingabe: eine Anfrage (Methode, Pfad, Parameter). Ausgabe: eine Antwort (Status, JSON).
// Dadurch ist die gesamte Logik der Schnittstelle ohne Socket testbar.
//
// Endpunkte (alle nur GET):
//   /api/wertpapiere                       Liste der Wertpapiere aus dem Katalog
//   /api/kurse?symbol=&von=&bis=           Kerzen eines Wertpapiers (von/bis optional, Format YYYY-MM-DD)
//   /api/indikatoren?symbol=&von=&bis=     alle Indikatoren inkl. Support/Resistance und Pivot-Punkte,
//                    &sma=&ema=&rsi=&atr=  Perioden optional (Standard: 20, 20, 14, 14)
//                    &macd=12,26,9         schnell, langsam, signal
//                    &bollinger=20,2       Periode, Breite k
//                    &fenster=5            Fenster für lokale Extrema
//                    &toleranz=0.02        relative Toleranz für Niveaus
//   /api/ereignisstudie?symbol=&von=&bis=  Kursreaktion nach Zinsentscheidungen
//
// Statuscodes:
//   200 OK
//   400 ungültige, fehlende oder unbekannte Parameter
//   404 unbekannter Pfad oder unbekanntes Symbol
//   405 andere Methode als GET
//   422 Parameter gültig, aber mit diesen Daten nicht berechenbar (z. B. Reihe kürzer als die Periode)
//   502 Kursdaten-Anbieter liefert keine brauchbaren Daten (Netz, HTTP-Fehler, ungültiges JSON)
//   503 Anfragekontingent des Anbieters erschöpft und keine Daten im Cache
//   500 unerwarteter interner Fehler
// Jeder Fehler wird als JSON {"fehler": "...", "status": ...} geliefert.
class Anfragebehandlung {
public:
    // wertpapiere:     Katalog der Wertpapiere (wird kopiert)
    // quelle:          Kursdaten-Quelle (Referenz, gehört dem Aufrufer)
    // entscheidungen:  Zinsentscheidungen aller Zentralbanken für die Ereignisstudie
    Anfragebehandlung(std::vector<Wertpapier> wertpapiere, IPriceSource& quelle,
                      std::vector<Zinsentscheidung> entscheidungen);

    // Behandelt eine Anfrage und liefert immer eine Antwort (wirft selbst keine Exceptions).
    // Nicht const, weil die Quelle beim Laden ihren Zustand ändern kann (z. B. Cache-Dateien schreiben).
    Antwort behandle(const Anfrage& anfrage);

private:
    // Die vier Endpunkte; jeder wirft bei Fehlern eine interne Ausnahme, die behandle() in eine Antwort übersetzt
    Antwort wertpapiere(const Anfrage& anfrage) const;
    Antwort kurse(const Anfrage& anfrage);
    Antwort indikatoren(const Anfrage& anfrage);
    Antwort ereignisstudie(const Anfrage& anfrage);

    // Sucht das Wertpapier zum Parameter "symbol"; wirft 400 (fehlt) oder 404 (unbekannt)
    const Wertpapier& findeWertpapier(const Anfrage& anfrage) const;

    // Lädt die Kurse und schneidet sie auf von/bis zu (beide Grenzen eingeschlossen)
    Kursdaten ladeImZeitraum(const Wertpapier& wp, const Anfrage& anfrage);

    // Katalog
    std::vector<Wertpapier> wertpapiere_;
    // Kursdaten-Quelle
    IPriceSource& quelle_;
    // Zinsentscheidungen
    std::vector<Zinsentscheidung> entscheidungen_;
};
