#pragma once

#include <cstddef>

// Beispielklasse der Vorlage: laufende Statistik über Messwerte.
// Darf gelöscht werden, sobald es eigenen Code und eigene Tests gibt.
class Statistik {
public:
    void hinzufuegen(double wert);

    std::size_t anzahl() const;
    double mittelwert() const;   // wirft std::logic_error, wenn anzahl() == 0
    double minimum() const;      // dito
    double maximum() const;      // dito

private:
    std::size_t anzahl_ = 0;
    double summe_ = 0.0;
    double minimum_ = 0.0;
    double maximum_ = 0.0;
};
