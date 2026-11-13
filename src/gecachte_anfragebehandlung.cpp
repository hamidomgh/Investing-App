// Eigene Header-Datei mit der Klasse GecachteAnfragebehandlung
#include "gecachte_anfragebehandlung.hpp"

// Anonymer Namespace: Hilfsfunktion nur in dieser Datei sichtbar
namespace {

// Hängt "<länge>:<text>;" an, z. B. "3:IBM;"
void haengeAn(std::string& ziel, const std::string& teil) {
    // Länge, Doppelpunkt, Text, Trenner
    ziel += std::to_string(teil.size()) + ":" + teil + ";";
}

}  // namespace

// Konstruktor: Referenzen merken
GecachteAnfragebehandlung::GecachteAnfragebehandlung(IAnfrageBehandler& innen, ErgebnisCache& cache)
    : innen_(innen), cache_(cache) {}

// Schlüssel bauen. Die Parameter liegen in einer std::map und sind daher immer alphabetisch sortiert:
// "?sma=5&rsi=7" und "?rsi=7&sma=5" ergeben denselben Schlüssel.
std::string GecachteAnfragebehandlung::schluessel(const Anfrage& anfrage) {
    // Ergebnistext
    std::string s;
    // Methode und Pfad
    haengeAn(s, anfrage.methode);
    haengeAn(s, anfrage.pfad);
    // Alle Parameter als Name und Wert
    for (const auto& [name, wert] : anfrage.parameter) {
        // Name
        haengeAn(s, name);
        // Wert
        haengeAn(s, wert);
    }
    // Fertiger Schlüssel
    return s;
}

// Über den Cache behandeln
Antwort GecachteAnfragebehandlung::behandle(const Anfrage& anfrage) {
    // Schlüssel der Anfrage; das Lambda rechnet nur, wenn der Cache es verlangt
    return cache_.hole(schluessel(anfrage), [this, &anfrage] { return innen_.behandle(anfrage); });
}
