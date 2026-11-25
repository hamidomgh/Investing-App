// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::string
#include <string>
// std::vector für die Liste der Länder
#include <vector>

// Eine Zentralbank aus dem Katalog (E6): wofür sie zuständig ist und wo ihre Zinsdaten liegen
struct Zentralbank {
    // Name, z. B. "Fed" oder "EZB"
    std::string name;
    // Alle Länder, für die sie den Leitzins festlegt. Für die EZB alle Länder des Euroraums.
    // Ein Wertpapier gehört zu der Zentralbank, deren Liste sein Land enthält.
    std::vector<std::string> laender;
    // Währung, z. B. "USD"
    std::string waehrung;
    // Kennung der Zeitreihe bei FRED, z. B. "DFEDTARU" (Obergrenze des Fed-Zielbands, täglich)
    std::string fredSerie;

    // Konstruktor: setzt und prüft alle Felder (wie Wertpapier in E1).
    // Wirft std::invalid_argument, wenn ein Feld leer ist, die Länderliste leer ist oder ein Land leer ist.
    Zentralbank(std::string name, std::vector<std::string> laender, std::string waehrung, std::string fredSerie);

    // true, wenn die Zentralbank für dieses Land zuständig ist
    bool zustaendigFuer(const std::string& land) const;
};
