// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Die Logik der Schnittstelle (ohne Netzwerk)
#include "anfragebehandlung.hpp"

// std::filesystem::path für den Ordner der Weboberfläche
#include <filesystem>
// std::unique_ptr
#include <memory>
// std::string
#include <string>

// Vorwärtsdeklaration: Die Klasse httplib::Server wird hier nur als Zeiger gebraucht.
// So muss dieser Header die große Datei httplib.h nicht einbinden; nur http_server.cpp kennt sie.
namespace httplib {
class Server;
}

// Dünne Netzwerkschicht des Servers (E8, Teil 2) auf Basis von cpp-httplib.
// Sie enthält keine Logik der Schnittstelle, sondern übersetzt nur:
//   HTTP-Anfrage  -> Anfrage  -> Anfragebehandlung::behandle -> Antwort -> HTTP-Antwort
// Zusätzlich liefert sie die statischen Dateien der Weboberfläche (E10) aus einem Ordner aus.
class HttpServer {
public:
    // api:  die Anfragebehandlung (Referenz, gehört dem Aufrufer)
    // web:  Ordner mit index.html usw.; existiert er nicht, gibt es nur die API
    HttpServer(Anfragebehandlung& api, std::filesystem::path web);

    // Destruktor muss in der .cpp definiert sein, weil httplib::Server hier unvollständig ist
    ~HttpServer();

    // Startet den Server und blockiert, bis stoppe() aufgerufen wird.
    // Liefert false, wenn der Port nicht geöffnet werden kann (z. B. schon belegt).
    bool starte(const std::string& host, int port);

    // Beendet den Server (aus einem anderen Thread aufzurufen)
    void stoppe();

private:
    // Die Logik der Schnittstelle
    Anfragebehandlung& api_;
    // Der eigentliche Server der Bibliothek
    std::unique_ptr<httplib::Server> server_;
};
