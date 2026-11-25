// Eigene Header-Datei mit der Klasse TwelveDataQuelle
#include "twelvedata_quelle.hpp"

// Fehlerklassen
#include "quellenfehler.hpp"
// leseApiSchluessel
#include "umgebung.hpp"

// JSON-Bibliothek nlohmann/json
#include <nlohmann/json.hpp>

// std::sort
#include <algorithm>
// std::isalnum für das Kodieren der URL
#include <cctype>
// std::from_chars
#include <charconv>
// std::chrono für das Datum
#include <chrono>
// std::invalid_argument aus E1
#include <stdexcept>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar.
// Datum und Zahl werden wie in alphavantage_quelle.cpp gelesen. Die Hilfsfunktionen sind hier bewusst eine eigene
// Kopie: So bleibt der getestete Code von Alpha Vantage durch F3 unverändert.
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

// "YYYY-MM-DD" in einen Kalendertag; wirft ParseFehler
std::chrono::sys_days leseDatum(const std::string& text, const std::string& symbol) {
    // Bestandteile
    int jahr = 0;
    unsigned monat = 0;
    unsigned tag = 0;
    // Teilstück vollständig als Zahl lesen
    auto lies = [&text](std::size_t start, std::size_t laenge, auto& wert) {
        // Anfang und Ende
        const char* a = text.data() + start;
        const char* e = a + laenge;
        // Umwandeln
        const auto r = std::from_chars(a, e, wert);
        // Erfolg nur bei vollständigem Lesen
        return r.ec == std::errc() && r.ptr == e;
    };
    // Format prüfen (Tageskurse haben kein Uhrzeit-Teil)
    const bool format = text.size() == 10 && text[4] == '-' && text[7] == '-' && lies(0, 4, jahr) &&
                        lies(5, 2, monat) && lies(8, 2, tag);
    // Kalenderdatum
    const std::chrono::year_month_day ymd{std::chrono::year{jahr}, std::chrono::month{monat}, std::chrono::day{tag}};
    // Ungültig?
    if (!format || !ymd.ok()) {
        // Fehler
        throw ParseFehler("Twelve Data response for " + symbol + ": invalid date '" + text + "'");
    }
    // Kalendertag
    return std::chrono::sys_days{ymd};
}

// Liest ein Feld eines Tages als Zahl (Text wie "357.95999" oder echte JSON-Zahl); wirft ParseFehler
double leseZahl(const json& tag, const char* feld, const std::string& datum, const std::string& symbol) {
    // Anfang aller Meldungen
    const std::string ort = "Twelve Data response for " + symbol + " on " + datum + ": field '" + feld + "'";
    // Feld suchen
    const auto it = tag.find(feld);
    // Fehlt
    if (it == tag.end()) throw ParseFehler(ort + " is missing");
    // Echte Zahl
    if (it->is_number()) return it->get<double>();
    // Weder Zahl noch Text
    if (!it->is_string()) throw ParseFehler(ort + " has the wrong type");
    // Text
    const std::string text = it->get<std::string>();
    // Hier landet die Zahl
    double wert = 0.0;
    // Umwandeln
    const auto r = std::from_chars(text.data(), text.data() + text.size(), wert);
    // Nur vollständige Zahlen
    if (text.empty() || r.ec != std::errc() || r.ptr != text.data() + text.size()) {
        // Fehler
        throw ParseFehler(ort + " is not a number: '" + text + "'");
    }
    // Zahl
    return wert;
}

// Text der Meldung aus einer Fehlerantwort ("message"); leer, wenn es keine gibt
std::string meldungAus(const std::string& body) {
    // Ohne Exceptions parsen
    const json j = json::parse(body, nullptr, false);
    // Objekt mit Text "message"?
    if (j.is_object() && j.contains("message") && j["message"].is_string()) return j["message"].get<std::string>();
    // Keine Meldung
    return "";
}

// Wirft den passenden Fehler zu einem Fehlercode von Twelve Data (gleich, ob er aus dem HTTP-Status oder aus "code" kommt)
[[noreturn]] void wirfFehler(int code, const std::string& meldung, const std::string& symbol) {
    // Text des Anbieters in Klammern anhängen, wenn vorhanden
    const std::string grund = meldung.empty() ? "" : ": " + meldung;
    // Ungültiger oder fehlender Key
    if (code == 401) throw HttpFehler(401, "Twelve Data rejected the request for " + symbol + " (401): API key invalid" + grund);
    // Nicht erlaubt
    if (code == 403) throw HttpFehler(403, "Twelve Data does not permit the request for " + symbol + " (403)" + grund);
    // Unbekanntes Symbol oder nicht im kostenlosen Tarif: wie "nicht gefunden" (E2)
    if (code == 404) {
        // Meldung mit beiden möglichen Gründen
        throw HttpFehler(404, "Twelve Data has no data for " + symbol + " (unknown symbol or not in the free plan)" + grund);
    }
    // Kontingent (8 pro Minute oder Tageskontingent): eigener Fehlertyp, damit E7 alte Daten liefern kann
    if (code == 429) throw KontingentFehler("Twelve Data request quota exhausted (" + symbol + ")" + grund);
    // Alles andere mit dem Code
    throw HttpFehler(code, "Twelve Data request for " + symbol + " failed (" + std::to_string(code) + ")" + grund);
}

}  // namespace

// Konstruktor: Client merken, Key sofort lesen (fehlt er, wirft leseApiSchluessel KonfigurationsFehler)
TwelveDataQuelle::TwelveDataQuelle(IHttpClient& client, const std::string& schluesselVariable)
    : client_(client), schluessel_(leseApiSchluessel(schluesselVariable)) {}

// URL für time_series mit Tageskursen, 100 Einträge (wie Alpha Vantage "compact")
std::string TwelveDataQuelle::url(const std::string& kuerzel) const {
    // Kürzel und Key für die URL kodieren
    return "https://api.twelvedata.com/time_series?symbol=" + urlKodieren(kuerzel) +
           "&interval=1day&outputsize=100&apikey=" + urlKodieren(schluessel_);
}

// Einfache Variante über ladeKursdaten
Kursreihe TwelveDataQuelle::ladeKurse(const Wertpapier& wertpapier) {
    // Nur die Kursreihe
    return ladeKursdaten(wertpapier).reihe;
}

// Anfrage stellen, Fehler auswerten, Antwort parsen, Cache-Hinweis übernehmen
Kursdaten TwelveDataQuelle::ladeKursdaten(const Wertpapier& wertpapier) {
    // Kürzel bei Twelve Data (wirft, wenn das Wertpapier dort nicht geführt wird)
    const std::string kuerzel = wertpapier.symbolFuer(anbieterId);
    // Anfrage (wirft NetzwerkFehler oder ZeitueberschreitungFehler)
    const HttpAntwort antwort = client_.get(url(kuerzel));
    // Fehlerstatus im HTTP: mit dem Text aus "message" in den passenden Fehler übersetzen
    if (antwort.status < 200 || antwort.status > 299) wirfFehler(antwort.status, meldungAus(antwort.body), wertpapier.symbol);
    // Inhalt auswerten (wertet auch "status": "error" aus) und den Hinweis des Caches übernehmen
    return Kursdaten{parse(antwort.body, wertpapier.symbol), antwort.veraltet, antwort.hinweis};
}

// Antwort in eine Kursreihe umwandeln
Kursreihe TwelveDataQuelle::parse(const std::string& text, const std::string& symbol) {
    // Geparstes Dokument
    json dokument;
    // Parsen versuchen
    try {
        // Text in JSON
        dokument = json::parse(text);
    } catch (const json::parse_error& e) {
        // Ungültiges JSON in eigenen Fehlertyp übersetzen
        throw ParseFehler("Twelve Data response for " + symbol + " is not valid JSON: " + e.what());
    }
    // Muss ein Objekt sein
    if (!dokument.is_object()) throw ParseFehler("Twelve Data response for " + symbol + " is not a JSON object");

    // Fehler im Inhalt: {"status": "error", "code": 404, "message": "..."}
    if (dokument.contains("status") && dokument["status"] == "error") {
        // Code (fehlt er, gilt 0 als "unbekannt")
        const int code = dokument.contains("code") && dokument["code"].is_number_integer() ? dokument["code"].get<int>() : 0;
        // Meldung
        const std::string meldung = dokument.contains("message") && dokument["message"].is_string()
                                        ? dokument["message"].get<std::string>() : "";
        // Passenden Fehler werfen
        wirfFehler(code, meldung, symbol);
    }

    // Die Tageskurse stehen in der Liste "values"
    const auto werte = dokument.find("values");
    // Fehlt sie oder ist keine Liste
    if (werte == dokument.end() || !werte->is_array()) {
        // Fehler nennt das Feld
        throw ParseFehler("Twelve Data response for " + symbol + ": field 'values' is missing");
    }

    // Gesammelte Kerzen
    std::vector<Kerze> kerzen;
    // Platz reservieren
    kerzen.reserve(werte->size());
    // Jeder Eintrag ist ein Tag
    for (const json& tag : *werte) {
        // Muss ein Objekt mit Text "datetime" sein
        if (!tag.is_object() || !tag.contains("datetime") || !tag["datetime"].is_string()) {
            // Fehler
            throw ParseFehler("Twelve Data response for " + symbol + ": entry needs a text field 'datetime'");
        }
        // Datum als Text und als Kalendertag
        const std::string datumText = tag["datetime"].get<std::string>();
        const std::chrono::sys_days datum = leseDatum(datumText, symbol);
        // Die fünf Werte
        const double open = leseZahl(tag, "open", datumText, symbol);
        const double high = leseZahl(tag, "high", datumText, symbol);
        const double low = leseZahl(tag, "low", datumText, symbol);
        const double close = leseZahl(tag, "close", datumText, symbol);
        const double volumen = leseZahl(tag, "volume", datumText, symbol);
        // Kerze erzeugen; der Konstruktor aus E1 prüft die Werte
        try {
            // Anhängen
            kerzen.emplace_back(datum, open, high, low, close, volumen);
        } catch (const std::invalid_argument& e) {
            // Inkonsistente Daten des Anbieters
            throw ParseFehler("Twelve Data response for " + symbol + " contains an invalid candle: " + e.what());
        }
    }
    // Twelve Data liefert den neuesten Tag zuerst: aufsteigend sortieren
    std::sort(kerzen.begin(), kerzen.end(), [](const Kerze& a, const Kerze& b) { return a.datum < b.datum; });
    // Kursreihe erzeugen; sie lehnt doppelte Tage ab (E1)
    try {
        // Fertige Reihe
        return Kursreihe(kerzen);
    } catch (const std::invalid_argument& e) {
        // Doppelter Tag: Fehler der Daten
        throw ParseFehler("Twelve Data response for " + symbol + " is inconsistent: " + e.what());
    }
}

// Bewertung für den Festplatten-Cache (E7)
Antwortbewertung TwelveDataQuelle::bewerte(const HttpAntwort& antwort) {
    // HTTP 429: Kontingent
    if (antwort.status == 429) return Antwortbewertung::KontingentErschoepft;
    // Andere Fehlerstatus: nicht speichern
    if (antwort.status < 200 || antwort.status > 299) return Antwortbewertung::Unbrauchbar;
    // Inhalt probeweise auswerten
    try {
        // Parsen
        parse(antwort.body, "cache check");
        // Gültig: speichern
        return Antwortbewertung::Verwertbar;
    } catch (const KontingentFehler&) {
        // "code": 429 im Inhalt
        return Antwortbewertung::KontingentErschoepft;
    } catch (const QuellenFehler&) {
        // Sonstige Fehler oder ungültiger Inhalt
        return Antwortbewertung::Unbrauchbar;
    }
}
