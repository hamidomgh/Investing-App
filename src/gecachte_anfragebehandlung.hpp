// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle, die diese Klasse implementiert und umhüllt
#include "anfrage_behandler.hpp"
// Der thread-sichere Ergebnis-Cache
#include "ergebnis_cache.hpp"

// std::string
#include <string>

// Decorator (E9): fragt für jede Anfrage zuerst den ErgebnisCache und lässt nur bei Bedarf den inneren
// Behandler rechnen. Identische Anfragen werden so nur einmal berechnet, auch wenn sie gleichzeitig ankommen.
class GecachteAnfragebehandlung : public IAnfrageBehandler {
public:
    // innen: rechnet wirklich (z. B. Anfragebehandlung); cache: speichert die Ergebnisse.
    // Beide als Referenz, sie gehören dem Aufrufer.
    GecachteAnfragebehandlung(IAnfrageBehandler& innen, ErgebnisCache& cache);

    // Ergebnis aus dem Cache oder vom inneren Behandler
    Antwort behandle(const Anfrage& anfrage) override;

    // Eindeutiger Schlüssel einer Anfrage: Methode, Pfad und alle Parameter.
    // Jeder Teil wird mit seiner Länge davor geschrieben ("3:IBM"), damit Werte mit Sonderzeichen wie & oder =
    // nie zwei verschiedene Anfragen auf denselben Schlüssel abbilden können.
    static std::string schluessel(const Anfrage& anfrage);

private:
    // Der innere Behandler
    IAnfrageBehandler& innen_;
    // Der Cache
    ErgebnisCache& cache_;
};
