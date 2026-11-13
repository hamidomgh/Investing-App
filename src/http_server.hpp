// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// Schnittstelle für die Behandlung der Anfragen (mit oder ohne Ergebnis-Cache)
#include "anfrage_behandler.hpp"

// std::size_t
#include <cstddef>
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

// Dünne Netzwerkschicht des Servers (E8) auf Basis von cpp-httplib, ab E9 mit eigenem Thread-Pool.
// Sie enthält keine Logik der Schnittstelle, sondern übersetzt nur:
//   HTTP-Anfrage  -> Anfrage  -> IAnfrageBehandler::behandle -> Antwort -> HTTP-Antwort
// Zusätzlich liefert sie die statischen Dateien der Weboberfläche (E10) aus einem Ordner aus.
//
// Ablauf:  binde(host, port)  ->  lausche() (blockiert)  ->  stoppe() aus einem anderen Thread
// Nach stoppe() kehrt lausche() zurück, sobald alle laufenden Anfragen fertig und alle Threads des Pools
// beendet sind (sauberes Beenden, DoD E9).
class HttpServer {
public:
    // behandler: bearbeitet die Anfragen; muss thread-sicher sein (Referenz, gehört dem Aufrufer)
    // web:       Ordner mit index.html usw.; existiert er nicht, gibt es nur die API
    // threads:   Anzahl der Arbeits-Threads des Pools (mindestens 1)
    HttpServer(IAnfrageBehandler& behandler, std::filesystem::path web, std::size_t threads);

    // Destruktor muss in der .cpp definiert sein, weil httplib::Server hier unvollständig ist
    ~HttpServer();

    // Öffnet den Port. port = 0 wählt einen freien Port (für Tests).
    // Liefert den tatsächlich geöffneten Port oder -1, wenn das nicht geht (z. B. Port belegt).
    int binde(const std::string& host, int port);

    // Bearbeitet Anfragen, bis stoppe() aufgerufen wird (blockiert). Vorher binde() aufrufen.
    // Liefert false, wenn der Server nicht lauschen konnte oder schon vorher gestoppt wurde.
    bool lausche();

    // Wartet, bis lausche() Anfragen annimmt oder feststeht, dass es das nicht tun wird
    void warteBisBereit() const;

    // Beendet den Server; darf aus jedem Thread und auch vor lausche() aufgerufen werden
    void stoppe();

private:
    // Der Behandler der Anfragen
    IAnfrageBehandler& behandler_;
    // Der eigentliche Server der Bibliothek
    std::unique_ptr<httplib::Server> server_;
};
