// Eigene Header-Datei mit der Klasse ThreadPool
#include "threadpool.hpp"

// std::find_if, um den aufrufenden Thread unter den Pool-Threads zu suchen
#include <algorithm>

// Konstruktor: startet die Arbeits-Threads
ThreadPool::ThreadPool(std::size_t anzahl) {
    // Ein Pool ohne Threads würde nie etwas ausführen
    if (anzahl == 0) {
        // Fehler
        throw std::invalid_argument("ThreadPool: number of threads must be > 0");
    }
    // Platz für alle Threads reservieren
    threads_.reserve(anzahl);
    // Threads starten; jeder führt arbeite() aus
    for (std::size_t i = 0; i < anzahl; ++i) {
        // emplace_back erzeugt den Thread direkt im Vector
        threads_.emplace_back([this] { arbeite(); });
    }
}

// Destruktor: sauber beenden, damit kein Thread übrig bleibt
ThreadPool::~ThreadPool() {
    // Aufgaben abarbeiten und alle Threads einsammeln
    beende();
}

// Reiht eine Aufgabe ein
bool ThreadPool::einreihenOhneErgebnis(std::function<void()> aufgabe) {
    // Eigener Block, damit der Mutex vor dem Wecken wieder frei ist
    {
        // Warteschlange sperren
        std::lock_guard<std::mutex> sperre(mutex_);
        // Nach beende() werden keine Aufgaben mehr angenommen
        if (beendet_) {
            // Ablehnen
            return false;
        }
        // Aufgabe anhängen
        aufgaben_.push(std::move(aufgabe));
    }
    // Einen schlafenden Thread wecken
    signal_.notify_one();
    // Angenommen
    return true;
}

// Schleife eines Arbeits-Threads
void ThreadPool::arbeite() {
    // Läuft, bis der Pool beendet und die Warteschlange leer ist
    while (true) {
        // Die nächste Aufgabe
        std::function<void()> aufgabe;
        // Eigener Block: Mutex nur zum Herausnehmen halten, nicht beim Ausführen
        {
            // unique_lock statt lock_guard, weil wait() den Mutex zwischendurch freigeben muss
            std::unique_lock<std::mutex> sperre(mutex_);
            // Schlafen, bis es Arbeit gibt oder beendet wird. Die Bedingung wird nach jedem Aufwachen geprüft
            // (schützt auch gegen "spurious wakeups", also Aufwachen ohne Signal).
            signal_.wait(sperre, [this] { return beendet_ || !aufgaben_.empty(); });
            // Beendet und nichts mehr zu tun: Thread verlassen
            if (beendet_ && aufgaben_.empty()) {
                // Ende der Schleife
                return;
            }
            // Vorderste Aufgabe herausnehmen
            aufgabe = std::move(aufgaben_.front());
            aufgaben_.pop();
        }
        // Aufgabe OHNE gesperrten Mutex ausführen, damit andere Threads gleichzeitig arbeiten können
        try {
            // Ausführen
            aufgabe();
        } catch (...) {
            // Eine Exception darf den Thread nicht beenden (das würde das Programm abbrechen).
            // Aufgaben aus einreihen() fangen ihre Exceptions schon selbst im packaged_task ab.
        }
    }
}

// Beendet den Pool sauber
void ThreadPool::beende() {
    // Nur ein Aufrufer gleichzeitig darf die Threads einsammeln
    std::lock_guard<std::mutex> beendenSperre(beendenMutex_);
    // Ist der aufrufende Thread einer der Pool-Threads? Dann würde join() auf sich selbst warten (Deadlock).
    const auto ich = std::this_thread::get_id();
    if (std::find_if(threads_.begin(), threads_.end(), [ich](const std::thread& t) { return t.get_id() == ich; }) !=
        threads_.end()) {
        // Verboten
        throw std::logic_error("ThreadPool: beende() must not be called from a pool thread");
    }
    // Eigener Block: Zustand ändern
    {
        // Warteschlange sperren
        std::lock_guard<std::mutex> sperre(mutex_);
        // Keine neuen Aufgaben mehr
        beendet_ = true;
    }
    // Alle schlafenden Threads wecken, damit sie das Ende bemerken
    signal_.notify_all();
    // Auf jeden Thread warten
    for (std::thread& t : threads_) {
        // Nur Threads, die noch nicht eingesammelt wurden (zweiter Aufruf von beende())
        if (t.joinable()) {
            // Warten, bis der Thread seine Schleife verlassen hat
            t.join();
        }
    }
}

// Anzahl der Threads
std::size_t ThreadPool::anzahlThreads() const {
    // Größe des Vectors
    return threads_.size();
}
