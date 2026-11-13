// Eigene Header-Datei mit der Klasse FredQuelle
#include "fred_quelle.hpp"

// Fehlerklassen
#include "quellenfehler.hpp"
// leseApiSchluessel
#include "umgebung.hpp"

// JSON-Bibliothek nlohmann/json
#include <nlohmann/json.hpp>

// std::sort, std::adjacent_find
#include <algorithm>
// std::isalnum für das Kodieren der URL
#include <cctype>
// std::from_chars
#include <charconv>
// std::format für Datumstexte
#include <format>
// std::invalid_argument aus dem Zinsentscheidung-Konstruktor
#include <stdexcept>
// std::move
#include <utility>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Kurzer Name für den JSON-Typ
using json = nlohmann::json;

// Prozent-Kodierung für URLs: Buchstaben, Ziffern und - _ . ~ bleiben, alles andere wird %XX
std::string urlKodieren(const std::string& text) {
    // Hexadezimale Ziffern
    static const char* const hex = "0123456789ABCDEF";
    // Ergebnis
    std::string ergebnis;
    // Jedes Zeichen einzeln
    for (unsigned char c : text) {
        // Erlaubte Zeichen unverändert
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // Anhängen
            ergebnis += static_cast<char>(c);
        } else {
            // %XX
            ergebnis += '%';
            ergebnis += hex[c >> 4];
            ergebnis += hex[c & 0x0F];
        }
    }
    // Fertig
    return ergebnis;
}

// "YYYY-MM-DD" in einen Kalendertag umwandeln; wirft ParseFehler
std::chrono::sys_days leseDatum(const std::string& text, const std::string& serie) {
    // Bestandteile
    int jahr = 0;
    unsigned monat = 0;
    unsigned tag = 0;
    // Lambda: Teilstück als Zahl lesen, vollständig
    auto lies = [&text](std::size_t start, std::size_t laenge, auto& wert) {
        // Anfang und Ende
        const char* a = text.data() + start;
        const char* e = a + laenge;
        // Umwandeln
        const auto r = std::from_chars(a, e, wert);
        // Erfolg nur bei vollständigem Lesen
        return r.ec == std::errc() && r.ptr == e;
    };
    // Format prüfen
    const bool format = text.size() == 10 && text[4] == '-' && text[7] == '-' && lies(0, 4, jahr) &&
                        lies(5, 2, monat) && lies(8, 2, tag);
    // Kalenderdatum
    const std::chrono::year_month_day ymd{std::chrono::year{jahr}, std::chrono::month{monat}, std::chrono::day{tag}};
    // Ungültig?
    if (!format || !ymd.ok()) {
        // Fehler
        throw ParseFehler("FRED response for " + serie + ": invalid date '" + text + "'");
    }
    // Kalendertag
    return std::chrono::sys_days{ymd};
}

// Liest "error_message" aus einer FRED-Fehlerantwort; leer, wenn es keine gibt
std::string fehlermeldung(const std::string& body) {
    // Ohne Exception parsen (allow_exceptions = false liefert bei Fehlern einen "discarded"-Wert)
    const json j = json::parse(body, nullptr, false);
    // Objekt mit Textfeld "error_message"?
    if (j.is_object() && j.contains("error_message") && j["error_message"].is_string()) {
        // Meldung zurückgeben
        return j["error_message"].get<std::string>();
    }
    // Keine Meldung
    return "";
}

}  // namespace

// Konstruktor: Client merken, Key sofort lesen
FredQuelle::FredQuelle(IHttpClient& client, const std::string& schluesselVariable, std::string ab)
    : client_(client), schluessel_(leseApiSchluessel(schluesselVariable)), ab_(std::move(ab)) {}

// URL für eine Zeitreihe
std::string FredQuelle::url(const std::string& serie) const {
    // series/observations als JSON, ab dem eingestellten Tag
    return "https://api.stlouisfed.org/fred/series/observations?series_id=" + urlKodieren(serie) +
           "&api_key=" + urlKodieren(schluessel_) + "&file_type=json&observation_start=" + urlKodieren(ab_);
}

// Lädt und wertet aus
std::vector<Zinsentscheidung> FredQuelle::ladeEntscheidungen(const Zentralbank& bank) {
    // Anfrage (wirft NetzwerkFehler / ZeitueberschreitungFehler)
    const HttpAntwort antwort = client_.get(url(bank.fredSerie));
    // Kontext für Fehlermeldungen
    const std::string kontext = "rates of " + bank.name + " (FRED series " + bank.fredSerie + ")";
    // Fehlerstatus: FRED schreibt den Grund als JSON in den Inhalt, den wollen wir in der Meldung haben
    if (antwort.status < 200 || antwort.status > 299) {
        // Text des Anbieters
        const std::string grund = fehlermeldung(antwort.body);
        // 429: Kontingent
        if (antwort.status == 429) {
            // Kontingentfehler
            throw KontingentFehler("Request for " + kontext + " failed (HTTP 429): request quota exhausted");
        }
        // Unbekannte Zeitreihe kommt bei FRED als 400 mit diesem Text: wie 404 behandeln (wie "unbekanntes Symbol")
        if (antwort.status == 400 && grund.find("does not exist") != std::string::npos) {
            // Nicht gefunden
            throw HttpFehler(404, "Request for " + kontext + " failed: series does not exist (" + grund + ")");
        }
        // Alle anderen Fehler (z. B. 400 mit ungültigem Key) mit Status und Grund
        throw HttpFehler(antwort.status, "Request for " + kontext + " failed (HTTP " + std::to_string(antwort.status) +
                                             ")" + (grund.empty() ? "" : ": " + grund));
    }
    // Tageswerte lesen und Entscheidungen ableiten
    return leiteEntscheidungenAb(leseBeobachtungen(antwort.body, bank.fredSerie), bank);
}

// Liest die Tageswerte
std::vector<FredBeobachtung> FredQuelle::leseBeobachtungen(const std::string& text, const std::string& serie) {
    // Geparstes Dokument
    json dokument;
    // Parsen versuchen
    try {
        // JSON lesen
        dokument = json::parse(text);
    } catch (const json::parse_error& e) {
        // Ungültiges JSON
        throw ParseFehler("FRED response for " + serie + " is not valid JSON: " + e.what());
    }
    // Es muss ein Objekt mit einer Liste "observations" sein
    if (!dokument.is_object() || !dokument.contains("observations") || !dokument["observations"].is_array()) {
        // Feld fehlt
        throw ParseFehler("FRED response for " + serie + ": field 'observations' is missing");
    }
    // Ergebnisliste
    std::vector<FredBeobachtung> ergebnis;
    // Jede Beobachtung
    for (const json& b : dokument["observations"]) {
        // Muss ein Objekt mit den Textfeldern "date" und "value" sein
        if (!b.is_object() || !b.contains("date") || !b["date"].is_string() || !b.contains("value") ||
            !b["value"].is_string()) {
            // Fehler
            throw ParseFehler("FRED response for " + serie + ": observation needs text fields 'date' and 'value'");
        }
        // Datum
        const std::string datumText = b["date"].get<std::string>();
        const std::chrono::sys_days datum = leseDatum(datumText, serie);
        // Wert als Text
        const std::string wertText = b["value"].get<std::string>();
        // "." bedeutet bei FRED: für diesen Tag kein Wert (z. B. Feiertag) -> überspringen
        if (wertText == ".") {
            // Nächste Beobachtung
            continue;
        }
        // Wert als Zahl
        double wert = 0.0;
        const auto r = std::from_chars(wertText.data(), wertText.data() + wertText.size(), wert);
        // Muss vollständig eine Zahl sein
        if (wertText.empty() || r.ec != std::errc() || r.ptr != wertText.data() + wertText.size()) {
            // Fehler mit Datum
            throw ParseFehler("FRED response for " + serie + " on " + datumText + ": value is not a number: '" +
                              wertText + "'");
        }
        // Anhängen
        ergebnis.push_back({datum, wert});
    }
    // Aufsteigend sortieren (nicht auf die Reihenfolge der Antwort verlassen)
    std::sort(ergebnis.begin(), ergebnis.end(),
              [](const FredBeobachtung& a, const FredBeobachtung& b) { return a.datum < b.datum; });
    // Doppelte Tage sind ein Fehler der Daten
    const auto doppelt = std::adjacent_find(ergebnis.begin(), ergebnis.end(),
                                            [](const FredBeobachtung& a, const FredBeobachtung& b) {
                                                return a.datum == b.datum;
                                            });
    // Gefunden?
    if (doppelt != ergebnis.end()) {
        // Fehler mit Datum
        throw ParseFehler("FRED response for " + serie + ": duplicate date " + std::format("{:%Y-%m-%d}", doppelt->datum));
    }
    // Fertige Liste
    return ergebnis;
}

// Leitet die Entscheidungen aus den Änderungen ab
std::vector<Zinsentscheidung> FredQuelle::leiteEntscheidungenAb(const std::vector<FredBeobachtung>& beobachtungen,
                                                               const Zentralbank& bank) {
    // Ergebnisliste
    std::vector<Zinsentscheidung> ergebnis;
    // Ab dem zweiten Tag mit dem Vortag (mit Wert) vergleichen
    for (std::size_t i = 1; i < beobachtungen.size(); ++i) {
        // Gleicher Wert wie vorher: keine Entscheidung erkennbar
        if (beobachtungen[i].wert == beobachtungen[i - 1].wert) {
            // Weiter
            continue;
        }
        // Änderung: eine Entscheidung für jedes Land der Zentralbank
        for (const std::string& land : bank.laender) {
            // Der Konstruktor prüft die Werte (z. B. endliche Zahl)
            try {
                // Anhängen
                ergebnis.emplace_back(bank.name, land, beobachtungen[i].datum, beobachtungen[i].wert);
            } catch (const std::invalid_argument& e) {
                // Ungültiger Wert (z. B. unendlich) als Fehler der Daten melden
                throw ParseFehler("FRED data for " + bank.name + " contains an invalid rate: " + e.what());
            }
        }
    }
    // Fertige Liste (aufsteigend, weil die Beobachtungen sortiert sind)
    return ergebnis;
}

// Bewertung für den Festplatten-Cache
Antwortbewertung FredQuelle::bewerte(const HttpAntwort& antwort) {
    // 429: Kontingent
    if (antwort.status == 429) {
        // Alte Daten verwenden
        return Antwortbewertung::KontingentErschoepft;
    }
    // Sonstige Fehlerstatus
    if (antwort.status < 200 || antwort.status > 299) {
        // Nicht speichern
        return Antwortbewertung::Unbrauchbar;
    }
    // Status 200: Inhalt probeweise lesen
    try {
        // Lesen
        leseBeobachtungen(antwort.body, "cache check");
        // Gültig
        return Antwortbewertung::Verwertbar;
    } catch (const QuellenFehler&) {
        // Ungültig
        return Antwortbewertung::Unbrauchbar;
    }
}
