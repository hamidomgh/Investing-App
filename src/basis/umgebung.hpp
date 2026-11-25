// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::string für Name und Wert der Umgebungsvariable
#include <string>

// Liest einen API-Key aus der Umgebungsvariable "variable" (z. B. "ALPHAVANTAGE_API_KEY").
// Der Schlüssel steht so nie im Quellcode oder im Repository.
// Wirft KonfigurationsFehler, wenn die Variable fehlt oder leer ist. Die Meldung nennt den Namen der Variable,
// damit man weiß, was zu setzen ist. Aufgerufen wird das vor der ersten Anfrage (DoD E2).
std::string leseApiSchluessel(const std::string& variable);
