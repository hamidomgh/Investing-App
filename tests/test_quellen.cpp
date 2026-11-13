// Die Teile, die hier getestet werden
#include "fake_http_client.hpp"
#include "http_client.hpp"
#include "quellenfehler.hpp"
#include "umgebung.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// setenv und unsetenv (POSIX), um Umgebungsvariablen im Test zu setzen und zu löschen
#include <cstdlib>
// std::string
#include <string>

// ---------------------------------------------------------------- Fehler-Hierarchie

// Jeder spezielle Fehler lässt sich als QuellenFehler fangen; eine Zeitüberschreitung auch als NetzwerkFehler
TEST(Quellenfehler, HierarchieErlaubtAllgemeinesUndGezieltesFangen) {
    // Zeitüberschreitung ist ein Netzwerkfehler
    EXPECT_THROW(throw ZeitueberschreitungFehler("t"), NetzwerkFehler);
    // Netzwerkfehler ist ein Quellenfehler
    EXPECT_THROW(throw NetzwerkFehler("n"), QuellenFehler);
    // Alle übrigen sind ebenfalls Quellenfehler
    EXPECT_THROW(throw HttpFehler(500, "h"), QuellenFehler);
    EXPECT_THROW(throw KontingentFehler("k"), QuellenFehler);
    EXPECT_THROW(throw ParseFehler("p"), QuellenFehler);
    EXPECT_THROW(throw KonfigurationsFehler("c"), QuellenFehler);
    // Und alle sind std::runtime_error, also auch mit Standardmitteln fangbar
    EXPECT_THROW(throw ParseFehler("p"), std::runtime_error);
}

// Ein Kontingentfehler ist bewusst KEIN Netzwerkfehler (E7 behandelt beide, aber getrennt unterscheidbar)
TEST(Quellenfehler, KontingentIstKeinNetzwerkfehler) {
    // Merkt sich, ob der falsche catch-Block erreicht wurde
    bool alsNetzwerkGefangen = false;
    // Kontingentfehler werfen und nur als NetzwerkFehler fangen
    try {
        // Fehler werfen
        throw KontingentFehler("k");
    } catch (const NetzwerkFehler&) {
        // Darf nicht passieren
        alsNetzwerkGefangen = true;
    } catch (const QuellenFehler&) {
        // Hier soll er landen
    }
    // Prüfen
    EXPECT_FALSE(alsNetzwerkGefangen);
}

// HttpFehler merkt sich den Statuscode
TEST(Quellenfehler, HttpFehlerKenntStatus) {
    // Fehler mit Status 404
    const HttpFehler f(404, "nicht gefunden");
    // Status abfragen
    EXPECT_EQ(f.status(), 404);
    // Meldung abfragen
    EXPECT_STREQ(f.what(), "nicht gefunden");
}

// ---------------------------------------------------------------- pruefeStatus

// Statuscodes 200 bis 299 sind in Ordnung
TEST(PruefeStatus, ErfolgWirftNicht) {
    // 200 OK
    EXPECT_NO_THROW(pruefeStatus({200, "{}"}, "test"));
    // 204 No Content
    EXPECT_NO_THROW(pruefeStatus({204, ""}, "test"));
}

// 401 und 403: HttpFehler mit Hinweis auf den API-Key
TEST(PruefeStatus, UngueltigerSchluessel) {
    // Meldung und Status abfangen
    try {
        // 401 auswerten
        pruefeStatus({401, ""}, "prices for AAPL");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status 401
        EXPECT_EQ(f.status(), 401);
        // Meldung nennt den Kontext und den Schlüssel
        const std::string m = f.what();
        EXPECT_NE(m.find("AAPL"), std::string::npos) << m;
        EXPECT_NE(m.find("API key"), std::string::npos) << m;
    }
    // 403 wird genauso behandelt
    EXPECT_THROW(pruefeStatus({403, ""}, "test"), HttpFehler);
}

// 404: HttpFehler mit Status 404 (z. B. unbekanntes Symbol)
TEST(PruefeStatus, NichtGefunden) {
    // Status abfangen
    try {
        // 404 auswerten
        pruefeStatus({404, ""}, "prices for XYZ");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status 404
        EXPECT_EQ(f.status(), 404);
    }
}

// 429: eigener KontingentFehler
TEST(PruefeStatus, KontingentErschoepft) {
    // 429 auswerten
    EXPECT_THROW(pruefeStatus({429, ""}, "test"), KontingentFehler);
}

// Andere Fehlerstatus: allgemeiner HttpFehler mit dem Code
TEST(PruefeStatus, SonstigerFehlerstatus) {
    // Status abfangen
    try {
        // 500 auswerten
        pruefeStatus({500, ""}, "test");
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const HttpFehler& f) {
        // Status 500
        EXPECT_EQ(f.status(), 500);
    }
}

// ---------------------------------------------------------------- FakeHttpClient

// Festgelegte Antworten werden geliefert und alle Anfragen protokolliert
TEST(FakeHttpClient, LiefertFestgelegteAntwortUndProtokolliert) {
    // Fake anlegen
    FakeHttpClient fake;
    // Antwort festlegen
    fake.setzeAntwort("https://example.test/a", 200, "{\"ok\":true}");
    // Über die Schnittstelle aufrufen, wie es der echte Code tun wird
    IHttpClient& client = fake;
    // Anfrage stellen
    const HttpAntwort a = client.get("https://example.test/a");
    // Status und Inhalt stimmen
    EXPECT_EQ(a.status, 200);
    EXPECT_EQ(a.body, "{\"ok\":true}");
    // Genau eine Anfrage protokolliert
    ASSERT_EQ(fake.anfragen().size(), 1u);
    EXPECT_EQ(fake.anfragen()[0], "https://example.test/a");
}

// Fehlerstatus wird zurückgegeben, nicht geworfen (Vertrag von IHttpClient)
TEST(FakeHttpClient, FehlerstatusWirdZurueckgegeben) {
    // Fake mit 404-Antwort
    FakeHttpClient fake;
    fake.setzeAntwort("https://example.test/x", 404, "");
    // Kein Fehler beim Abruf selbst
    EXPECT_EQ(fake.get("https://example.test/x").status, 404);
}

// Simulierte Ausfälle werfen die passenden Fehler
TEST(FakeHttpClient, SimulierteAusfaelle) {
    // Fake anlegen
    FakeHttpClient fake;
    // Zwei Ausfälle festlegen
    fake.setzeAusfall("https://example.test/n", FakeHttpClient::Ausfall::Netzwerk);
    fake.setzeAusfall("https://example.test/t", FakeHttpClient::Ausfall::Zeitueberschreitung);
    // Netzwerkfehler
    EXPECT_THROW(fake.get("https://example.test/n"), NetzwerkFehler);
    // Zeitüberschreitung
    EXPECT_THROW(fake.get("https://example.test/t"), ZeitueberschreitungFehler);
    // Unbekannte URL verhält sich wie ein nicht erreichbarer Server
    EXPECT_THROW(fake.get("https://example.test/unbekannt"), NetzwerkFehler);
}

// ---------------------------------------------------------------- API-Key aus der Umgebung

// Name einer Variable, die nur in diesen Tests verwendet wird
static const char* const testVariable = "MARKETLENS_TEST_API_KEY";

// Gesetzte Variable wird gelesen
TEST(ApiSchluessel, WirdAusUmgebungGelesen) {
    // Variable setzen (1 = vorhandenen Wert überschreiben)
    setenv(testVariable, "abc123", 1);
    // Wert lesen
    EXPECT_EQ(leseApiSchluessel(testVariable), "abc123");
    // Aufräumen, damit andere Tests nicht beeinflusst werden
    unsetenv(testVariable);
}

// Fehlende Variable: KonfigurationsFehler, Meldung nennt den Namen der Variable
TEST(ApiSchluessel, FehlendeVariable) {
    // Sicherstellen, dass die Variable nicht gesetzt ist
    unsetenv(testVariable);
    // Meldung abfangen
    try {
        // Lesen versuchen
        leseApiSchluessel(testVariable);
        // Hierher darf der Test nicht kommen
        FAIL() << "keine Exception geworfen";
    } catch (const KonfigurationsFehler& f) {
        // Meldung enthält den Namen der Variable
        EXPECT_NE(std::string(f.what()).find(testVariable), std::string::npos) << f.what();
    }
}

// Leere Variable zählt wie eine fehlende
TEST(ApiSchluessel, LeereVariable) {
    // Variable auf leeren Text setzen
    setenv(testVariable, "", 1);
    // Fehler erwartet
    EXPECT_THROW(leseApiSchluessel(testVariable), KonfigurationsFehler);
    // Aufräumen
    unsetenv(testVariable);
}
