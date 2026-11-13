// Eigene Header-Datei mit der Klasse Anfragebehandlung
#include "anfragebehandlung.hpp"

// Die Analyse-Klassen (E3-E6), deren Ergebnisse ausgeliefert werden
#include "ereignisstudie.hpp"
#include "risikokennzahlen.hpp"
#include "supportresistance.hpp"
#include "trendindikatoren.hpp"
// Fehlerklassen der Datenquellen (E2)
#include "quellenfehler.hpp"

// JSON-Bibliothek nlohmann/json
#include <nlohmann/json.hpp>

// std::find für die Liste erlaubter Parameter
#include <algorithm>
// std::from_chars: Text in Zahlen umwandeln
#include <charconv>
// std::chrono für Datumswerte
#include <chrono>
// std::isfinite
#include <cmath>
// std::format für Datumstexte
#include <format>
// std::optional
#include <optional>
// std::invalid_argument aus den Analyse-Klassen
#include <stdexcept>
// std::string
#include <string>
// std::move
#include <utility>
// std::vector
#include <vector>

// Anonymer Namespace: Hilfsfunktionen und interne Typen nur in dieser Datei sichtbar
namespace {

// Kurzer Name für den JSON-Typ
using json = nlohmann::json;

// Interne Ausnahme: ein Fehler mit HTTP-Status und Meldung.
// Wird überall in dieser Datei geworfen und in behandle() in eine JSON-Fehlerantwort übersetzt.
struct ApiFehler {
    // HTTP-Status, z. B. 400 oder 404
    int status;
    // Meldung für den Benutzer
    std::string meldung;
};

// Baut eine Fehlerantwort {"fehler": "...", "status": ...}
Antwort fehlerAntwort(int status, const std::string& meldung) {
    // Antwortobjekt
    Antwort antwort;
    // Status setzen
    antwort.status = status;
    // JSON mit Meldung und Status
    antwort.body = json{{"fehler", meldung}, {"status", status}}.dump();
    // Fertige Antwort
    return antwort;
}

// Baut eine erfolgreiche Antwort mit Status 200 aus einem JSON-Objekt
Antwort okAntwort(const json& inhalt) {
    // Antwortobjekt (Status ist standardmäßig 200)
    Antwort antwort;
    // JSON als Text
    antwort.body = inhalt.dump();
    // Fertige Antwort
    return antwort;
}

// Prüft, dass die Anfrage nur erlaubte Parameter enthält (fängt Tippfehler wie "smaa=20" ab)
void pruefeErlaubteParameter(const Anfrage& anfrage, const std::vector<std::string>& erlaubt) {
    // Jeden übergebenen Parameter prüfen
    for (const auto& [name, wert] : anfrage.parameter) {
        // Steht der Name nicht in der Liste, ist er unbekannt
        if (std::find(erlaubt.begin(), erlaubt.end(), name) == erlaubt.end()) {
            // 400 mit dem Namen des Parameters
            throw ApiFehler{400, "unknown parameter '" + name + "'"};
        }
    }
}

// Liefert den Wert eines Parameters oder nullopt, wenn er fehlt
std::optional<std::string> parameter(const Anfrage& anfrage, const std::string& name) {
    // Parameter suchen
    const auto it = anfrage.parameter.find(name);
    // Nicht vorhanden
    if (it == anfrage.parameter.end()) {
        // Kein Wert
        return std::nullopt;
    }
    // Wert zurückgeben
    return it->second;
}

// Wandelt einen Text in eine positive ganze Zahl um; wirft 400 mit verständlicher Meldung
int positiveGanzzahl(const std::string& text, const std::string& name) {
    // Hier landet die Zahl
    int wert = 0;
    // Umwandeln
    const auto ergebnis = std::from_chars(text.data(), text.data() + text.size(), wert);
    // Nur gültig, wenn der ganze Text gelesen wurde und die Zahl größer als 0 ist
    if (text.empty() || ergebnis.ec != std::errc() || ergebnis.ptr != text.data() + text.size() || wert <= 0) {
        // 400
        throw ApiFehler{400, "parameter '" + name + "' must be a positive integer, got '" + text + "'"};
    }
    // Zahl zurückgeben
    return wert;
}

// Wandelt einen Text in eine Kommazahl >= 0 um; wirft 400
double nichtnegativeZahl(const std::string& text, const std::string& name) {
    // Hier landet die Zahl
    double wert = 0.0;
    // Umwandeln (unabhängig von der Spracheinstellung: Punkt als Dezimaltrenner)
    const auto ergebnis = std::from_chars(text.data(), text.data() + text.size(), wert);
    // Gültig nur bei vollständigem Text, endlicher Zahl und Wert >= 0
    if (text.empty() || ergebnis.ec != std::errc() || ergebnis.ptr != text.data() + text.size() ||
        !std::isfinite(wert) || wert < 0.0) {
        // 400
        throw ApiFehler{400, "parameter '" + name + "' must be a number >= 0, got '" + text + "'"};
    }
    // Zahl zurückgeben
    return wert;
}

// Zerlegt "12,26,9" in {"12", "26", "9"}
std::vector<std::string> teile(const std::string& text) {
    // Ergebnisliste
    std::vector<std::string> teile;
    // Anfang des aktuellen Teils
    std::size_t anfang = 0;
    // Bis kein Komma mehr gefunden wird
    while (true) {
        // Nächstes Komma suchen
        const std::size_t komma = text.find(',', anfang);
        // Teil bis zum Komma (oder bis zum Ende) anhängen
        teile.push_back(text.substr(anfang, komma == std::string::npos ? std::string::npos : komma - anfang));
        // Kein weiteres Komma: fertig
        if (komma == std::string::npos) {
            // Schleife verlassen
            break;
        }
        // Hinter dem Komma weitermachen
        anfang = komma + 1;
    }
    // Alle Teile zurückgeben
    return teile;
}

// Wandelt "YYYY-MM-DD" in einen Kalendertag um; wirft 400 bei ungültigem Datum
std::chrono::sys_days datumParameter(const std::string& text, const std::string& name) {
    // Bestandteile
    int jahr = 0;
    unsigned monat = 0;
    unsigned tag = 0;
    // Lambda: liest ein Stück des Textes als Zahl und prüft, dass es vollständig gelesen wurde
    auto lies = [&text](std::size_t start, std::size_t laenge, auto& wert) {
        // Anfang und Ende des Stücks
        const char* a = text.data() + start;
        const char* e = a + laenge;
        // Umwandeln
        const auto r = std::from_chars(a, e, wert);
        // Erfolg nur, wenn alles gelesen wurde
        return r.ec == std::errc() && r.ptr == e;
    };
    // Format prüfen: 10 Zeichen, Bindestriche an Position 4 und 7, Zahlen dazwischen
    const bool format = text.size() == 10 && text[4] == '-' && text[7] == '-' && lies(0, 4, jahr) &&
                        lies(5, 2, monat) && lies(8, 2, tag);
    // Kalenderdatum bilden
    const std::chrono::year_month_day ymd{std::chrono::year{jahr}, std::chrono::month{monat}, std::chrono::day{tag}};
    // Ungültiges Format oder Datum, das es nicht gibt (z. B. 2026-02-30)
    if (!format || !ymd.ok()) {
        // 400
        throw ApiFehler{400, "parameter '" + name + "' must be a date YYYY-MM-DD, got '" + text + "'"};
    }
    // Kalendertag zurückgeben
    return std::chrono::sys_days{ymd};
}

// Datum einer Kerze als "YYYY-MM-DD"
std::string datumText(std::chrono::system_clock::time_point zeitpunkt) {
    // Auf den Tag abrunden und formatieren
    return std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(zeitpunkt));
}

// Wandelt eine Indikatorreihe in ein JSON-Array um; "nicht verfügbar" wird zu null
json alsJson(const Indikatorreihe& reihe) {
    // Leeres Array
    json array = json::array();
    // Jeden Wert übernehmen
    for (const auto& wert : reihe) {
        // Vorhandener Wert als Zahl, sonst null
        array.push_back(wert ? json(*wert) : json(nullptr));
    }
    // Fertiges Array
    return array;
}

// Wertpapier als JSON-Objekt
json alsJson(const Wertpapier& wp) {
    // Die vier Felder
    return json{{"symbol", wp.symbol}, {"name", wp.name}, {"land", wp.land}, {"waehrung", wp.waehrung}};
}

// Gemeinsame Felder aller Antworten zu einem Wertpapier: Symbol, Währung, Aktualität
json kopf(const Wertpapier& wp, const Kursdaten& daten) {
    // Hinweis als Text oder null, wenn es keinen gibt
    const json hinweis = daten.hinweis.empty() ? json(nullptr) : json(daten.hinweis);
    // Objekt mit den Feldern
    return json{{"symbol", wp.symbol},       {"name", wp.name},
                {"land", wp.land},           {"waehrung", wp.waehrung},
                {"veraltet", daten.veraltet}, {"hinweis", hinweis}};
}

// Die gemeinsamen Parameter von /api/kurse, /api/indikatoren und /api/ereignisstudie
const std::vector<std::string> zeitraumParameter = {"symbol", "von", "bis"};

}  // namespace

// Konstruktor: Daten übernehmen
Anfragebehandlung::Anfragebehandlung(std::vector<Wertpapier> wertpapiere, IPriceSource& quelle,
                                     std::vector<Zinsentscheidung> entscheidungen)
    : wertpapiere_(std::move(wertpapiere)), quelle_(quelle), entscheidungen_(std::move(entscheidungen)) {}

// Zentrale Funktion: Pfad auswählen, Fehler in Antworten übersetzen. Wirft nie.
Antwort Anfragebehandlung::behandle(const Anfrage& anfrage) {
    // Alle Fehler der Endpunkte hier abfangen
    try {
        // Bekannte Pfade
        const bool bekannt = anfrage.pfad == "/api/wertpapiere" || anfrage.pfad == "/api/kurse" ||
                             anfrage.pfad == "/api/indikatoren" || anfrage.pfad == "/api/ereignisstudie";
        // Unbekannter Pfad: 404
        if (!bekannt) {
            // Fehlerantwort
            return fehlerAntwort(404, "unknown path '" + anfrage.pfad + "'");
        }
        // Die Schnittstelle liest nur: andere Methoden als GET sind nicht erlaubt
        if (anfrage.methode != "GET") {
            // 405
            return fehlerAntwort(405, "method '" + anfrage.methode + "' not allowed, use GET");
        }
        // Passenden Endpunkt aufrufen
        if (anfrage.pfad == "/api/wertpapiere") {
            // Liste der Wertpapiere
            return wertpapiere(anfrage);
        }
        if (anfrage.pfad == "/api/kurse") {
            // Kerzen
            return kurse(anfrage);
        }
        if (anfrage.pfad == "/api/indikatoren") {
            // Indikatoren
            return indikatoren(anfrage);
        }
        // Bleibt nur die Ereignisstudie
        return ereignisstudie(anfrage);
    } catch (const ApiFehler& f) {
        // Eigene Fehler (400, 404 ...) mit ihrem Status
        return fehlerAntwort(f.status, f.meldung);
    } catch (const KontingentFehler& f) {
        // Kontingent des Anbieters erschöpft und nichts im Cache: Dienst vorübergehend nicht verfügbar
        return fehlerAntwort(503, f.what());
    } catch (const QuellenFehler& f) {
        // Anbieter liefert keine brauchbaren Daten (Netz, HTTP, JSON): Fehler "hinter" unserem Server
        return fehlerAntwort(502, f.what());
    } catch (const std::invalid_argument& f) {
        // Parameter gültig, aber mit den vorhandenen Daten nicht berechenbar (z. B. Reihe zu kurz)
        return fehlerAntwort(422, f.what());
    } catch (const std::exception&) {
        // Unerwarteter Fehler: keine internen Details nach außen geben
        return fehlerAntwort(500, "internal server error");
    }
}

// Sucht das Wertpapier zum Parameter "symbol"
const Wertpapier& Anfragebehandlung::findeWertpapier(const Anfrage& anfrage) const {
    // Parameter lesen
    const auto symbol = parameter(anfrage, "symbol");
    // Fehlt er oder ist er leer: 400
    if (!symbol || symbol->empty()) {
        // Pflichtparameter
        throw ApiFehler{400, "parameter 'symbol' is required"};
    }
    // Im Katalog suchen
    for (const Wertpapier& wp : wertpapiere_) {
        // Symbol vergleichen
        if (wp.symbol == *symbol) {
            // Gefunden
            return wp;
        }
    }
    // Nicht im Katalog: 404
    throw ApiFehler{404, "unknown symbol '" + *symbol + "'"};
}

// Lädt die Kurse und filtert sie auf den Zeitraum von/bis
Kursdaten Anfragebehandlung::ladeImZeitraum(const Wertpapier& wp, const Anfrage& anfrage) {
    // Optionale Grenzen lesen und prüfen (vor dem Laden, damit kein Kontingent verschwendet wird)
    const auto vonText = parameter(anfrage, "von");
    const auto bisText = parameter(anfrage, "bis");
    // Ohne Angabe: keine Grenze
    std::optional<std::chrono::sys_days> von;
    std::optional<std::chrono::sys_days> bis;
    // Untere Grenze umwandeln
    if (vonText) {
        // Wirft 400 bei ungültigem Datum
        von = datumParameter(*vonText, "von");
    }
    // Obere Grenze umwandeln
    if (bisText) {
        // Wirft 400 bei ungültigem Datum
        bis = datumParameter(*bisText, "bis");
    }
    // Beide gesetzt, aber vertauscht
    if (von && bis && *von > *bis) {
        // 400
        throw ApiFehler{400, "parameter 'von' must not be after 'bis'"};
    }
    // Kurse über die Quelle laden (wirft QuellenFehler bei Problemen)
    Kursdaten daten = quelle_.ladeKursdaten(wp);
    // Ohne Grenzen nichts weiter zu tun
    if (!von && !bis) {
        // Unverändert zurückgeben
        return daten;
    }
    // Neue Reihe mit den Kerzen im Zeitraum
    Kursreihe gefiltert;
    // Alle Kerzen durchgehen
    for (std::size_t i = 0; i < daten.reihe.groesse(); ++i) {
        // Tag der Kerze
        const auto tag = std::chrono::floor<std::chrono::days>(daten.reihe[i].datum);
        // Im Zeitraum (Grenzen eingeschlossen)?
        if ((!von || tag >= *von) && (!bis || tag <= *bis)) {
            // Kerze übernehmen (Reihenfolge bleibt aufsteigend)
            gefiltert.hinzufuegen(daten.reihe[i]);
        }
    }
    // Gefilterte Reihe einsetzen
    daten.reihe = gefiltert;
    // Zurückgeben
    return daten;
}

// GET /api/wertpapiere
Antwort Anfragebehandlung::wertpapiere(const Anfrage& anfrage) const {
    // Dieser Endpunkt hat keine Parameter
    pruefeErlaubteParameter(anfrage, {});
    // Leeres Array
    json liste = json::array();
    // Jedes Wertpapier anhängen
    for (const Wertpapier& wp : wertpapiere_) {
        // Als JSON-Objekt
        liste.push_back(alsJson(wp));
    }
    // Antwort {"wertpapiere": [...]}
    return okAntwort(json{{"wertpapiere", liste}});
}

// GET /api/kurse
Antwort Anfragebehandlung::kurse(const Anfrage& anfrage) {
    // Nur symbol, von, bis
    pruefeErlaubteParameter(anfrage, zeitraumParameter);
    // Wertpapier suchen (400/404)
    const Wertpapier& wp = findeWertpapier(anfrage);
    // Kurse laden und filtern
    const Kursdaten daten = ladeImZeitraum(wp, anfrage);
    // Leeres Array für die Kerzen
    json kerzen = json::array();
    // Jede Kerze als Objekt
    for (std::size_t i = 0; i < daten.reihe.groesse(); ++i) {
        // Aktuelle Kerze
        const Kerze& k = daten.reihe[i];
        // Felder übernehmen
        kerzen.push_back(json{{"datum", datumText(k.datum)}, {"open", k.open},   {"high", k.high},
                              {"low", k.low},                {"close", k.close}, {"volumen", k.volumen}});
    }
    // Kopf plus Kerzen
    json inhalt = kopf(wp, daten);
    inhalt["kerzen"] = kerzen;
    // Antwort
    return okAntwort(inhalt);
}

// GET /api/indikatoren
Antwort Anfragebehandlung::indikatoren(const Anfrage& anfrage) {
    // Erlaubte Parameter: Zeitraum plus alle Perioden
    pruefeErlaubteParameter(anfrage, {"symbol", "von", "bis", "sma", "ema", "rsi", "atr", "macd", "bollinger",
                                      "fenster", "toleranz"});
    // Einfache Perioden mit Standardwerten; value_or liefert den Standard, wenn der Parameter fehlt
    const int sma = positiveGanzzahl(parameter(anfrage, "sma").value_or("20"), "sma");
    const int ema = positiveGanzzahl(parameter(anfrage, "ema").value_or("20"), "ema");
    const int rsi = positiveGanzzahl(parameter(anfrage, "rsi").value_or("14"), "rsi");
    const int atr = positiveGanzzahl(parameter(anfrage, "atr").value_or("14"), "atr");
    const int fenster = positiveGanzzahl(parameter(anfrage, "fenster").value_or("5"), "fenster");
    const double toleranz = nichtnegativeZahl(parameter(anfrage, "toleranz").value_or("0.02"), "toleranz");

    // MACD: genau drei Zahlen "schnell,langsam,signal"
    const std::vector<std::string> macdTeile = teile(parameter(anfrage, "macd").value_or("12,26,9"));
    // Andere Anzahl: 400
    if (macdTeile.size() != 3) {
        // Fehler
        throw ApiFehler{400, "parameter 'macd' must be 'fast,slow,signal', e.g. '12,26,9'"};
    }
    // Die drei Perioden
    const int macdSchnell = positiveGanzzahl(macdTeile[0], "macd");
    const int macdLangsam = positiveGanzzahl(macdTeile[1], "macd");
    const int macdSignal = positiveGanzzahl(macdTeile[2], "macd");
    // Die schnelle Periode muss kleiner sein (sonst würde E3 werfen; hier schon als Parameterfehler 400)
    if (macdSchnell >= macdLangsam) {
        // Fehler
        throw ApiFehler{400, "parameter 'macd': fast period must be smaller than slow period"};
    }

    // Bollinger: genau zwei Werte "periode,k"
    const std::vector<std::string> bbTeile = teile(parameter(anfrage, "bollinger").value_or("20,2"));
    // Andere Anzahl: 400
    if (bbTeile.size() != 2) {
        // Fehler
        throw ApiFehler{400, "parameter 'bollinger' must be 'period,k', e.g. '20,2'"};
    }
    // Periode und Breite
    const int bbPeriode = positiveGanzzahl(bbTeile[0], "bollinger");
    const double bbK = nichtnegativeZahl(bbTeile[1], "bollinger");

    // Erst jetzt, nach allen Prüfungen: Wertpapier suchen und Kurse laden
    const Wertpapier& wp = findeWertpapier(anfrage);
    const Kursdaten daten = ladeImZeitraum(wp, anfrage);

    // Analyse-Objekte für die Kursreihe (E3, E4, E5)
    const Trendindikatoren trend(daten.reihe);
    const Risikokennzahlen risiko(daten.reihe);
    const SupportResistance sr(daten.reihe);

    // Alle Berechnungen; zu kurze Reihen werfen std::invalid_argument -> 422 in behandle()
    const MacdErgebnis macd = trend.macd(macdSchnell, macdLangsam, macdSignal);
    const BollingerErgebnis bb = risiko.bollinger(bbPeriode, bbK);
    const PivotPunkte pivot = sr.pivotPunkte();
    const std::optional<double> volatilitaet = risiko.annualisierteVolatilitaet();

    // Datum jeder Kerze, damit der Client die Werte der richtigen x-Position zuordnen kann
    json tage = json::array();
    // Alle Daten sammeln
    for (std::size_t i = 0; i < daten.reihe.groesse(); ++i) {
        // Datum als Text
        tage.push_back(datumText(daten.reihe[i].datum));
    }
    // Niveaus als Liste von Objekten
    json niveaus = json::array();
    // Jedes Niveau
    for (const Niveau& n : sr.niveaus(fenster, toleranz)) {
        // Preis, Berührungen, Art
        niveaus.push_back(json{{"preis", n.preis},
                               {"beruehrungen", n.beruehrungen},
                               {"art", n.art == NiveauArt::Support ? "support" : "resistance"}});
    }

    // Kopf plus alle Ergebnisse
    json inhalt = kopf(wp, daten);
    // Tatsächlich verwendete Parameter (inklusive Standardwerte), damit der Client sie anzeigen kann
    inhalt["parameter"] = json{{"sma", sma},
                               {"ema", ema},
                               {"rsi", rsi},
                               {"atr", atr},
                               {"macd", {macdSchnell, macdLangsam, macdSignal}},
                               {"bollinger", {bbPeriode, bbK}},
                               {"fenster", fenster},
                               {"toleranz", toleranz}};
    // Zeitachse
    inhalt["daten"] = tage;
    // Trend und Momentum (E3)
    inhalt["sma"] = alsJson(trend.sma(sma));
    inhalt["ema"] = alsJson(trend.ema(ema));
    inhalt["rsi"] = alsJson(trend.rsi(rsi));
    inhalt["macd"] = json{{"linie", alsJson(macd.linie)},
                          {"signal", alsJson(macd.signal)},
                          {"histogramm", alsJson(macd.histogramm)}};
    // Volatilität und Risiko (E4)
    inhalt["bollinger"] = json{{"mitte", alsJson(bb.mitte)}, {"oben", alsJson(bb.oben)}, {"unten", alsJson(bb.unten)}};
    inhalt["atr"] = alsJson(risiko.atr(atr));
    inhalt["renditen"] = json{{"einfach", alsJson(risiko.einfacheRenditen())}, {"log", alsJson(risiko.logRenditen())}};
    inhalt["volatilitaet"] = volatilitaet ? json(*volatilitaet) : json(nullptr);
    inhalt["maxDrawdown"] = risiko.maxDrawdown();
    // Support/Resistance und Pivot-Punkte (E5)
    inhalt["niveaus"] = niveaus;
    inhalt["pivot"] = json{{"p", pivot.p}, {"r1", pivot.r1}, {"r2", pivot.r2}, {"s1", pivot.s1}, {"s2", pivot.s2}};
    // Antwort
    return okAntwort(inhalt);
}

// GET /api/ereignisstudie
Antwort Anfragebehandlung::ereignisstudie(const Anfrage& anfrage) {
    // Nur symbol, von, bis
    pruefeErlaubteParameter(anfrage, zeitraumParameter);
    // Wertpapier suchen (400/404)
    const Wertpapier& wp = findeWertpapier(anfrage);
    // Kurse laden und filtern
    const Kursdaten daten = ladeImZeitraum(wp, anfrage);
    // Ereignisstudie mit allen Entscheidungen (E6); passende Zentralbank wird dort ausgewählt
    const std::vector<Ereignis> ergebnis = Ereignisstudie(entscheidungen_).auswerten(wp, daten.reihe);
    // Leeres Array
    json ereignisse = json::array();
    // Jedes Ereignis als Objekt
    for (const Ereignis& e : ergebnis) {
        // Alle Felder
        ereignisse.push_back(json{{"zentralbank", e.entscheidung.zentralbank},
                                  {"datum", std::format("{:%Y-%m-%d}", e.entscheidung.datum)},
                                  {"leitzins", e.entscheidung.leitzins},
                                  {"reaktionstag", std::format("{:%Y-%m-%d}", e.reaktionstag)},
                                  {"kursVorher", e.kursVorher},
                                  {"kursNachher", e.kursNachher},
                                  {"veraenderung", e.veraenderung}});
    }
    // Kopf plus Ereignisse (leere Liste, wenn keine passende Zentralbank: kein Fehler)
    json inhalt = kopf(wp, daten);
    inhalt["ereignisse"] = ereignisse;
    // Antwort
    return okAntwort(inhalt);
}
