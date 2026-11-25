// Eigene Header-Datei mit der Klasse Katalog
#include "katalog.hpp"

// ParseFehler und KonfigurationsFehler
#include "quellenfehler.hpp"

// JSON-Bibliothek nlohmann/json
#include <nlohmann/json.hpp>

// std::ifstream zum Lesen der Datei
#include <fstream>
// std::map für die Kürzel je Anbieter (F3)
#include <map>
// std::set, um doppelte Symbole zu erkennen
#include <set>
// std::stringstream, um die ganze Datei in einen String zu lesen
#include <sstream>
// std::invalid_argument aus dem Wertpapier-Konstruktor (E1)
#include <stdexcept>
// std::move
#include <utility>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Kurzer Name für den JSON-Typ
using json = nlohmann::json;

// Interne Ausnahme für einen einzelnen fehlerhaften Eintrag; wird in ausJson gefangen und
// in einen KatalogFehler umgewandelt, verlässt diese Datei also nie
struct EintragFehler {
    // Beschreibung des Fehlers
    std::string meldung;
};

// Liest ein Textfeld aus einem Eintrag; wirft EintragFehler, wenn es fehlt oder kein Text ist
std::string leseText(const json& eintrag, const char* feld) {
    // Feld suchen
    const auto it = eintrag.find(feld);
    // Fehlt das Feld?
    if (it == eintrag.end()) {
        // Fehler nennt den Feldnamen
        throw EintragFehler{std::string("field '") + feld + "' is missing"};
    }
    // Ist es kein Text (z. B. eine Zahl)?
    if (!it->is_string()) {
        // Fehler nennt den Feldnamen
        throw EintragFehler{std::string("field '") + feld + "' must be a string"};
    }
    // Text zurückgeben
    return it->get<std::string>();
}

// Liest eine Liste von Texten (z. B. "laender"); wirft EintragFehler, wenn sie fehlt oder etwas anderes enthält
std::vector<std::string> leseTextliste(const json& eintrag, const char* feld) {
    // Feld suchen
    const auto it = eintrag.find(feld);
    // Muss eine Liste sein
    if (it == eintrag.end() || !it->is_array()) {
        // Fehler nennt den Feldnamen
        throw EintragFehler{std::string("field '") + feld + "' must be a list of strings"};
    }
    // Ergebnis
    std::vector<std::string> liste;
    // Jedes Element
    for (const json& element : *it) {
        // Muss Text sein
        if (!element.is_string()) {
            // Fehler
            throw EintragFehler{std::string("field '") + feld + "' must be a list of strings"};
        }
        // Anhängen
        liste.push_back(element.get<std::string>());
    }
    // Fertige Liste
    return liste;
}

}  // namespace

// Liest den Katalog aus einem JSON-Text
Katalog Katalog::ausJson(const std::string& text) {
    // Das geparste Dokument
    json dokument;
    // Parsen versuchen
    try {
        // Text in JSON umwandeln
        dokument = json::parse(text);
    } catch (const json::parse_error& e) {
        // Ungültiges JSON: der ganze Katalog ist unbrauchbar
        throw ParseFehler(std::string("Catalog is not valid JSON: ") + e.what());
    }
    // Es muss ein Objekt mit einer Liste "wertpapiere" sein
    if (!dokument.is_object() || !dokument.contains("wertpapiere") || !dokument["wertpapiere"].is_array()) {
        // Ohne diese Liste gibt es nichts auszuwerten
        throw ParseFehler("Catalog must be an object with an array 'wertpapiere'");
    }

    // Neuer, leerer Katalog
    Katalog katalog;
    // Bereits gesehene Symbole, um doppelte Einträge zu erkennen
    std::set<std::string> gesehen;
    // Die Liste der Einträge
    const json& liste = dokument["wertpapiere"];
    // Jeden Eintrag einzeln prüfen; ein Fehler betrifft nur diesen Eintrag
    for (std::size_t i = 0; i < liste.size(); ++i) {
        // Aktueller Eintrag
        const json& eintrag = liste[i];
        // Symbol für die Fehlermeldung, falls es sich lesen lässt
        std::string symbol;
        // Fehler dieses Eintrags abfangen
        try {
            // Jeder Eintrag muss ein Objekt sein
            if (!eintrag.is_object()) {
                // Fehler
                throw EintragFehler{"entry must be an object"};
            }
            // Die vier Pflichtfelder lesen
            symbol = leseText(eintrag, "symbol");
            const std::string name = leseText(eintrag, "name");
            const std::string land = leseText(eintrag, "land");
            const std::string waehrung = leseText(eintrag, "waehrung");
            // Optional (F3): Kürzel je Anbieter, z. B. "symbole": {"alphavantage": "IBM", "twelvedata": "IBM"}
            std::map<std::string, std::string> symbole;
            if (eintrag.contains("symbole")) {
                // Muss ein Objekt sein
                if (!eintrag["symbole"].is_object()) {
                    // Fehler
                    throw EintragFehler{"field 'symbole' must be an object of strings"};
                }
                // Jeder Eintrag: Anbieter -> Kürzel als Text
                for (const auto& [anbieter, kuerzel] : eintrag["symbole"].items()) {
                    // Kürzel muss Text sein
                    if (!kuerzel.is_string()) {
                        // Fehler nennt den Anbieter
                        throw EintragFehler{"field 'symbole." + anbieter + "' must be a string"};
                    }
                    // Übernehmen
                    symbole[anbieter] = kuerzel.get<std::string>();
                }
            }
            // Wertpapier erzeugen; der Konstruktor aus E1 lehnt leere Felder (auch leere Kürzel) ab
            Wertpapier wp(symbol, name, land, waehrung, symbole);
            // insert liefert second == false, wenn das Symbol schon vorhanden war
            if (!gesehen.insert(symbol).second) {
                // Der erste Eintrag gilt, der doppelte wird gemeldet
                throw EintragFehler{"duplicate symbol"};
            }
            // Gültigen Eintrag übernehmen
            katalog.wertpapiere_.push_back(wp);
        } catch (const EintragFehler& f) {
            // Fehler dieses Eintrags merken und mit dem nächsten weitermachen
            katalog.fehler_.push_back({i, symbol, f.meldung});
        } catch (const std::invalid_argument& e) {
            // Leeres Feld (Meldung aus dem Wertpapier-Konstruktor) merken und weitermachen
            katalog.fehler_.push_back({i, symbol, e.what()});
        }
    }
    // Zentralbanken (E6): optional, damit Kataloge ohne diesen Abschnitt weiter funktionieren
    if (dokument.contains("zentralbanken")) {
        // Muss eine Liste sein, sonst ist der ganze Abschnitt unbrauchbar
        if (!dokument["zentralbanken"].is_array()) {
            // Ganzer Katalog ungültig, wie bei "wertpapiere"
            throw ParseFehler("Catalog field 'zentralbanken' must be an array");
        }
        // Bereits gesehene Namen
        std::set<std::string> namen;
        // Die Liste
        const json& banken = dokument["zentralbanken"];
        // Jeden Eintrag einzeln prüfen; ein Fehler betrifft nur diesen Eintrag
        for (std::size_t i = 0; i < banken.size(); ++i) {
            // Aktueller Eintrag
            const json& eintrag = banken[i];
            // Name für die Fehlermeldung
            std::string name;
            // Fehler dieses Eintrags abfangen
            try {
                // Muss ein Objekt sein
                if (!eintrag.is_object()) {
                    // Fehler
                    throw EintragFehler{"entry must be an object"};
                }
                // Felder lesen
                name = leseText(eintrag, "name");
                std::vector<std::string> laender = leseTextliste(eintrag, "laender");
                const std::string waehrung = leseText(eintrag, "waehrung");
                const std::string serie = leseText(eintrag, "fredSerie");
                // Zentralbank erzeugen; der Konstruktor prüft leere Felder und Länder
                Zentralbank bank(name, std::move(laender), waehrung, serie);
                // Doppelter Name?
                if (!namen.insert(name).second) {
                    // Der erste Eintrag gilt
                    throw EintragFehler{"duplicate central bank"};
                }
                // Übernehmen
                katalog.zentralbanken_.push_back(bank);
            } catch (const EintragFehler& f) {
                // Fehler merken, weitermachen
                katalog.zentralbankFehler_.push_back({i, name, f.meldung});
            } catch (const std::invalid_argument& e) {
                // Fehler aus dem Konstruktor merken, weitermachen
                katalog.zentralbankFehler_.push_back({i, name, e.what()});
            }
        }
    }
    // Fertigen Katalog zurückgeben
    return katalog;
}

// Liest den Katalog aus einer Datei
Katalog Katalog::ausDatei(const std::string& pfad) {
    // Datei öffnen
    std::ifstream datei(pfad);
    // Nicht vorhanden oder nicht lesbar: Konfigurationsfehler mit Pfad
    if (!datei) {
        // Fehler
        throw KonfigurationsFehler("Catalog file cannot be read: " + pfad);
    }
    // Ganzen Inhalt lesen
    std::stringstream puffer;
    puffer << datei.rdbuf();
    // Als JSON auswerten
    return ausJson(puffer.str());
}

// Liefert die gültigen Wertpapiere
const std::vector<Wertpapier>& Katalog::wertpapiere() const {
    // Referenz auf die interne Liste (keine Kopie)
    return wertpapiere_;
}

// Liefert die fehlerhaften Einträge
const std::vector<KatalogFehler>& Katalog::fehler() const {
    // Referenz auf die interne Liste
    return fehler_;
}

// Nur die Wertpapiere, die ein Anbieter liefert (F3), in der Reihenfolge der Datei
std::vector<Wertpapier> Katalog::fuerAnbieter(const std::string& anbieter) const {
    // Ergebnis
    std::vector<Wertpapier> ergebnis;
    // Alle gültigen Wertpapiere prüfen
    for (const Wertpapier& wp : wertpapiere_) {
        // Liefert der Anbieter es?
        if (wp.bietet(anbieter)) ergebnis.push_back(wp);
    }
    // Gefilterte Liste
    return ergebnis;
}

// Liefert die gültigen Zentralbanken
const std::vector<Zentralbank>& Katalog::zentralbanken() const {
    // Referenz auf die interne Liste
    return zentralbanken_;
}

// Liefert die fehlerhaften Zentralbanken
const std::vector<KatalogFehler>& Katalog::zentralbankFehler() const {
    // Referenz auf die interne Liste
    return zentralbankFehler_;
}

// Sucht ein Wertpapier nach Symbol (lineare Suche; Kataloge sind klein)
const Wertpapier* Katalog::finde(const std::string& symbol) const {
    // Alle Wertpapiere durchgehen
    for (const Wertpapier& wp : wertpapiere_) {
        // Symbol vergleichen
        if (wp.symbol == symbol) {
            // Adresse des gefundenen Wertpapiers zurückgeben
            return &wp;
        }
    }
    // Nicht gefunden
    return nullptr;
}
