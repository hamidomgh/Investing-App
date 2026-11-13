// Eigene Header-Datei mit der Klasse HttpServer
#include "http_server.hpp"

// Unser Thread-Pool (E9)
#include "threadpool.hpp"

// HTTP-Bibliothek cpp-httplib (Header-only, liegt in third_party/)
#include <httplib.h>

// std::function
#include <functional>
// std::move
#include <utility>

// Anonymer Namespace: Adapter nur in dieser Datei sichtbar
namespace {

// Adapter (E9): cpp-httplib verteilt jede Verbindung als Aufgabe an eine "TaskQueue".
// Diese Klasse erfüllt die TaskQueue-Schnittstelle der Bibliothek mit unserem eigenen ThreadPool.
//   enqueue  -> ThreadPool::einreihenOhneErgebnis
//   shutdown -> ThreadPool::beende (arbeitet ab und wartet mit join auf alle Threads)
// cpp-httplib ruft shutdown() auf, wenn lausche() nach stoppe() zu Ende geht.
class PoolWarteschlange : public httplib::TaskQueue {
public:
    // Pool mit der gewünschten Anzahl Threads anlegen
    explicit PoolWarteschlange(std::size_t threads) : pool_(threads) {}

    // Aufgabe (eine Verbindung) an den Pool geben; false, wenn der Pool schon beendet ist
    bool enqueue(std::function<void()> aufgabe) override { return pool_.einreihenOhneErgebnis(std::move(aufgabe)); }

    // Pool sauber beenden
    void shutdown() override { pool_.beende(); }

private:
    // Unser Thread-Pool
    ThreadPool pool_;
};

}  // namespace

// Konstruktor: Server anlegen, Pool, Ordner und Routen einrichten
HttpServer::HttpServer(IAnfrageBehandler& behandler, std::filesystem::path web, std::size_t threads)
    : behandler_(behandler), server_(std::make_unique<httplib::Server>()) {
    // Mindestens ein Thread (0 würde der Pool ablehnen)
    const std::size_t anzahl = threads == 0 ? 1 : threads;
    // cpp-httplib holt sich beim Start von lausche() über diese Funktion seine Warteschlange: unseren Pool
    server_->new_task_queue = [anzahl] { return new PoolWarteschlange(anzahl); };
    // Offene Verbindungen ohne neue Anfrage nach 2 s schließen; sonst würde ein Browser mit "keep-alive"
    // einen Thread bis zu 5 s (Standard) belegen und das Beenden entsprechend verzögern
    server_->set_keep_alive_timeout(2);
    // Socket-Optionen selbst festlegen: cpp-httplib setzt standardmäßig auch SO_REUSEPORT. Damit könnten zwei
    // Server-Prozesse denselben Port gleichzeitig öffnen und sich die Anfragen teilen, statt dass der zweite
    // "Port belegt" meldet. Wir setzen nur SO_REUSEADDR: Nach einem Neustart ist der Port sofort wieder nutzbar,
    // aber ein zweiter laufender Server wird abgewiesen.
    server_->set_socket_options([](socket_t sock) {
        // 1 = Option einschalten
        int ein = 1;
        // Nur SO_REUSEADDR
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const void*>(&ein), sizeof(ein));
    });

    // Statische Dateien der Weboberfläche aus dem Ordner ausliefern, falls er existiert.
    // Für "/" liefert cpp-httplib automatisch index.html.
    if (std::filesystem::is_directory(web)) {
        // Ordner unter "/" einhängen
        server_->set_mount_point("/", web.string());
    }

    // Lambda, das jede Anfrage an den Behandler weitergibt (läuft in einem Thread des Pools)
    auto weiterleiten = [this](const httplib::Request& anfrage, httplib::Response& antwort) {
        // Eigene Anfrage aus der HTTP-Anfrage bauen: Methode und Pfad übernehmen
        Anfrage a;
        a.methode = anfrage.method;
        a.pfad = anfrage.path;
        // Parameter übernehmen; bei doppelten Namen ("?sma=5&sma=6") gilt der erste
        for (const auto& [name, wert] : anfrage.params) {
            // emplace fügt nur ein, wenn der Name noch fehlt
            a.parameter.emplace(name, wert);
        }
        // Logik ausführen (wirft nie; darf von mehreren Threads gleichzeitig aufgerufen werden)
        const Antwort ergebnis = behandler_.behandle(a);
        // Status und Inhalt in die HTTP-Antwort übernehmen
        antwort.status = ergebnis.status;
        antwort.set_content(ergebnis.body, ergebnis.inhaltstyp);
    };

    // Alle Pfade an den Behandler geben. Dateien der Weboberfläche werden vorher bedient,
    // alles andere (auch unbekannte Pfade) beantwortet der Behandler mit JSON (z. B. 404).
    // Auch andere Methoden als GET, damit sie eine verständliche 405-Antwort bekommen.
    server_->Get(".*", weiterleiten);
    server_->Post(".*", weiterleiten);
    server_->Put(".*", weiterleiten);
    server_->Delete(".*", weiterleiten);
    server_->Patch(".*", weiterleiten);
}

// Destruktor: hier ist httplib::Server vollständig bekannt, unique_ptr kann ihn löschen
HttpServer::~HttpServer() = default;

// Port öffnen
int HttpServer::binde(const std::string& host, int port) {
    // Port 0: das Betriebssystem wählt einen freien Port
    if (port == 0) {
        // Liefert den gewählten Port oder -1
        return server_->bind_to_any_port(host);
    }
    // Bestimmter Port: true bei Erfolg
    return server_->bind_to_port(host, port) ? port : -1;
}

// Anfragen bearbeiten, bis stoppe() aufgerufen wird
bool HttpServer::lausche() {
    // Bearbeitet Verbindungen in unserem Pool; kehrt nach stoppe() zurück, nachdem der Pool beendet ist
    return server_->listen_after_bind();
}

// Warten, bis der Server läuft (oder nicht mehr starten wird)
void HttpServer::warteBisBereit() const {
    // Wartet in kleinen Schritten auf den Zustand der Bibliothek
    server_->wait_until_ready();
}

// Server beenden
void HttpServer::stoppe() {
    // Läuft der Server: Port schließen, die Schleife in lausche() endet
    server_->stop();
    // Läuft er noch nicht: verhindern, dass ein späteres lausche() überhaupt startet
    server_->decommission();
}
