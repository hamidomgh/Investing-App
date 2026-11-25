// Eigene Header-Datei mit leseApiSchluessel
#include "umgebung.hpp"

// KonfigurationsFehler
#include "quellenfehler.hpp"

// std::getenv: liest eine Umgebungsvariable
#include <cstdlib>

// Liest den API-Key aus der Umgebung
std::string leseApiSchluessel(const std::string& variable) {
    // getenv liefert einen Zeiger auf den Wert oder nullptr, wenn die Variable nicht gesetzt ist
    const char* wert = std::getenv(variable.c_str());
    // Fehlende oder leere Variable: Fehler mit Anleitung, was zu tun ist
    if (wert == nullptr || *wert == '\0') {
        // Meldung nennt den Namen der Variable, aber natürlich keinen Schlüssel
        throw KonfigurationsFehler("API key missing: set the environment variable " + variable);
    }
    // Wert als std::string zurückgeben
    return std::string(wert);
}
