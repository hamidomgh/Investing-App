// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Anfrage und Antwort
#include "anfrage.hpp"

// Schnittstelle für alles, was eine Anfrage in eine Antwort verwandelt (E9).
// Implementiert von Anfragebehandlung (rechnet selbst) und GecachteAnfragebehandlung (fragt zuerst den Cache).
// Der Server kennt nur diese Schnittstelle, deshalb lässt sich der Cache einfach dazwischenschalten.
class IAnfrageBehandler {
public:
    // Virtueller Destruktor für sauberes Löschen über einen Basiszeiger
    virtual ~IAnfrageBehandler() = default;

    // Behandelt eine Anfrage. Darf von mehreren Threads gleichzeitig aufgerufen werden (Vertrag ab E9).
    virtual Antwort behandle(const Anfrage& anfrage) = 0;
};
