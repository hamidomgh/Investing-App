// Eigene Header-Datei mit der Klasse HttpServer
#include "http_server.hpp"

// HTTP-Bibliothek cpp-httplib (Header-only, liegt in third_party/)
#include <httplib.h>

// std::move
#include <utility>

// Konstruktor: Server anlegen, Ordner und Routen einrichten
HttpServer::HttpServer(Anfragebehandlung& api, std::filesystem::path web)
    : api_(api), server_(std::make_unique<httplib::Server>()) {
    // cpp-httplib bearbeitet Anfragen normalerweise in mehreren Threads gleichzeitig.
    // Anfragebehandlung und Cache sind aber noch nicht thread-sicher (das kommt mit E9).
    // Deshalb vorerst genau ein Arbeits-Thread: Anfragen werden nacheinander bearbeitet.
    server_->new_task_queue = [] { return new httplib::ThreadPool(1); };

    // Statische Dateien der Weboberfläche aus dem Ordner ausliefern, falls er existiert.
    // Für "/" liefert cpp-httplib automatisch index.html.
    if (std::filesystem::is_directory(web)) {
        // Ordner unter "/" einhängen
        server_->set_mount_point("/", web.string());
    }

    // Lambda, das jede Anfrage an die Anfragebehandlung weitergibt
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
        // Logik ausführen (wirft nie)
        const Antwort ergebnis = api_.behandle(a);
        // Status und Inhalt in die HTTP-Antwort übernehmen
        antwort.status = ergebnis.status;
        antwort.set_content(ergebnis.body, ergebnis.inhaltstyp);
    };

    // Alle Pfade an die Anfragebehandlung geben. Dateien der Weboberfläche werden vorher bedient,
    // alles andere (auch unbekannte Pfade) beantwortet die Anfragebehandlung mit JSON (z. B. 404).
    // Auch andere Methoden als GET, damit sie eine verständliche 405-Antwort bekommen.
    server_->Get(".*", weiterleiten);
    server_->Post(".*", weiterleiten);
    server_->Put(".*", weiterleiten);
    server_->Delete(".*", weiterleiten);
    server_->Patch(".*", weiterleiten);
}

// Destruktor: hier ist httplib::Server vollständig bekannt, unique_ptr kann ihn löschen
HttpServer::~HttpServer() = default;

// Startet den Server (blockiert)
bool HttpServer::starte(const std::string& host, int port) {
    // listen öffnet den Port und bearbeitet Anfragen, bis stop() aufgerufen wird
    return server_->listen(host, port);
}

// Beendet den Server
void HttpServer::stoppe() {
    // listen() in starte() kehrt danach zurück
    server_->stop();
}
