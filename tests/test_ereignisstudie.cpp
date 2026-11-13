// Die Klassen, die hier getestet werden
#include "ereignisstudie.hpp"
#include "zinsentscheidung.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::chrono für Kalendertage
#include <chrono>
// std::numeric_limits für NaN und unendlich
#include <limits>
// std::invalid_argument für die erwarteten Fehler
#include <stdexcept>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Kurzschreibweise für einen Kalendertag, z. B. tag(2024, 1, 3)
std::chrono::sys_days tag(int jahr, unsigned monat, unsigned t) {
    // Jahr/Monat/Tag zu einem sys_days zusammensetzen
    return std::chrono::year{jahr} / std::chrono::month{monat} / std::chrono::day{t};
}

// Ein Handelstag mit Schlusskurs
struct Handelstag {
    // Datum
    std::chrono::sys_days datum;
    // Schlusskurs
    double close;
};

// Baut eine Kursreihe aus Handelstagen (Open = High = Low = Close)
Kursreihe reiheAus(const std::vector<Handelstag>& tage) {
    // Leere Kursreihe
    Kursreihe reihe;
    // Für jeden Handelstag eine Kerze anhängen
    for (const Handelstag& h : tage) {
        // Alle vier Preise gleich, Volumen 1
        reihe.hinzufuegen(Kerze(h.datum, h.close, h.close, h.close, h.close, 1.0));
    }
    // Fertige Reihe
    return reihe;
}

// Testreihe: zwei Handelswochen im Januar 2024 (der 1. Januar 2024 war ein Montag).
// Samstag 6. und Sonntag 7. Januar fehlen, weil dort nicht gehandelt wird.
Kursreihe testreihe() {
    // Mo 1, Di 2, Mi 3, Do 4, Fr 5, Mo 8, Di 9
    return reiheAus({
        {tag(2024, 1, 1), 100},
        {tag(2024, 1, 2), 102},
        {tag(2024, 1, 3), 101},
        {tag(2024, 1, 4), 105},
        {tag(2024, 1, 5), 104},
        {tag(2024, 1, 8), 110},
        {tag(2024, 1, 9), 99},
    });
}

// Ein US-Wertpapier (passt zur Fed)
const Wertpapier usAktie("AAPL", "Apple Inc.", "USA", "USD");

// Erlaubte Rundungsabweichung
constexpr double eps = 1e-9;

}  // namespace

// ---------------------------------------------------------------- Zinsentscheidung

// Eine gültige Zinsentscheidung wird angenommen, auch mit negativem Leitzins
TEST(Zinsentscheidung, GueltigAuchMitNegativemZins) {
    // Normaler Leitzins
    EXPECT_NO_THROW(Zinsentscheidung("Fed", "USA", tag(2024, 1, 31), 5.5));
    // Negativer Leitzins (gab es bei EZB und Bank of Japan wirklich)
    EXPECT_NO_THROW(Zinsentscheidung("EZB", "Eurozone", tag(2019, 9, 12), -0.5));
}

// Ungültige Felder werden abgelehnt
TEST(Zinsentscheidung, UngueltigeFelderWerdenAbgelehnt) {
    // Leere Zentralbank
    EXPECT_THROW(Zinsentscheidung("", "USA", tag(2024, 1, 31), 5.5), std::invalid_argument);
    // Leeres Land
    EXPECT_THROW(Zinsentscheidung("Fed", "", tag(2024, 1, 31), 5.5), std::invalid_argument);
    // NaN als Leitzins (z. B. nach fehlerhaftem Parsen)
    EXPECT_THROW(Zinsentscheidung("Fed", "USA", tag(2024, 1, 31), std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
    // Unendlich als Leitzins
    EXPECT_THROW(Zinsentscheidung("Fed", "USA", tag(2024, 1, 31), std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
}

// Die Fehlermeldung nennt Zentralbank und Datum
TEST(Zinsentscheidung, FehlermeldungNenntBankUndDatum) {
    // Meldung des Fehlers abfangen
    try {
        // Leeres Land ist ungültig
        Zinsentscheidung("Fed", "", tag(2024, 3, 20), 5.5);
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const std::invalid_argument& e) {
        // Meldung als String
        const std::string m = e.what();
        // Datum enthalten
        EXPECT_NE(m.find("2024-03-20"), std::string::npos) << m;
        // Zentralbank enthalten
        EXPECT_NE(m.find("Fed"), std::string::npos) << m;
    }
}

// ---------------------------------------------------------------- Ereignisstudie

// Entscheidung an einem Handelstag (Mittwoch 3. Januar):
//   Reaktionstag = Donnerstag 4. Januar, Kurs vorher 101 (Mittwoch), nachher 105
//   Veränderung = 105 / 101 - 1 = 0,0396 (+3,96 %)
TEST(Ereignisstudie, EntscheidungAnHandelstag) {
    // Eine Fed-Entscheidung
    Ereignisstudie studie({Zinsentscheidung("Fed", "USA", tag(2024, 1, 3), 5.5)});
    // Auswertung für die US-Aktie
    const auto ergebnis = studie.auswerten(usAktie, testreihe());
    // Genau ein Ereignis
    ASSERT_EQ(ergebnis.size(), 1u);
    // Reaktionstag ist der Donnerstag
    EXPECT_EQ(ergebnis[0].reaktionstag, tag(2024, 1, 4));
    // Kurse vorher und nachher
    EXPECT_NEAR(ergebnis[0].kursVorher, 101.0, eps);
    EXPECT_NEAR(ergebnis[0].kursNachher, 105.0, eps);
    // Veränderung
    EXPECT_NEAR(ergebnis[0].veraenderung, 105.0 / 101.0 - 1.0, eps);
}

// Entscheidung an einem Samstag (6. Januar) wird auf den nächsten Handelstag (Montag 8.) abgebildet:
//   Kurs vorher 104 (Freitag 5.), nachher 110 (Montag 8.), Veränderung = 110 / 104 - 1 = 0,0577
TEST(Ereignisstudie, NichtHandelstagWirdAufNaechstenHandelstagAbgebildet) {
    // Entscheidungen an Samstag und Sonntag
    Ereignisstudie studie({
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 6), 5.5),
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 7), 5.5),
    });
    // Auswertung
    const auto ergebnis = studie.auswerten(usAktie, testreihe());
    // Beide werden ausgewertet
    ASSERT_EQ(ergebnis.size(), 2u);
    // Beide landen auf dem Montag mit demselben Ergebnis
    for (const Ereignis& e : ergebnis) {
        // Reaktionstag Montag 8. Januar
        EXPECT_EQ(e.reaktionstag, tag(2024, 1, 8));
        // Kurs vorher ist der Freitag
        EXPECT_NEAR(e.kursVorher, 104.0, eps);
        // Veränderung
        EXPECT_NEAR(e.veraenderung, 110.0 / 104.0 - 1.0, eps);
    }
}

// Entscheidungen außerhalb des Datenzeitraums werden übersprungen
TEST(Ereignisstudie, AusserhalbDesZeitraumsWirdUebersprungen) {
    // Vor dem ersten Tag, am letzten Tag (kein Tag danach) und nach dem Ende
    Ereignisstudie studie({
        Zinsentscheidung("Fed", "USA", tag(2023, 12, 31), 5.5),  // Reaktion wäre Mo 1., aber kein Kurs davor
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 9), 5.5),    // letzter Handelstag, kein Tag danach
        Zinsentscheidung("Fed", "USA", tag(2024, 2, 1), 5.5),    // nach dem Ende der Daten
    });
    // Keine Entscheidung kann ausgewertet werden
    EXPECT_TRUE(studie.auswerten(usAktie, testreihe()).empty());
}

// Entscheidung am ersten Handelstag: Kurs vorher ist dieser Tag, Reaktion am zweiten Tag
TEST(Ereignisstudie, EntscheidungAmErstenHandelstag) {
    // Entscheidung am Montag 1. Januar
    Ereignisstudie studie({Zinsentscheidung("Fed", "USA", tag(2024, 1, 1), 5.5)});
    // Auswertung
    const auto ergebnis = studie.auswerten(usAktie, testreihe());
    // Ein Ereignis
    ASSERT_EQ(ergebnis.size(), 1u);
    // 102 / 100 - 1 = 0,02
    EXPECT_NEAR(ergebnis[0].veraenderung, 0.02, eps);
}

// Nur die Zentralbank des eigenen Landes zählt; ohne passende Zentralbank ist das Ergebnis leer
TEST(Ereignisstudie, NurZentralbankDesEigenenLandes) {
    // Entscheidungen von Fed und EZB
    Ereignisstudie studie({
        Zinsentscheidung("EZB", "Eurozone", tag(2024, 1, 2), 4.5),
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 3), 5.5),
    });
    // US-Aktie: nur die Fed-Entscheidung
    const auto us = studie.auswerten(usAktie, testreihe());
    ASSERT_EQ(us.size(), 1u);
    EXPECT_EQ(us[0].entscheidung.zentralbank, "Fed");
    // Japanische Aktie: keine passende Zentralbank -> leere Liste, kein Fehler
    const Wertpapier jpAktie("7203.T", "Toyota", "Japan", "JPY");
    EXPECT_TRUE(studie.auswerten(jpAktie, testreihe()).empty());
}

// Unsortierte Eingabe: das Ergebnis ist trotzdem nach Datum sortiert
TEST(Ereignisstudie, ErgebnisIstNachDatumSortiert) {
    // Entscheidungen in falscher Reihenfolge übergeben
    Ereignisstudie studie({
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 6), 5.5),
        Zinsentscheidung("Fed", "USA", tag(2024, 1, 3), 5.25),
    });
    // Auswertung
    const auto ergebnis = studie.auswerten(usAktie, testreihe());
    // Zwei Ereignisse
    ASSERT_EQ(ergebnis.size(), 2u);
    // Zuerst der 3., dann der 6. Januar
    EXPECT_EQ(ergebnis[0].entscheidung.datum, tag(2024, 1, 3));
    EXPECT_EQ(ergebnis[1].entscheidung.datum, tag(2024, 1, 6));
}

// Leere Kursreihe oder keine Entscheidungen: leeres Ergebnis
TEST(Ereignisstudie, LeereEingaben) {
    // Keine Kurse
    Ereignisstudie mitEntscheidung({Zinsentscheidung("Fed", "USA", tag(2024, 1, 3), 5.5)});
    EXPECT_TRUE(mitEntscheidung.auswerten(usAktie, Kursreihe()).empty());
    // Keine Entscheidungen
    Ereignisstudie ohneEntscheidung({});
    EXPECT_TRUE(ohneEntscheidung.auswerten(usAktie, testreihe()).empty());
}

// Ein Kurs von 0 vor dem Reaktionstag wird abgelehnt (Division durch 0)
TEST(Ereignisstudie, KursNullWirdAbgelehnt) {
    // Kurs 0 am Mittwoch (laut E1 eine gültige Kerze)
    const Kursreihe reihe = reiheAus({{tag(2024, 1, 3), 0}, {tag(2024, 1, 4), 105}});
    // Entscheidung am Mittwoch: Kurs vorher wäre 0
    Ereignisstudie studie({Zinsentscheidung("Fed", "USA", tag(2024, 1, 3), 5.5)});
    // Fehler statt Division durch 0
    EXPECT_THROW(studie.auswerten(usAktie, reihe), std::invalid_argument);
}
