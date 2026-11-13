// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::chrono::system_clock für Zeitpunkte
#include <chrono>

// Schnittstelle für eine Uhr (E7). Der Cache fragt die aktuelle Zeit nur hierüber ab.
// Im Programm liefert SystemUhr die echte Zeit; in Tests liefert eine FakeUhr eine Zeit,
// die der Test selbst vorgibt und vorstellen kann. So lässt sich "ein Tag später" testen, ohne zu warten.
class IUhr {
public:
    // Virtueller Destruktor, damit abgeleitete Klassen über einen Basiszeiger richtig gelöscht werden
    virtual ~IUhr() = default;

    // Liefert die aktuelle Zeit
    virtual std::chrono::system_clock::time_point jetzt() const = 0;
};

// Echte Uhr des Systems
class SystemUhr : public IUhr {
public:
    // Fragt die Systemzeit ab
    std::chrono::system_clock::time_point jetzt() const override { return std::chrono::system_clock::now(); }
};
