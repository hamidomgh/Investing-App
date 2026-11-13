// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::chrono::sys_days: ein Kalendertag ohne Uhrzeit (C++20)
#include <chrono>
// std::string für Namen
#include <string>

// Eine Zinsentscheidung einer Zentralbank (E6): welche Bank, welches Land, wann, welcher Leitzins
struct Zinsentscheidung {
    // Name der Zentralbank, z. B. "Fed", "EZB", "Bank of England"
    std::string zentralbank;
    // Land bzw. Währungsraum der Zentralbank; muss zum Land der Wertpapiere passen (z. B. "USA")
    std::string land;
    // Tag der Entscheidung (nur Datum, ohne Uhrzeit)
    std::chrono::sys_days datum;
    // Leitzins nach der Entscheidung in Prozent (z. B. 5.25). Negative Werte sind erlaubt (EZB 2014-2022).
    double leitzins;

    // Konstruktor: setzt alle Felder und prüft sie sofort (wie bei Kerze in E1).
    // Wirft std::invalid_argument bei leerer Zentralbank, leerem Land oder nicht endlichem Leitzins.
    Zinsentscheidung(std::string zentralbank, std::string land, std::chrono::sys_days datum, double leitzins);

    // Prüft alle Felder; die Fehlermeldung nennt Zentralbank und Datum
    void validieren() const;
};
