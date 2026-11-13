// Eigene Header-Datei mit der Klasse AlphaVantageQuelle
#include "alphavantage_quelle.hpp"

// Fehlerklassen der Datenquellen
#include "quellenfehler.hpp"
// leseApiSchluessel
#include "umgebung.hpp"

// JSON-Bibliothek nlohmann/json (Header-only, liegt in third_party/)
#include <nlohmann/json.hpp>

// std::sort für die zeitliche Sortierung der Kerzen
#include <algorithm>
// std::isalnum für das Kodieren der URL
#include <cctype>
// std::from_chars: wandelt Text in Zahlen um, ohne Exceptions und ohne Abhängigkeit von der Spracheinstellung
#include <charconv>
// std::chrono für das Datum
#include <chrono>
// std::invalid_argument (Fehler aus E1 beim Erzeugen einer Kerze)
#include <stdexcept>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Kurzer Name für den JSON-Typ der Bibliothek
using json = nlohmann::json;

// Kodiert einen Text für die Verwendung in einer URL (Prozent-Kodierung, RFC 3986).
// Buchstaben, Ziffern und - _ . ~ bleiben, alles andere wird zu %XX (z. B. "^" -> "%5E" bei Indizes).
std::string urlKodieren(const std::string& text) {
    // Hexadezimale Ziffern für die Kodierung
    static const char* const hex = "0123456789ABCDEF";
    // Ergebnis
    std::string ergebnis;
    // Jedes Zeichen einzeln betrachten
    for (unsigned char c : text) {
        // Erlaubte Zeichen unverändert übernehmen
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            // Zeichen anhängen
            ergebnis += static_cast<char>(c);
        } else {
            // Prozentzeichen und zwei Hex-Ziffern (obere und untere 4 Bit des Zeichens)
            ergebnis += '%';
            ergebnis += hex[c >> 4];
            ergebnis += hex[c & 0x0F];
        }
    }
    // Kodierten Text zurückgeben
    return ergebnis;
}

// Liest eine ganze Zahl aus einem Textstück; liefert false, wenn der Text keine vollständige Zahl ist
bool leseGanzzahl(const std::string& text, std::size_t start, std::size_t laenge, int& wert) {
    // Anfang und Ende des Textstücks
    const char* anfang = text.data() + start;
    const char* ende = anfang + laenge;
    // Umwandeln; ergebnis.ptr zeigt danach auf das erste nicht gelesene Zeichen
    const auto ergebnis = std::from_chars(anfang, ende, wert);
    // Erfolgreich nur, wenn kein Fehler auftrat und das ganze Stück gelesen wurde
    return ergebnis.ec == std::errc() && ergebnis.ptr == ende;
}

// Wandelt ein Datum im Format "YYYY-MM-DD" in einen Kalendertag um; wirft ParseFehler bei ungültigem Datum
std::chrono::sys_days leseDatum(const std::string& text, const std::string& symbol) {
    // Bestandteile des Datums
    int jahr = 0;
    int monat = 0;
    int tag = 0;
    // Länge 10, Bindestriche an Position 4 und 7, dazwischen nur Ziffern
    const bool formatOk = text.size() == 10 && text[4] == '-' && text[7] == '-' &&
                          leseGanzzahl(text, 0, 4, jahr) && leseGanzzahl(text, 5, 2, monat) &&
                          leseGanzzahl(text, 8, 2, tag);
    // Aus den Zahlen einen Kalendertag bilden (Jahr/Monat/Tag)
    const std::chrono::year_month_day ymd{std::chrono::year{jahr}, std::chrono::month{static_cast<unsigned>(monat)},
                                          std::chrono::day{static_cast<unsigned>(tag)}};
    // ok() prüft, ob es den Tag wirklich gibt (z. B. kein 31. Februar)
    if (!formatOk || !ymd.ok()) {
        // Fehler mit dem fehlerhaften Text
        throw ParseFehler("Alpha Vantage response for " + symbol + ": invalid date '" + text + "'");
    }
    // In sys_days umwandeln (Anzahl Tage seit 1970-01-01)
    return std::chrono::sys_days{ymd};
}

// Liest ein Feld einer Tageskerze als Zahl. Alpha Vantage liefert Zahlen als Text ("160.5000"),
// akzeptiert wird zur Sicherheit auch eine echte JSON-Zahl. Wirft ParseFehler, wenn das Feld fehlt oder ungültig ist.
double leseZahl(const json& tag, const char* feld, const std::string& datum, const std::string& symbol) {
    // Gemeinsamer Anfang aller Fehlermeldungen dieses Feldes
    const std::string ort = "Alpha Vantage response for " + symbol + " on " + datum + ": field '" + feld + "'";
    // Feld suchen
    const auto it = tag.find(feld);
    // Fehlendes Feld
    if (it == tag.end()) {
        // Fehler nennt Symbol, Datum und Feld
        throw ParseFehler(ort + " is missing");
    }
    // Schon eine Zahl: direkt übernehmen
    if (it->is_number()) {
        // Als double zurückgeben
        return it->get<double>();
    }
    // Weder Zahl noch Text: falscher Typ
    if (!it->is_string()) {
        // Fehler
        throw ParseFehler(ort + " has the wrong type");
    }
    // Text holen
    const std::string text = it->get<std::string>();
    // Hier landet die Zahl
    double wert = 0.0;
    // Text in Zahl umwandeln
    const auto ergebnis = std::from_chars(text.data(), text.data() + text.size(), wert);
    // Nur gültig, wenn der ganze Text eine Zahl war
    if (text.empty() || ergebnis.ec != std::errc() || ergebnis.ptr != text.data() + text.size()) {
        // Fehler mit dem fehlerhaften Text
        throw ParseFehler(ort + " is not a number: '" + text + "'");
    }
    // Zahl zurückgeben
    return wert;
}

// Prüft, ob ein Hinweistext von Alpha Vantage ein erschöpftes Kontingent meint
bool istKontingentHinweis(const std::string& text) {
    // Typische Formulierungen von Alpha Vantage
    return text.find("rate limit") != std::string::npos || text.find("call frequency") != std::string::npos ||
           text.find("requests per day") != std::string::npos;
}

}  // namespace

// Konstruktor: Client merken und den Key lesen. Fehlt er, wirft leseApiSchluessel KonfigurationsFehler.
AlphaVantageQuelle::AlphaVantageQuelle(IHttpClient& client, const std::string& schluesselVariable)
    : client_(client), schluessel_(leseApiSchluessel(schluesselVariable)) {}

// Baut die URL für TIME_SERIES_DAILY; Symbol und Key werden für die URL kodiert
std::string AlphaVantageQuelle::url(const std::string& symbol) const {
    // Basisadresse der API, Funktion, Symbol, Datenumfang und Key
    return "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=" + urlKodieren(symbol) +
           "&outputsize=compact&apikey=" + urlKodieren(schluessel_);
}

// Lädt die Kurse: nutzt ladeKursdaten und gibt nur die Kursreihe zurück
Kursreihe AlphaVantageQuelle::ladeKurse(const Wertpapier& wertpapier) {
    // Gleicher Ablauf, Hinweis wird hier nicht gebraucht
    return ladeKursdaten(wertpapier).reihe;
}

// Lädt die Kurse: Anfrage stellen, Status prüfen, Antwort parsen, Cache-Hinweis übernehmen
Kursdaten AlphaVantageQuelle::ladeKursdaten(const Wertpapier& wertpapier) {
    // Anfrage über die Schnittstelle (wirft bei Netzwerkfehler oder Zeitüberschreitung)
    const HttpAntwort antwort = client_.get(url(wertpapier.symbol));
    // HTTP-Status prüfen (wirft bei 401, 404, 429 usw.)
    pruefeStatus(antwort, "prices for " + wertpapier.symbol);
    // JSON in eine Kursreihe umwandeln und die Markierung des Caches (E7) übernehmen
    return Kursdaten{parse(antwort.body, wertpapier.symbol), antwort.veraltet, antwort.hinweis};
}

// Wandelt die JSON-Antwort in eine Kursreihe um
Kursreihe AlphaVantageQuelle::parse(const std::string& text, const std::string& symbol) {
    // Das geparste JSON-Dokument
    json dokument;
    // Parsen versuchen
    try {
        // Text in JSON umwandeln
        dokument = json::parse(text);
    } catch (const json::parse_error& e) {
        // Ungültiges JSON: in unseren eigenen Fehlertyp übersetzen (der Aufrufer kennt nlohmann nicht)
        throw ParseFehler("Alpha Vantage response for " + symbol + " is not valid JSON: " + e.what());
    }
    // Die Antwort muss ein JSON-Objekt { ... } sein
    if (!dokument.is_object()) {
        // Fehler
        throw ParseFehler("Alpha Vantage response for " + symbol + " is not a JSON object");
    }

    // Fehlerhinweise, die Alpha Vantage mit Status 200 schickt, auswerten
    // "Error Message": unbekanntes Symbol oder ungültige Anfrage -> wie HTTP 404 behandeln
    if (const auto it = dokument.find("Error Message"); it != dokument.end()) {
        // Text des Anbieters in die Meldung übernehmen (dump() funktioniert auch, falls es kein String ist)
        throw HttpFehler(404, "Alpha Vantage rejected the request for " + symbol + " (unknown symbol?): " +
                                  (it->is_string() ? it->get<std::string>() : it->dump()));
    }
    // "Note": ältere Form der Kontingent-Meldung
    if (const auto it = dokument.find("Note"); it != dokument.end()) {
        // Kontingent erschöpft
        throw KontingentFehler("Alpha Vantage request quota exhausted (" + symbol + "): " +
                               (it->is_string() ? it->get<std::string>() : it->dump()));
    }
    // "Information": Kontingent erschöpft oder sonstiger Hinweis (z. B. Premium-Funktion)
    if (const auto it = dokument.find("Information"); it != dokument.end()) {
        // Text des Hinweises
        const std::string hinweis = it->is_string() ? it->get<std::string>() : it->dump();
        // Ist von Kontingent bzw. Rate Limit die Rede?
        if (istKontingentHinweis(hinweis)) {
            // Kontingent erschöpft
            throw KontingentFehler("Alpha Vantage request quota exhausted (" + symbol + "): " + hinweis);
        }
        // Sonstiger Hinweis: allgemeiner Quellenfehler mit dem Text des Anbieters
        throw QuellenFehler("Alpha Vantage did not return prices for " + symbol + ": " + hinweis);
    }

    // Die eigentlichen Kursdaten stehen unter "Time Series (Daily)"
    const auto reihe = dokument.find("Time Series (Daily)");
    // Fehlt das Feld oder ist es kein Objekt, ist die Antwort unbrauchbar
    if (reihe == dokument.end() || !reihe->is_object()) {
        // Fehler nennt das fehlende Feld
        throw ParseFehler("Alpha Vantage response for " + symbol + ": field 'Time Series (Daily)' is missing");
    }

    // Gesammelte Kerzen
    std::vector<Kerze> kerzen;
    // Platz reservieren
    kerzen.reserve(reihe->size());
    // Jeder Eintrag: Schlüssel = Datum, Wert = Objekt mit den Kursen des Tages
    for (const auto& [datumText, tag] : reihe->items()) {
        // Jeder Tag muss ein Objekt sein
        if (!tag.is_object()) {
            // Fehler mit Datum
            throw ParseFehler("Alpha Vantage response for " + symbol + " on " + datumText + ": entry is not an object");
        }
        // Datum umwandeln
        const std::chrono::sys_days datum = leseDatum(datumText, symbol);
        // Die fünf Felder lesen (Namen genau wie bei Alpha Vantage)
        const double open = leseZahl(tag, "1. open", datumText, symbol);
        const double high = leseZahl(tag, "2. high", datumText, symbol);
        const double low = leseZahl(tag, "3. low", datumText, symbol);
        const double close = leseZahl(tag, "4. close", datumText, symbol);
        const double volumen = leseZahl(tag, "5. volume", datumText, symbol);
        // Kerze erzeugen; der Konstruktor aus E1 prüft High/Low/Open/Close/Volumen
        try {
            // Kerze anhängen
            kerzen.emplace_back(datum, open, high, low, close, volumen);
        } catch (const std::invalid_argument& e) {
            // Inkonsistente Daten des Anbieters als ParseFehler melden (Meldung aus E1 nennt das Datum)
            throw ParseFehler("Alpha Vantage response for " + symbol + " contains an invalid candle: " + e.what());
        }
    }
    // Zeitlich aufsteigend sortieren (nicht auf die Reihenfolge im JSON verlassen)
    std::sort(kerzen.begin(), kerzen.end(), [](const Kerze& a, const Kerze& b) { return a.datum < b.datum; });
    // Kursreihe erzeugen; sie prüft noch einmal die strenge Reihenfolge (E1)
    return Kursreihe(kerzen);
}

// Bewertet eine Antwort für den Cache
Antwortbewertung AlphaVantageQuelle::bewerte(const HttpAntwort& antwort) {
    // HTTP 429: Kontingent erschöpft
    if (antwort.status == 429) {
        // Nicht speichern, alte Daten verwenden
        return Antwortbewertung::KontingentErschoepft;
    }
    // Andere Fehlerstatus: nicht speichern
    if (antwort.status < 200 || antwort.status > 299) {
        // Unbrauchbar
        return Antwortbewertung::Unbrauchbar;
    }
    // Status 200: am Inhalt prüfen, ob es echte Kursdaten sind
    try {
        // Probeweise parsen; das Symbol dient nur der Fehlermeldung, die hier niemand sieht
        parse(antwort.body, "cache check");
        // Ließ sich parsen: speichern
        return Antwortbewertung::Verwertbar;
    } catch (const KontingentFehler&) {
        // "Note" oder "Information" zum Kontingent
        return Antwortbewertung::KontingentErschoepft;
    } catch (const QuellenFehler&) {
        // "Error Message", kaputtes JSON, fehlende Felder ...
        return Antwortbewertung::Unbrauchbar;
    }
}
