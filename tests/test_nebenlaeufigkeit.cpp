// Die Teile, die hier getestet werden
#include "anfragebehandlung.hpp"
#include "ergebnis_cache.hpp"
#include "gecachte_anfragebehandlung.hpp"
#include "synchronisierte_quelle.hpp"
#include "threadpool.hpp"
// Fake-Uhr für die Gültigkeit
#include "fake_uhr.hpp"

// GoogleTest-Framework
#include <gtest/gtest.h>

// std::shuffle
#include <algorithm>
// std::atomic: Zähler, die mehrere Threads gleichzeitig ändern dürfen
#include <atomic>
// std::chrono
#include <chrono>
// std::sin
#include <cmath>
// std::condition_variable
#include <condition_variable>
// std::future
#include <future>
// std::map
#include <map>
// std::mutex
#include <mutex>
// std::mt19937: Zufallszahlen mit festem Startwert (Seed), also bei jedem Lauf gleich
#include <random>
// std::set
#include <set>
// std::runtime_error
#include <stdexcept>
// std::string
#include <string>
// std::thread
#include <thread>
// std::vector
#include <vector>

// Kurze Schreibweise für Zeitangaben (z. B. 5min, 100ms)
using namespace std::chrono_literals;

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Feste Startzeit für die Fake-Uhr
const std::chrono::system_clock::time_point startzeit = std::chrono::sys_days{std::chrono::year{2026} / 11 / 13};

// Testreihe mit 60 Tagen; "versatz" macht die Reihen verschiedener Symbole unterschiedlich
Kursreihe testreihe(double versatz) {
    // Leere Reihe
    Kursreihe reihe;
    // Startdatum
    const std::chrono::sys_days start = std::chrono::year{2026} / 1 / 1;
    // 60 Tage
    for (int i = 0; i < 60; ++i) {
        // Schlusskurs mit Welle und Trend
        const double c = 100.0 + versatz + 10.0 * std::sin(i / 5.0 + versatz) + 0.2 * i;
        // Gültige Kerze anhängen
        reihe.hinzufuegen(Kerze(start + std::chrono::days{i}, c, c + 1.0, c - 1.0, c, 1000.0));
    }
    // Fertige Reihe
    return reihe;
}

// Einfache Testquelle: jedes Symbol bekommt eine eigene, feste Reihe; zählt die Ladevorgänge.
// NICHT thread-sicher (ein gewöhnliches int als Zähler) – genau dafür gibt es SynchronisierteQuelle.
class TestQuelle : public IPriceSource {
public:
    // Kurse laden
    Kursreihe ladeKurse(const Wertpapier& wp) override {
        // Zählen (nicht atomar!)
        ++ladungen;
        // Versatz aus der Länge des Symbols: IBM 3, AAPL 4, 7203.T 6
        return testreihe(static_cast<double>(wp.symbol.size()));
    }
    // Anzahl Ladevorgänge
    int ladungen = 0;
};

// Katalog für die Tests
std::vector<Wertpapier> katalog() {
    // Drei Wertpapiere aus zwei Ländern
    return {Wertpapier("IBM", "IBM", "USA", "USD"), Wertpapier("AAPL", "Apple", "USA", "USD"),
            Wertpapier("7203.T", "Toyota", "Japan", "JPY")};
}

// Zinsentscheidungen für die Tests
std::vector<Zinsentscheidung> entscheidungen() {
    // Zwei Fed-Entscheidungen
    return {Zinsentscheidung("Fed", "USA", std::chrono::year{2026} / 1 / 10, 4.25),
            Zinsentscheidung("Fed", "USA", std::chrono::year{2026} / 2 / 5, 4.0)};
}

// Behandler-Decorator, der zählt, wie oft wirklich gerechnet wurde (thread-sicher dank std::atomic)
class ZaehlenderBehandler : public IAnfrageBehandler {
public:
    // Inneren Behandler merken
    explicit ZaehlenderBehandler(IAnfrageBehandler& innen) : innen_(innen) {}
    // Zählen und weitergeben
    Antwort behandle(const Anfrage& a) override {
        // Atomar erhöhen: gleichzeitige Erhöhungen gehen nicht verloren
        ++aufrufe;
        // Weitergeben
        return innen_.behandle(a);
    }
    // Anzahl der Aufrufe
    std::atomic<int> aufrufe{0};

private:
    // Innerer Behandler
    IAnfrageBehandler& innen_;
};

// Erzeugt "anzahl" zufällige Anfragen mit festem Seed: bei jedem Testlauf exakt dieselbe Folge.
// Mischt gültige, ungültige (400), unbekannte Symbole (404), zu kurze Reihen (422) und unbekannte Pfade.
std::vector<Anfrage> zufallsAnfragen(std::size_t anzahl, unsigned seed) {
    // Zufallsgenerator mit festem Startwert
    std::mt19937 zufall(seed);
    // Auswahl aus einer Liste mit gleicher Wahrscheinlichkeit
    auto waehle = [&zufall](const std::vector<std::string>& liste) {
        // Gleichverteilter Index
        std::uniform_int_distribution<std::size_t> index(0, liste.size() - 1);
        // Element
        return liste[index(zufall)];
    };
    // Mögliche Werte
    const std::vector<std::string> pfade = {"/api/wertpapiere", "/api/kurse", "/api/indikatoren",
                                            "/api/ereignisstudie", "/api/gibtsnicht"};
    const std::vector<std::string> symbole = {"IBM", "AAPL", "7203.T", "MSFT"};
    const std::vector<std::string> smaWerte = {"", "5", "10", "20", "0", "100"};
    const std::vector<std::string> vonWerte = {"", "2026-01-15", "2026-02-20", "2026-13-01"};
    const std::vector<std::string> macdWerte = {"", "12,26,9", "3,6,2", "26,12,9"};
    // Ergebnisliste
    std::vector<Anfrage> anfragen;
    // Platz reservieren
    anfragen.reserve(anzahl);
    // Anfragen erzeugen
    for (std::size_t i = 0; i < anzahl; ++i) {
        // Neue Anfrage
        Anfrage a;
        // Pfad
        a.pfad = waehle(pfade);
        // Symbol (außer bei der Liste)
        if (a.pfad != "/api/wertpapiere") {
            // Symbol setzen
            a.parameter["symbol"] = waehle(symbole);
        }
        // Optionaler Startzeitpunkt
        if (const std::string von = waehle(vonWerte); !von.empty() && a.pfad != "/api/wertpapiere") {
            // Setzen
            a.parameter["von"] = von;
        }
        // Perioden nur bei den Indikatoren
        if (a.pfad == "/api/indikatoren") {
            // SMA optional
            if (const std::string sma = waehle(smaWerte); !sma.empty()) {
                // Setzen
                a.parameter["sma"] = sma;
            }
            // MACD optional
            if (const std::string macd = waehle(macdWerte); !macd.empty()) {
                // Setzen
                a.parameter["macd"] = macd;
            }
        }
        // Anhängen
        anfragen.push_back(a);
    }
    // Fertige Liste
    return anfragen;
}

}  // namespace

// ================================================================ ThreadPool

// Ergebnisse kommen über die futures beim Aufrufer an
TEST(ThreadPool, ErgebnisseUeberFutures) {
    // Pool mit 4 Threads
    ThreadPool pool(4);
    // 100 Aufgaben einreihen
    std::vector<std::future<int>> zukuenfte;
    for (int i = 0; i < 100; ++i) {
        // Aufgabe: Quadrat berechnen
        zukuenfte.push_back(pool.einreihen([i] { return i * i; }));
    }
    // Summe der Quadrate 0..99 = 328350
    int summe = 0;
    for (auto& z : zukuenfte) {
        // Wartet auf das jeweilige Ergebnis
        summe += z.get();
    }
    // Vergleich mit der Formel n(n+1)(2n+1)/6 für n = 99
    EXPECT_EQ(summe, 328350);
}

// Der Pool arbeitet wirklich parallel: alle 4 Threads sind gleichzeitig in einer Aufgabe
TEST(ThreadPool, ArbeitetParallel) {
    // Pool mit 4 Threads
    ThreadPool pool(4);
    // Gemeinsamer Zähler und Signal
    std::mutex m;
    std::condition_variable cv;
    int gleichzeitig = 0;
    // Jede Aufgabe wartet (höchstens 5 s), bis alle 4 gleichzeitig laufen
    auto aufgabe = [&] {
        // Sperren
        std::unique_lock<std::mutex> sperre(m);
        // Eine mehr läuft
        ++gleichzeitig;
        // Alle wecken
        cv.notify_all();
        // Warten, bis 4 erreicht sind; true, wenn es geklappt hat
        return cv.wait_for(sperre, 5s, [&] { return gleichzeitig >= 4; });
    };
    // 4 Aufgaben starten
    std::vector<std::future<bool>> zukuenfte;
    for (int i = 0; i < 4; ++i) {
        // Einreihen
        zukuenfte.push_back(pool.einreihen(aufgabe));
    }
    // Jede Aufgabe hat die anderen drei gleichzeitig gesehen (sonst wäre nach 5 s false gekommen)
    for (auto& z : zukuenfte) {
        // Prüfen
        EXPECT_TRUE(z.get());
    }
}

// Eine Exception in der Aufgabe kommt bei future.get() an, und der Pool arbeitet weiter
TEST(ThreadPool, ExceptionKommtImFutureAn) {
    // Pool mit 2 Threads
    ThreadPool pool(2);
    // Aufgabe, die wirft
    auto fehler = pool.einreihen([]() -> int { throw std::runtime_error("kaputt"); });
    // Exception beim Abholen
    EXPECT_THROW(fehler.get(), std::runtime_error);
    // Der Pool funktioniert weiterhin
    EXPECT_EQ(pool.einreihen([] { return 42; }).get(), 42);
}

// beende() arbeitet die Warteschlange noch ab und wartet auf alle Threads
TEST(ThreadPool, BeendeArbeitetWarteschlangeAb) {
    // Zähler, den mehrere Threads erhöhen
    std::atomic<int> erledigt{0};
    // Pool mit einem Thread, damit sich Aufgaben stauen
    ThreadPool pool(1);
    // 50 Aufgaben
    for (int i = 0; i < 50; ++i) {
        // Kurz arbeiten und zählen
        pool.einreihenOhneErgebnis([&erledigt] {
            // Etwas Zeit vergehen lassen
            std::this_thread::sleep_for(1ms);
            // Zählen
            ++erledigt;
        });
    }
    // Beenden: muss alle 50 noch ausführen
    pool.beende();
    // Alle erledigt
    EXPECT_EQ(erledigt.load(), 50);
}

// Nach beende(): keine neuen Aufgaben, mehrfaches beende() ist harmlos
TEST(ThreadPool, NachBeendenKeineNeuenAufgaben) {
    // Pool
    ThreadPool pool(2);
    // Beenden
    pool.beende();
    // Zweites Beenden: nichts passiert
    EXPECT_NO_THROW(pool.beende());
    // Neue Aufgaben werden abgelehnt
    EXPECT_FALSE(pool.einreihenOhneErgebnis([] {}));
    EXPECT_THROW(pool.einreihen([] { return 1; }), std::runtime_error);
}

// Der Destruktor beendet sauber, auch mit laufenden Aufgaben (kein Hängen, kein Absturz)
TEST(ThreadPool, DestruktorBeendetSauber) {
    // Zähler
    std::atomic<int> erledigt{0};
    // Eigener Block: Pool lebt nur hier
    {
        // Pool mit 4 Threads
        ThreadPool pool(4);
        // 20 Aufgaben
        for (int i = 0; i < 20; ++i) {
            // Kurz arbeiten
            pool.einreihenOhneErgebnis([&erledigt] {
                // Etwas Zeit vergehen lassen
                std::this_thread::sleep_for(2ms);
                // Zählen
                ++erledigt;
            });
        }
        // Hier endet der Block: Destruktor ruft beende()
    }
    // Alle Aufgaben wurden noch ausgeführt
    EXPECT_EQ(erledigt.load(), 20);
}

// Ungültige Benutzung wird abgelehnt
TEST(ThreadPool, UngueltigeBenutzung) {
    // 0 Threads
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);
    // beende() aus einem Thread des Pools würde auf sich selbst warten: logic_error
    ThreadPool pool(1);
    auto z = pool.einreihen([&pool] { pool.beende(); });
    EXPECT_THROW(z.get(), std::logic_error);
}

// ================================================================ ErgebnisCache

// Gleicher Schlüssel: nur einmal berechnet; anderer Schlüssel: neu berechnet
TEST(ErgebnisCache, GleicherSchluesselNurEinmal) {
    // Uhr und Cache
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 100);
    // Zähler
    int berechnet = 0;
    // Berechnung, die zählt
    auto berechne = [&berechnet] {
        // Zählen
        ++berechnet;
        // Antwort mit Inhalt
        Antwort a;
        a.body = "ergebnis";
        return a;
    };
    // Zweimal derselbe Schlüssel
    EXPECT_EQ(cache.hole("a", berechne).body, "ergebnis");
    EXPECT_EQ(cache.hole("a", berechne).body, "ergebnis");
    // Nur einmal gerechnet
    EXPECT_EQ(berechnet, 1);
    // Anderer Schlüssel
    cache.hole("b", berechne);
    EXPECT_EQ(berechnet, 2);
    EXPECT_EQ(cache.anzahlBerechnungen(), 2u);
}

// Viele gleichzeitige Anfragen mit demselben Schlüssel: genau eine Berechnung, alle bekommen das Ergebnis
TEST(ErgebnisCache, GleichzeitigeAnfragenNurEinmalBerechnet) {
    // Uhr und Cache
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 100);
    // Freigabe für die Berechnung: sie wartet, bis der Test sie loslässt
    std::promise<void> freigabe;
    std::shared_future<void> losgelassen = freigabe.get_future().share();
    // Zähler der Berechnungen (atomar, da mehrere Threads)
    std::atomic<int> berechnet{0};
    // Langsame Berechnung
    auto berechne = [&] {
        // Zählen
        ++berechnet;
        // Warten auf die Freigabe
        losgelassen.wait();
        // Ergebnis
        Antwort a;
        a.body = "langsam";
        return a;
    };
    // Erster Thread beginnt zu rechnen
    auto erster = std::async(std::launch::async, [&] { return cache.hole("x", berechne).body; });
    // Warten, bis seine Berechnung wirklich läuft (Eintrag angelegt)
    while (cache.anzahlBerechnungen() == 0) {
        // Kurz warten
        std::this_thread::sleep_for(1ms);
    }
    // 7 weitere Threads fragen denselben Schlüssel, während der erste noch rechnet
    std::vector<std::future<std::string>> weitere;
    for (int i = 0; i < 7; ++i) {
        // Gleicher Schlüssel
        weitere.push_back(std::async(std::launch::async, [&] { return cache.hole("x", berechne).body; }));
    }
    // Den Threads Zeit geben, an hole() anzukommen (für die Korrektheit nicht nötig: wer später kommt,
    // findet den fertigen Eintrag und rechnet ebenfalls nicht)
    std::this_thread::sleep_for(50ms);
    // Berechnung freigeben
    freigabe.set_value();
    // Alle bekommen dasselbe Ergebnis
    EXPECT_EQ(erster.get(), "langsam");
    for (auto& w : weitere) {
        // Gleiches Ergebnis
        EXPECT_EQ(w.get(), "langsam");
    }
    // Nur eine Berechnung
    EXPECT_EQ(berechnet.load(), 1);
}

// Nach Ablauf der Gültigkeit wird neu berechnet
TEST(ErgebnisCache, AbgelaufenWirdNeuBerechnet) {
    // Uhr und Cache mit 5 Minuten
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 100);
    // Zähler
    int berechnet = 0;
    auto berechne = [&berechnet] {
        // Zählen
        ++berechnet;
        // Leere Antwort
        return Antwort{};
    };
    // Erste Berechnung
    cache.hole("a", berechne);
    // 4 Minuten 59 Sekunden später: noch gültig
    uhr.vorstellen(5min - 1s);
    cache.hole("a", berechne);
    EXPECT_EQ(berechnet, 1);
    // Genau 5 Minuten: abgelaufen
    uhr.vorstellen(1s);
    cache.hole("a", berechne);
    EXPECT_EQ(berechnet, 2);
}

// 5xx-Antworten werden nicht aufbewahrt, 4xx schon
TEST(ErgebnisCache, ServerfehlerWerdenNichtGespeichert) {
    // Uhr und Cache
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 100);
    // Zähler
    int berechnet = 0;
    // Antwort mit festem Status
    auto mitStatus = [&berechnet](int status) {
        // Lambda, das eine Antwort mit diesem Status liefert
        return [&berechnet, status] {
            // Zählen
            ++berechnet;
            // Antwort
            Antwort a;
            a.status = status;
            return a;
        };
    };
    // 502 zweimal: zweimal berechnet
    cache.hole("fehler", mitStatus(502));
    cache.hole("fehler", mitStatus(502));
    EXPECT_EQ(berechnet, 2);
    // 400 zweimal: einmal berechnet
    cache.hole("ungueltig", mitStatus(400));
    cache.hole("ungueltig", mitStatus(400));
    EXPECT_EQ(berechnet, 3);
}

// Eine Exception in der Berechnung wird weitergegeben und nicht gespeichert
TEST(ErgebnisCache, ExceptionWirdNichtGespeichert) {
    // Uhr und Cache
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 100);
    // Berechnung, die wirft
    EXPECT_THROW(cache.hole("a", []() -> Antwort { throw std::runtime_error("x"); }), std::runtime_error);
    // Kein Eintrag übrig
    EXPECT_EQ(cache.groesse(), 0u);
    // Beim nächsten Mal wird neu versucht
    EXPECT_EQ(cache.hole("a", [] { return Antwort{}; }).status, 200);
}

// Obergrenze: der älteste Eintrag wird verdrängt
TEST(ErgebnisCache, ObergrenzeWirdEingehalten) {
    // Uhr und Cache mit höchstens 2 Einträgen
    FakeUhr uhr(startzeit);
    ErgebnisCache cache(uhr, 5min, 2);
    // Zähler
    int berechnet = 0;
    auto berechne = [&berechnet] {
        // Zählen
        ++berechnet;
        // Leere Antwort
        return Antwort{};
    };
    // Drei Schlüssel nacheinander (Zeit läuft, damit "ältester" eindeutig ist)
    cache.hole("a", berechne);
    uhr.vorstellen(1s);
    cache.hole("b", berechne);
    uhr.vorstellen(1s);
    cache.hole("c", berechne);
    // Höchstens 2 Einträge
    EXPECT_EQ(cache.groesse(), 2u);
    // "b" und "c" sind noch da (keine neue Berechnung) ...
    cache.hole("b", berechne);
    cache.hole("c", berechne);
    EXPECT_EQ(berechnet, 3);
    // ... "a" wurde verdrängt und wird neu berechnet
    cache.hole("a", berechne);
    EXPECT_EQ(berechnet, 4);
}

// Ungültige Konfiguration
TEST(ErgebnisCache, UngueltigeKonfiguration) {
    // Uhr
    FakeUhr uhr(startzeit);
    // Negative Gültigkeit
    EXPECT_THROW(ErgebnisCache(uhr, -1s, 10), std::invalid_argument);
    // Kein Platz
    EXPECT_THROW(ErgebnisCache(uhr, 1s, 0), std::invalid_argument);
}

// ================================================================ Schlüssel und Synchronisierung

// Schlüssel: gleich bei gleicher Anfrage (auch andere Reihenfolge), verschieden sonst, keine Verwechslung
TEST(GecachteAnfragebehandlung, Schluessel) {
    // Kurzschreibweise
    auto s = [](const std::string& pfad, std::map<std::string, std::string> p) {
        // Schlüssel einer GET-Anfrage
        return GecachteAnfragebehandlung::schluessel(Anfrage{"GET", pfad, std::move(p)});
    };
    // Reihenfolge der Parameter egal (map ist sortiert)
    EXPECT_EQ(s("/api/indikatoren", {{"sma", "5"}, {"rsi", "7"}}), s("/api/indikatoren", {{"rsi", "7"}, {"sma", "5"}}));
    // Anderer Wert, anderer Pfad, andere Methode: anderer Schlüssel
    EXPECT_NE(s("/api/kurse", {{"symbol", "IBM"}}), s("/api/kurse", {{"symbol", "AAPL"}}));
    EXPECT_NE(s("/api/kurse", {{"symbol", "IBM"}}), s("/api/indikatoren", {{"symbol", "IBM"}}));
    EXPECT_NE(GecachteAnfragebehandlung::schluessel(Anfrage{"GET", "/api/kurse", {}}),
              GecachteAnfragebehandlung::schluessel(Anfrage{"POST", "/api/kurse", {}}));
    // Sonderzeichen in Werten können keine Verwechslung erzeugen: "a=b&c" als ein Wert ist etwas anderes
    // als zwei Parameter a=b und c (ohne Längenangabe könnten beide "a=b&c" ergeben)
    EXPECT_NE(s("/x", {{"a", "b&c="}}), s("/x", {{"a", "b"}, {"c", ""}}));
}

// SynchronisierteQuelle: niemals zwei Threads gleichzeitig in der inneren Quelle
TEST(SynchronisierteQuelle, NieZweiGleichzeitig) {
    // Innere Quelle, die gleichzeitige Zugriffe erkennt
    class Pruefquelle : public IPriceSource {
    public:
        // Lädt langsam und prüft, ob schon jemand drin ist
        Kursreihe ladeKurse(const Wertpapier&) override {
            // Anzahl gleichzeitiger Aufrufe erhöhen; war schon jemand drin, ist das ein Fehler
            if (++drin > 1) {
                // Gleichzeitigkeit erkannt
                gleichzeitig = true;
            }
            // Etwas Zeit vergehen lassen, damit Überschneidungen wahrscheinlich wären
            std::this_thread::sleep_for(200us);
            // Wieder verlassen
            --drin;
            // Leere Reihe
            return Kursreihe();
        }
        // Wie viele gerade drin sind
        std::atomic<int> drin{0};
        // Ob jemals zwei gleichzeitig drin waren
        std::atomic<bool> gleichzeitig{false};
    };
    // Innere Quelle und Decorator
    Pruefquelle innen;
    SynchronisierteQuelle sicher(innen);
    // 8 Threads laden je 25-mal
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        // Thread starten
        threads.emplace_back([&sicher] {
            // 25 Ladevorgänge
            for (int i = 0; i < 25; ++i) {
                // Über den Decorator laden
                sicher.ladeKursdaten(Wertpapier("IBM", "IBM", "USA", "USD"));
            }
        });
    }
    // Auf alle warten
    for (auto& t : threads) {
        // Warten
        t.join();
    }
    // Nie zwei gleichzeitig
    EXPECT_FALSE(innen.gleichzeitig.load());
}

// ================================================================ Stresstest

// Stresstest mit festem Seed: 2000 zufällige Anfragen, parallel in gemischter Reihenfolge und mit
// verschiedener Anzahl Threads. Prüft für jede Anfrage, dass das Ergebnis exakt dem der sequentiellen
// Berechnung entspricht, dass jede verschiedene Anfrage genau einmal berechnet wurde und dass alles in
// begrenzter Zeit fertig wird.
TEST(Stresstest, ParallelGleichSequentiellUndOhneDoppelteBerechnung) {
    // Feste Folge von Anfragen
    const std::vector<Anfrage> anfragen = zufallsAnfragen(2000, 20261113);

    // --- Referenz: sequentiell, ohne Cache, ohne Threads ---
    TestQuelle referenzQuelle;
    Anfragebehandlung referenz(katalog(), referenzQuelle, entscheidungen());
    // Ergebnis jeder verschiedenen Anfrage
    std::map<std::string, Antwort> erwartet;
    for (const Anfrage& a : anfragen) {
        // Schlüssel
        const std::string k = GecachteAnfragebehandlung::schluessel(a);
        // Nur einmal pro Schlüssel rechnen (die Referenz rechnet ohnehin deterministisch)
        if (erwartet.find(k) == erwartet.end()) {
            // Sequentiell berechnen
            erwartet[k] = referenz.behandle(a);
        }
    }
    // Es soll eine gute Mischung geben: Erfolge und alle Fehlerarten kommen vor
    std::set<int> statuswerte;
    for (const auto& [k, antwort] : erwartet) {
        // Status merken
        statuswerte.insert(antwort.status);
    }
    EXPECT_EQ(statuswerte, (std::set<int>{200, 400, 404, 422}));

    // Zeitmessung für die Begrenzung
    const auto beginn = std::chrono::steady_clock::now();

    // --- Parallel: mit 1, 4 und 16 Threads, jedes Mal in anderer, aber fester Reihenfolge ---
    for (const std::size_t threads : {std::size_t{1}, std::size_t{4}, std::size_t{16}}) {
        // Neuer Aufbau für jeden Durchlauf
        TestQuelle quelle;
        SynchronisierteQuelle sichereQuelle(quelle);
        Anfragebehandlung api(katalog(), sichereQuelle, entscheidungen());
        ZaehlenderBehandler zaehler(api);
        FakeUhr uhr(startzeit);
        ErgebnisCache cache(uhr, 5min, 10000);
        GecachteAnfragebehandlung gecacht(zaehler, cache);

        // Reihenfolge mischen (fester Seed je Durchlauf)
        std::vector<std::size_t> reihenfolge(anfragen.size());
        for (std::size_t i = 0; i < reihenfolge.size(); ++i) {
            // 0, 1, 2, ...
            reihenfolge[i] = i;
        }
        std::mt19937 mischer(static_cast<unsigned>(threads));
        std::shuffle(reihenfolge.begin(), reihenfolge.end(), mischer);

        // Alle Anfragen in den Pool
        std::vector<std::future<Antwort>> zukuenfte(anfragen.size());
        {
            // Pool nur für diesen Block; am Ende wartet der Destruktor auf alle Threads
            ThreadPool pool(threads);
            for (std::size_t i : reihenfolge) {
                // Anfrage i einreihen; das Ergebnis landet an Position i
                zukuenfte[i] = pool.einreihen([&gecacht, &anfragen, i] { return gecacht.behandle(anfragen[i]); });
            }
        }

        // Jede Antwort mit der Referenz vergleichen
        for (std::size_t i = 0; i < anfragen.size(); ++i) {
            // Ergebnis abholen
            const Antwort a = zukuenfte[i].get();
            // Referenz
            const Antwort& r = erwartet.at(GecachteAnfragebehandlung::schluessel(anfragen[i]));
            // Gleicher Status und exakt gleicher Inhalt
            ASSERT_EQ(a.status, r.status) << "Anfrage " << i << " mit " << threads << " Threads";
            ASSERT_EQ(a.body, r.body) << "Anfrage " << i << " mit " << threads << " Threads";
        }
        // Jede verschiedene Anfrage wurde genau einmal berechnet
        EXPECT_EQ(static_cast<std::size_t>(zaehler.aufrufe.load()), erwartet.size()) << threads << " Threads";
        EXPECT_EQ(cache.anzahlBerechnungen(), erwartet.size()) << threads << " Threads";
    }

    // Begrenzte Zeit: alle drei Durchläufe zusammen deutlich unter 30 Sekunden
    const auto dauer = std::chrono::steady_clock::now() - beginn;
    EXPECT_LT(dauer, 30s);
}
