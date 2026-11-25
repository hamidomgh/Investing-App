// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::condition_variable: Threads schlafen lassen, bis es Arbeit gibt
#include <condition_variable>
// std::size_t
#include <cstddef>
// std::function für die Aufgaben in der Warteschlange
#include <functional>
// std::future und std::packaged_task für Ergebnisse von Aufgaben
#include <future>
// std::shared_ptr
#include <memory>
// std::mutex und std::lock_guard
#include <mutex>
// std::queue: Warteschlange der Aufgaben
#include <queue>
// std::runtime_error
#include <stdexcept>
// std::thread
#include <thread>
// std::invoke_result_t: Rückgabetyp einer Aufgabe bestimmen
#include <type_traits>
// std::move
#include <utility>
// std::vector für die Threads
#include <vector>

// Thread-Pool (E9): eine feste Anzahl Arbeits-Threads holt Aufgaben aus einer gemeinsamen Warteschlange.
// Statt für jede Anfrage einen neuen Thread zu starten (teuer, unbegrenzt viele), arbeiten immer
// dieselben N Threads die Aufgaben nacheinander ab.
//
// Thread-Sicherheit: Die Warteschlange wird von allen Threads geteilt und ist deshalb durch einen Mutex
// geschützt. Wartende Threads schlafen an einer condition_variable, bis eine Aufgabe kommt oder der
// Pool beendet wird (kein aktives Warten, das CPU verbraucht).
//
// Beenden: beende() nimmt keine neuen Aufgaben mehr an, lässt die Threads die Warteschlange noch
// abarbeiten und wartet dann mit join() auf jeden Thread. Danach läuft kein Thread des Pools mehr.
// Der Destruktor ruft beende() auf, damit nie ein Thread "hängen bleibt".
class ThreadPool {
public:
    // Startet "anzahl" Arbeits-Threads. Wirft std::invalid_argument bei anzahl == 0.
    explicit ThreadPool(std::size_t anzahl);

    // Beendet den Pool sauber (siehe beende())
    ~ThreadPool();

    // Ein Pool besitzt Threads; Kopieren ergäbe keinen Sinn und ist deshalb verboten
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Reiht eine Aufgabe ein und liefert ein future für ihr Ergebnis.
    // Wirft die Aufgabe eine Exception, kommt sie beim Aufruf von future.get() an.
    // Wirft std::runtime_error, wenn der Pool schon beendet ist.
    // Als Template im Header, weil der Rückgabetyp von der Aufgabe abhängt (z. B. int oder Antwort).
    template <class Aufgabe>
    auto einreihen(Aufgabe aufgabe) -> std::future<std::invoke_result_t<Aufgabe>>;

    // Reiht eine Aufgabe ohne Ergebnis ein (für die Anbindung an cpp-httplib in E9 Teil 2).
    // Liefert false statt einer Exception, wenn der Pool schon beendet ist.
    bool einreihenOhneErgebnis(std::function<void()> aufgabe);

    // Nimmt keine neuen Aufgaben mehr an, arbeitet die Warteschlange ab und wartet auf alle Threads.
    // Mehrfacher Aufruf ist erlaubt (ab dem zweiten passiert nichts).
    // Wirft std::logic_error, wenn es aus einem Thread des Pools selbst aufgerufen wird
    // (ein Thread kann nicht auf sich selbst warten).
    void beende();

    // Anzahl der Arbeits-Threads
    std::size_t anzahlThreads() const;

private:
    // Schleife jedes Arbeits-Threads: Aufgabe holen, ausführen, wiederholen
    void arbeite();

    // Die Arbeits-Threads
    std::vector<std::thread> threads_;
    // Warteschlange der noch nicht begonnenen Aufgaben
    std::queue<std::function<void()>> aufgaben_;
    // Schützt aufgaben_ und beendet_ (geteilter Zustand zwischen den Threads)
    std::mutex mutex_;
    // Weckt schlafende Threads, wenn eine Aufgabe kommt oder beendet wird
    std::condition_variable signal_;
    // true, sobald beende() aufgerufen wurde
    bool beendet_ = false;
    // Schützt das join() in beende(), damit zwei gleichzeitige Aufrufe nicht beide joinen
    std::mutex beendenMutex_;
};

// Implementierung des Templates (muss im Header stehen, damit der Compiler sie für jeden Typ erzeugen kann)
template <class Aufgabe>
auto ThreadPool::einreihen(Aufgabe aufgabe) -> std::future<std::invoke_result_t<Aufgabe>> {
    // Rückgabetyp der Aufgabe, z. B. int
    using Ergebnis = std::invoke_result_t<Aufgabe>;
    // packaged_task verbindet die Aufgabe mit einem future: Ergebnis oder Exception landen dort.
    // shared_ptr, weil std::function kopierbar sein muss, packaged_task aber nur verschiebbar ist.
    auto paket = std::make_shared<std::packaged_task<Ergebnis()>>(std::move(aufgabe));
    // Das future, über das der Aufrufer später das Ergebnis abholt
    std::future<Ergebnis> zukunft = paket->get_future();
    // In die Warteschlange stellen; das Lambda ruft beim Ausführen die eigentliche Aufgabe auf
    if (!einreihenOhneErgebnis([paket] { (*paket)(); })) {
        // Pool schon beendet
        throw std::runtime_error("ThreadPool: cannot add tasks after shutdown");
    }
    // future zurückgeben
    return zukunft;
}
