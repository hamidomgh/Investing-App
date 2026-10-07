#include "statistik.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

// Aufbau eines Tests: TEST(Gruppe, Name) { Vorbereiten; Ausführen; Prüfen; }

TEST(Statistik, IstAnfangsLeer) {
    Statistik s;
    EXPECT_EQ(s.anzahl(), 0u);
}

TEST(Statistik, BerechnetMittelwertMinimumMaximum) {
    Statistik s;
    s.hinzufuegen(3.0);
    s.hinzufuegen(-1.0);
    s.hinzufuegen(4.0);

    EXPECT_EQ(s.anzahl(), 3u);
    EXPECT_DOUBLE_EQ(s.mittelwert(), 2.0);
    EXPECT_DOUBLE_EQ(s.minimum(), -1.0);
    EXPECT_DOUBLE_EQ(s.maximum(), 4.0);
}

TEST(Statistik, WirftOhneWerte) {
    Statistik s;
    EXPECT_THROW(s.mittelwert(), std::logic_error);
    EXPECT_THROW(s.minimum(), std::logic_error);
    EXPECT_THROW(s.maximum(), std::logic_error);
}

TEST(Statistik, LehntNichtEndlicheWerteAb) {
    Statistik s;
    EXPECT_THROW(s.hinzufuegen(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(s.hinzufuegen(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_EQ(s.anzahl(), 0u);   // abgelehnte Werte verändern den Zustand nicht
}
