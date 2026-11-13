// Die Klasse, die hier getestet wird
#include "dateiquelle.hpp"
// Fehlerklassen
#include "quellenfehler.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::string
#include <string>

// Ordner mit den Alpha-Vantage-Testdaten
static const std::string ordner = std::string(FIXTURES_DIR) + "/alphavantage";

// Gültige Datei wird gelesen und geparst
TEST(DateiQuelle, LiestGespeicherteAntwort) {
    // Quelle auf den Testordner
    DateiQuelle quelle(ordner);
    // Die Datei heißt daily_ok.json, also Symbol "daily_ok"
    const Kursreihe r = quelle.ladeKurse(Wertpapier("daily_ok", "Test", "USA", "USD"));
    // Drei Tage wie in der Datei
    EXPECT_EQ(r.groesse(), 3u);
}

// Fehlende Datei: QuellenFehler mit dem Pfad in der Meldung
TEST(DateiQuelle, FehlendeDatei) {
    // Quelle auf den Testordner
    DateiQuelle quelle(ordner);
    // Meldung abfangen
    try {
        // Datei gibt es nicht
        quelle.ladeKurse(Wertpapier("GIBTSNICHT", "Test", "USA", "USD"));
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const QuellenFehler& f) {
        // Pfad enthalten
        EXPECT_NE(std::string(f.what()).find("GIBTSNICHT.json"), std::string::npos) << f.what();
    }
}

// Inhalt wird wie eine echte Antwort geprüft: Fehlerhinweise und kaputtes JSON führen zu den gleichen Fehlern
TEST(DateiQuelle, InhaltWirdGeprueft) {
    // Quelle auf den Testordner
    DateiQuelle quelle(ordner);
    // "Error Message" -> HttpFehler (wie 404)
    EXPECT_THROW(quelle.ladeKurse(Wertpapier("error_message", "T", "USA", "USD")), HttpFehler);
    // Abgeschnittenes JSON -> ParseFehler
    EXPECT_THROW(quelle.ladeKurse(Wertpapier("ungueltiges_json", "T", "USA", "USD")), ParseFehler);
}

// Symbole, die aus dem Ordner herausführen würden, werden abgelehnt
TEST(DateiQuelle, UnzulaessigeSymbole) {
    // Quelle auf den Testordner
    DateiQuelle quelle(ordner);
    // Pfadtrenner und übergeordneter Ordner
    EXPECT_THROW(quelle.dateiFuer("../geheim"), QuellenFehler);
    EXPECT_THROW(quelle.dateiFuer("a/b"), QuellenFehler);
    EXPECT_THROW(quelle.dateiFuer(".."), QuellenFehler);
    EXPECT_THROW(quelle.dateiFuer("a b"), QuellenFehler);
    // Übliche Symbole mit Börsen-Suffix und Index-Zeichen sind erlaubt
    EXPECT_NO_THROW(quelle.dateiFuer("MBG.DEX"));
    EXPECT_NO_THROW(quelle.dateiFuer("^GDAXI"));
    EXPECT_NO_THROW(quelle.dateiFuer("BRK-B"));
}
