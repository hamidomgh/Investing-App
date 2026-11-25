#include "statistik.hpp"

#include <cmath>
#include <stdexcept>

void Statistik::hinzufuegen(double wert) {
    if (!std::isfinite(wert)) {
        throw std::invalid_argument("Statistik: Wert ist nicht endlich");
    }
    if (anzahl_ == 0 || wert < minimum_) {
        minimum_ = wert;
    }
    if (anzahl_ == 0 || wert > maximum_) {
        maximum_ = wert;
    }
    summe_ += wert;
    ++anzahl_;
}

std::size_t Statistik::anzahl() const {
    return anzahl_;
}

static void pruefe_nicht_leer(std::size_t anzahl) {
    if (anzahl == 0) {
        throw std::logic_error("Statistik: noch keine Werte");
    }
}

double Statistik::mittelwert() const {
    pruefe_nicht_leer(anzahl_);
    return summe_ / static_cast<double>(anzahl_);
}

double Statistik::minimum() const {
    pruefe_nicht_leer(anzahl_);
    return minimum_;
}

double Statistik::maximum() const {
    pruefe_nicht_leer(anzahl_);
    return maximum_;
}
