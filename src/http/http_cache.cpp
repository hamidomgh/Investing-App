// Eigene Header-Datei mit CachingHttpClient
#include "http_cache.hpp"

// NetzwerkFehler und KonfigurationsFehler
#include "quellenfehler.hpp"

// std::uint64_t für den Hash
#include <cstdint>
// std::format für Hex-Zahlen und das Datum im Hinweis (C++20)
#include <format>
// std::ifstream und std::ofstream für die Dateien
#include <fstream>
// std::invalid_argument bei negativer Gültigkeit
#include <stdexcept>
// std::error_code für Dateioperationen ohne Exceptions
#include <system_error>
// std::move
#include <utility>

// Anonymer Namespace: Hilfsfunktionen nur in dieser Datei sichtbar
namespace {

// Kennung in der ersten Zeile jeder Cache-Datei, mit Formatversion
const std::string kennung = "MARKETLENS-CACHE 1";

// FNV-1a-Hash (64 Bit) eines Textes. Einfach, schnell und auf jedem System gleich
// (im Gegensatz zu std::hash, dessen Ergebnis der Standard nicht festlegt).
// Dient als Dateiname (aus der URL) und als Prüfsumme (über den Inhalt).
std::uint64_t fnv1a(const std::string& text) {
    // Startwert ("offset basis") des Verfahrens
    std::uint64_t hash = 14695981039346656037ULL;
    // Jedes Byte einarbeiten
    for (unsigned char c : text) {
        // Byte per XOR einmischen
        hash ^= c;
        // Mit der FNV-Primzahl multiplizieren (Überlauf ist gewollt, unsigned rechnet modulo 2^64)
        hash *= 1099511628211ULL;
    }
    // Fertiger Hash
    return hash;
}

// Hash als 16-stellige Hex-Zahl, z. B. "00af3c..." (gleiche Länge für alle Dateinamen)
std::string hex(std::uint64_t wert) {
    // {:016x}: hexadezimal, mit Nullen auf 16 Stellen aufgefüllt
    return std::format("{:016x}", wert);
}

// Liest eine Kopfzeile "schluessel wert" und liefert den Wert; leer, wenn die Zeile nicht passt
std::optional<std::string> leseKopf(std::istream& ein, const std::string& schluessel) {
    // Eine Zeile lesen
    std::string zeile;
    // Datei zu Ende: Kopf unvollständig
    if (!std::getline(ein, zeile)) {
        // Nichts gefunden
        return std::nullopt;
    }
    // Die Zeile muss mit "schluessel " beginnen
    const std::string anfang = schluessel + " ";
    // Anderer Anfang: Datei beschädigt
    if (zeile.rfind(anfang, 0) != 0) {
        // Nichts gefunden
        return std::nullopt;
    }
    // Den Rest der Zeile nach dem Leerzeichen zurückgeben
    return zeile.substr(anfang.size());
}

// Wandelt einen Text vollständig in eine ganze Zahl um; leer bei ungültigem Text
std::optional<long long> alsZahl(const std::string& text) {
    // Hier landet die Zahl
    long long wert = 0;
    // Position nach der Zahl
    std::size_t gelesen = 0;
    // stoll wirft bei ungültigem Text oder Überlauf
    try {
        // Umwandeln
        wert = std::stoll(text, &gelesen);
    } catch (const std::exception&) {
        // Keine Zahl
        return std::nullopt;
    }
    // Nur gültig, wenn der ganze Text gelesen wurde
    if (gelesen != text.size()) {
        // Reste hinter der Zahl
        return std::nullopt;
    }
    // Zahl zurückgeben
    return wert;
}

}  // namespace

// Standard-Bewertung nach HTTP-Status
Antwortbewertung standardBewertung(const HttpAntwort& antwort) {
    // Erfolg
    if (antwort.status >= 200 && antwort.status <= 299) {
        // Speichern
        return Antwortbewertung::Verwertbar;
    }
    // Zu viele Anfragen
    if (antwort.status == 429) {
        // Kontingent erschöpft
        return Antwortbewertung::KontingentErschoepft;
    }
    // Alles andere
    return Antwortbewertung::Unbrauchbar;
}

// Konstruktor: Werte übernehmen, Gültigkeit prüfen, Ordner anlegen
CachingHttpClient::CachingHttpClient(IHttpClient& innen, const IUhr& uhr, std::filesystem::path verzeichnis,
                                     std::chrono::seconds gueltigkeit, Bewerter bewerter)
    : innen_(innen), uhr_(uhr), verzeichnis_(std::move(verzeichnis)), gueltigkeit_(gueltigkeit),
      bewerter_(std::move(bewerter)) {
    // Eine negative Gültigkeit ist sinnlos
    if (gueltigkeit_.count() < 0) {
        // Fehler
        throw std::invalid_argument("Cache validity must be >= 0 seconds");
    }
    // Fehlercode statt Exception, damit die Meldung unseren eigenen Fehlertyp bekommt
    std::error_code fehler;
    // Ordner samt übergeordneten Ordnern anlegen (tut nichts, wenn er schon existiert)
    std::filesystem::create_directories(verzeichnis_, fehler);
    // Nicht anlegbar (z. B. keine Schreibrechte) oder existiert als Datei statt Ordner
    if (fehler || !std::filesystem::is_directory(verzeichnis_)) {
        // Konfigurationsfehler mit Pfad
        throw KonfigurationsFehler("Cache directory cannot be created: " + verzeichnis_.string());
    }
}

// Dateiname = Hash der URL; die URL selbst (mit API-Key) steht nirgends auf der Platte
std::filesystem::path CachingHttpClient::dateiFuer(const std::string& url) const {
    // Ordner + 16 Hex-Zeichen + Endung
    return verzeichnis_ / (hex(fnv1a(url)) + ".cache");
}

// Liest und prüft eine Cache-Datei. Jede Abweichung vom erwarteten Format gilt als "beschädigt".
//
// Aufbau einer Cache-Datei:
//   MARKETLENS-CACHE 1                 Kennung und Version
//   url <16 hex>                       Hash der URL (prüft, dass die Datei zur URL gehört)
//   abgerufen <Sekunden seit 1970>     Zeitpunkt des Abrufs
//   status <HTTP-Status>
//   laenge <Anzahl Bytes des Inhalts>
//   pruefsumme <16 hex>                Hash des Inhalts (erkennt veränderte Bytes)
//   <Inhalt, genau "laenge" Bytes>
std::optional<CachingHttpClient::Eintrag> CachingHttpClient::lese(const std::string& url) const {
    // Datei im Binärmodus öffnen (keine Umwandlung von Zeilenenden)
    std::ifstream ein(dateiFuer(url), std::ios::binary);
    // Keine Datei: kein Eintrag
    if (!ein) {
        // Nichts im Cache
        return std::nullopt;
    }
    // Erste Zeile: Kennung
    std::string zeile;
    // Fehlt sie oder stimmt sie nicht, ist die Datei keine (gültige) Cache-Datei
    if (!std::getline(ein, zeile) || zeile != kennung) {
        // Beschädigt
        return std::nullopt;
    }
    // Die fünf Kopfzeilen in fester Reihenfolge lesen
    const auto urlHash = leseKopf(ein, "url");
    const auto abgerufen = leseKopf(ein, "abgerufen");
    const auto status = leseKopf(ein, "status");
    const auto laenge = leseKopf(ein, "laenge");
    const auto pruefsumme = leseKopf(ein, "pruefsumme");
    // Fehlt eine Zeile, ist der Kopf unvollständig
    if (!urlHash || !abgerufen || !status || !laenge || !pruefsumme) {
        // Beschädigt
        return std::nullopt;
    }
    // Die Datei muss zu genau dieser URL gehören
    if (*urlHash != hex(fnv1a(url))) {
        // Falsche Datei
        return std::nullopt;
    }
    // Zahlen aus den Kopfzeilen
    const auto sekunden = alsZahl(*abgerufen);
    const auto statusZahl = alsZahl(*status);
    const auto bytes = alsZahl(*laenge);
    // Keine gültigen Zahlen oder negative Länge
    if (!sekunden || !statusZahl || !bytes || *bytes < 0) {
        // Beschädigt
        return std::nullopt;
    }
    // Platz für genau "laenge" Bytes
    std::string inhalt(static_cast<std::size_t>(*bytes), '\0');
    // Inhalt lesen
    ein.read(inhalt.data(), *bytes);
    // Weniger Bytes als angegeben: Datei abgeschnitten (z. B. Absturz beim Schreiben)
    if (ein.gcount() != *bytes) {
        // Unvollständig
        return std::nullopt;
    }
    // Danach darf nichts mehr kommen (peek liefert EOF, wenn die Datei zu Ende ist)
    if (ein.peek() != std::char_traits<char>::eof()) {
        // Zusätzliche Bytes: beschädigt
        return std::nullopt;
    }
    // Prüfsumme des Inhalts vergleichen: erkennt veränderte Bytes
    if (*pruefsumme != hex(fnv1a(inhalt))) {
        // Inhalt verändert
        return std::nullopt;
    }
    // Gültiger Eintrag: Zeitpunkt aus Sekunden seit 1970 zurückrechnen
    const auto zeitpunkt = std::chrono::system_clock::time_point{std::chrono::seconds{*sekunden}};
    // Eintrag zurückgeben
    return Eintrag{zeitpunkt, HttpAntwort{static_cast<int>(*statusZahl), std::move(inhalt)}};
}

// Schreibt eine Cache-Datei "atomar": erst in eine temporäre Datei, dann umbenennen.
// Stürzt das Programm beim Schreiben ab, bleibt höchstens die temporäre Datei halb geschrieben;
// die eigentliche Cache-Datei ist entweder die alte oder die vollständige neue.
void CachingHttpClient::schreibe(const std::string& url, const HttpAntwort& antwort) const {
    // Ziel und temporäre Datei daneben
    const std::filesystem::path ziel = dateiFuer(url);
    const std::filesystem::path temp = ziel.string() + ".tmp";
    // Abrufzeit in ganzen Sekunden seit 1970
    const auto sekunden =
        std::chrono::duration_cast<std::chrono::seconds>(uhr_.jetzt().time_since_epoch()).count();
    // Temporäre Datei schreiben (eigener Block, damit sie vor dem Umbenennen geschlossen ist)
    {
        // Im Binärmodus öffnen, vorhandenen Inhalt verwerfen
        std::ofstream aus(temp, std::ios::binary | std::ios::trunc);
        // Kopfzeilen schreiben
        aus << kennung << '\n'
            << "url " << hex(fnv1a(url)) << '\n'
            << "abgerufen " << sekunden << '\n'
            << "status " << antwort.status << '\n'
            << "laenge " << antwort.body.size() << '\n'
            << "pruefsumme " << hex(fnv1a(antwort.body)) << '\n';
        // Inhalt unverändert schreiben
        aus.write(antwort.body.data(), static_cast<std::streamsize>(antwort.body.size()));
        // Alles auf die Platte bringen
        aus.flush();
        // Fehler beim Schreiben (z. B. Platte voll): temporäre Datei entfernen und aufgeben
        if (!aus) {
            // Fehlercode, damit remove keine Exception wirft
            std::error_code ignoriert;
            // Temporäre Datei löschen
            std::filesystem::remove(temp, ignoriert);
            // Nicht speichern; die Antwort wird trotzdem ausgeliefert
            return;
        }
    }
    // Fehlercode für das Umbenennen
    std::error_code fehler;
    // Umbenennen ersetzt die alte Datei in einem Schritt
    std::filesystem::rename(temp, ziel, fehler);
    // Klappt das nicht, die temporäre Datei aufräumen
    if (fehler) {
        // Fehlercode, damit remove keine Exception wirft
        std::error_code ignoriert;
        // Temporäre Datei löschen
        std::filesystem::remove(temp, ignoriert);
    }
}

// Aktuell, wenn der Eintrag jünger als die Gültigkeitsdauer ist
bool CachingHttpClient::istAktuell(const Eintrag& eintrag) const {
    // Alter des Eintrags
    const auto alter = uhr_.jetzt() - eintrag.abgerufen;
    // Ein Abrufzeitpunkt in der Zukunft (z. B. Uhr zurückgestellt) ist verdächtig: als abgelaufen behandeln
    if (alter < std::chrono::seconds{0}) {
        // Neu abrufen
        return false;
    }
    // Genau bei Erreichen der Gültigkeit abgelaufen
    return alter < gueltigkeit_;
}

// Erzeugt eine als veraltet markierte Kopie mit Hinweis, z. B.
// "Stale data from 2026-11-12 14:03 UTC (network error: ...)"
HttpAntwort CachingHttpClient::alsVeraltet(const Eintrag& eintrag, const std::string& grund) {
    // Kopie der gespeicherten Antwort
    HttpAntwort antwort = eintrag.antwort;
    // Markierung für den Aufrufer
    antwort.veraltet = true;
    // Abrufzeit auf Minuten gerundet, als lesbares Datum mit Uhrzeit (UTC)
    const auto minute = std::chrono::floor<std::chrono::minutes>(eintrag.abgerufen);
    // Hinweistext zusammensetzen
    antwort.hinweis = std::format("Stale data from {:%Y-%m-%d %H:%M} UTC ({})", minute, grund);
    // Fertige Antwort
    return antwort;
}

// Hauptfunktion: aus dem Cache liefern oder neu abrufen
HttpAntwort CachingHttpClient::get(const std::string& url) {
    // Vorhandenen Eintrag lesen (leer, wenn keiner da oder beschädigt)
    const std::optional<Eintrag> eintrag = lese(url);
    // Aktueller Eintrag: ohne Anfrage zurückgeben
    if (eintrag && istAktuell(*eintrag)) {
        // Treffer im Cache
        return eintrag->antwort;
    }
    // Hier landet die neue Antwort
    HttpAntwort neu;
    // Inneren Client fragen
    try {
        // Anfrage weitergeben
        neu = innen_.get(url);
    } catch (const NetzwerkFehler& f) {
        // Netzwerk weg oder Zeitüberschreitung: alte Daten sind besser als keine
        if (eintrag) {
            // Alten Eintrag mit Hinweis ausliefern
            return alsVeraltet(*eintrag, std::string("network error: ") + f.what());
        }
        // Nichts im Cache: Fehler unverändert weitergeben
        throw;
    }
    // Antwort bewerten und entsprechend handeln
    switch (bewerter_(neu)) {
        // Gültige Daten
        case Antwortbewertung::Verwertbar:
            // Für das nächste Mal speichern
            schreibe(url, neu);
            // Und zurückgeben
            return neu;
        // Kontingent erschöpft
        case Antwortbewertung::KontingentErschoepft:
            // Alte Daten vorhanden: mit Hinweis ausliefern
            if (eintrag) {
                // Veraltete Daten
                return alsVeraltet(*eintrag, "request quota exhausted");
            }
            // Sonst die Antwort weitergeben; die Quelle meldet dann den Kontingentfehler
            return neu;
        // Sonstige Fehlerantwort (z. B. 404): nicht speichern, keine alten Daten, unverändert weitergeben
        case Antwortbewertung::Unbrauchbar:
            // Antwort zurückgeben
            return neu;
    }
    // Wird nie erreicht (alle Fälle oben behandelt); nötig, damit der Compiler keine Warnung ausgibt
    return neu;
}
