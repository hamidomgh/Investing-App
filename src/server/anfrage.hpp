// Verhindert, dass der Compiler diese Datei mehrfach einliest
#pragma once

// std::map für die Parameter
#include <map>
// std::string
#include <string>

// Eine Anfrage an den eigenen Server (E8), unabhängig von der Netzwerk-Bibliothek.
// Die Netzwerkschicht übersetzt eine echte HTTP-Anfrage in dieses einfache Objekt;
// Tests erzeugen es direkt, ganz ohne Socket.
struct Anfrage {
    // HTTP-Methode, z. B. "GET"
    std::string methode = "GET";
    // Pfad ohne Parameter, z. B. "/api/kurse"
    std::string pfad;
    // Parameter aus der URL, z. B. {"symbol": "IBM", "sma": "20"} für "?symbol=IBM&sma=20"
    std::map<std::string, std::string> parameter;
};

// Die Antwort des eigenen Servers, ebenfalls unabhängig von der Netzwerk-Bibliothek
struct Antwort {
    // HTTP-Status, z. B. 200, 400, 404
    int status = 200;
    // Inhaltstyp, bei der API immer JSON
    std::string inhaltstyp = "application/json; charset=utf-8";
    // Inhalt (JSON-Text)
    std::string body;
};
