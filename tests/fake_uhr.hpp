// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Die Schnittstelle, die der Fake erfüllt
#include "uhr.hpp"

// Fake-Uhr nur für Tests: zeigt eine feste Zeit, die der Test setzen und vorstellen kann
class FakeUhr : public IUhr {
public:
    // Startzeit festlegen
    explicit FakeUhr(std::chrono::system_clock::time_point start) : jetzt_(start) {}

    // Liefert die eingestellte Zeit
    std::chrono::system_clock::time_point jetzt() const override { return jetzt_; }

    // Stellt die Uhr um eine Dauer vor (oder mit negativer Dauer zurück)
    void vorstellen(std::chrono::system_clock::duration dauer) { jetzt_ += dauer; }

private:
    // Die eingestellte Zeit
    std::chrono::system_clock::time_point jetzt_;
};
