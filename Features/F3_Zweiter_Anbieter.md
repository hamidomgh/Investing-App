# F3 – Zweiter Kursdaten-Anbieter (Twelve Data) mit zwei Servern nebeneinander

## 1. Ziel

MarketLens kann seine Kurse außer von Alpha Vantage auch von **Twelve Data** beziehen. Beide Varianten laufen
**gleichzeitig** als zwei getrennte Server: Alpha Vantage auf Port **8080** mit allen Wertpapieren, Twelve Data auf
Port **8081** nur mit den **US-Wertpapieren**. So lassen sich die Ergebnisse in zwei Browser-Tabs vergleichen. Die Daten
der beiden stören sich gegenseitig nicht.

## 2. Wie es zu Twelve Data kam (Schritt 0)

Vor der Beschreibung wurde jeweils mit dem eigenen kostenlosen Key geprüft, ob der Anbieter **Tageskurse** (Open, High,
Low, Close, Volumen) liefert. Ohne sie kann MarketLens keine Indikatoren berechnen.

| Datum | Anbieter | Anfrage | Antwort | Folge |
|---|---|---|---|---|
| 2026-11-16 | Finnhub (60/Minute) | `stock/candle`, AAPL | `{"error":"You don't have access to this resource."}` | Tageskurse nur im bezahlten Tarif → **nicht verwendbar** |
| 2026-11-16 | Twelve Data | `time_series`, AAPL | `{"meta":{…},"values":[{"datetime":"2026-11-13","open":"357.95999",…,"close":"356.26999",…}],…}` | **US-Aktien im kostenlosen Tarif** |
| 2026-11-16 | Twelve Data | `time_series`, MBG auf XETR | `{"code":404,"message":"This symbol is available starting with the Grow or Venture plan …","status":"error"}` | Xetra nur im bezahlten Tarif |

Gegenprobe: Der Schlusskurs von Apple am 2026-11-13 ist bei Alpha Vantage **356,27** und bei Twelve Data **356,26999**:
dieselben Daten bis auf eine Rundung.

**Entscheidung (2026-11-16):** Twelve Data als zweiter Anbieter, eigener Server auf Port 8081, **nur US-Wertpapiere**.

## 3. Umfang

**Gehört dazu:**
- neue Kursquelle `TwelveDataQuelle : IPriceSource` (gleiches Muster wie `AlphaVantageQuelle`): API-Key aus
  `TWELVEDATA_API_KEY` vor der ersten Anfrage, URL, Fehlerbehandlung wie E2, Umwandlung in validierte Kerzen (E1),
  Bewertung für den Festplatten-Cache (E7)
- Programmoption `--quelle alphavantage|twelvedata` (Standard `alphavantage`, also unverändertes Verhalten)
- getrennte Ablage je Anbieter (Abschnitt 5)
- je Wertpapier ein Symbol pro Anbieter im Katalog; ein Server listet nur die Wertpapiere, die sein Anbieter liefert
  (Abschnitt 6)
- Anzeige des Anbieters in der Oberfläche, damit man die beiden Tabs nicht verwechselt
- zwei Server nebeneinander (Abschnitt 7)
- aufgezeichnete Antworten und Tests wie bei Alpha Vantage

**Voraussetzung, aber eigener Schritt:** der **Live-HTTPS-Client** hinter der CMake-Option (cpp-httplib + OpenSSL,
`libssl-dev` im `Containerfile`). Ohne ihn laufen beide Server offline aus aufgezeichneten Dateien, und das größere
Kontingent spielt keine Rolle. Der Client ist für beide Anbieter derselbe und wird einmal gebaut. Reihenfolge-Vorschlag
in Abschnitt 9.

**Gehört nicht dazu:** Wertpapiere außerhalb der USA bei Twelve Data (bezahlter Tarif), Zusammenführen der Daten beider
Anbieter, automatische Wahl des Anbieters, Umrechnung von Währungen.

## 4. Twelve-Data-Schnittstelle

    GET https://api.twelvedata.com/time_series?symbol=<SYMBOL>&interval=1day&outputsize=100&apikey=<KEY>

Erfolgreiche Antwort (gekürzt, wie in Schritt 0 aufgezeichnet):

```json
{ "meta": { "symbol": "AAPL", "interval": "1day", "currency": "USD", "exchange": "NASDAQ", "mic_code": "XNGS", … },
  "values": [ { "datetime": "2026-11-13", "open": "357.95999", "high": "360.82999", "low": "355.75",
                "close": "356.26999", "volume": "39985300" }, … ],
  "status": "ok" }
```

Fehlerantwort: `{ "code": 404, "message": "…", "status": "error" }`.

| Teil der Antwort | Abbildung in MarketLens |
|---|---|
| `"status": "ok"` und `values` | je Eintrag eine `Kerze`; nach Datum aufsteigend sortieren (Twelve Data liefert den neuesten Tag zuerst) |
| `datetime` `"YYYY-MM-DD"` | Kalendertag; ungültig → `ParseFehler` |
| `open` … `volume` **als Text** | mit `std::from_chars` lesen (wie bei Alpha Vantage); fehlend oder keine Zahl → `ParseFehler` mit Datum und Feld |
| `"status": "error"`, `code` 401 | `HttpFehler(401)`: Key ungültig |
| `code` 404 | `HttpFehler(404)` mit dem Text von Twelve Data (unbekanntes Symbol **oder** nicht im kostenlosen Tarif) |
| `code` 429 | `KontingentFehler` (8 pro Minute bzw. Tageskontingent erschöpft; E7 liefert dann alte Daten mit Hinweis) |
| anderer `code` | `HttpFehler(code)` mit dem Text von Twelve Data |
| kein gültiges JSON, `values` fehlt | `ParseFehler` |
| inkonsistente Kerze | `ParseFehler` mit der Meldung aus E1 |

**Zu prüfen beim ersten Aufzeichnen:** ob Twelve Data Fehler mit HTTP-Status 200 und `"status":"error"` im Inhalt
meldet oder mit dem echten HTTP-Status. Die Quelle wertet **beides** aus: zuerst den HTTP-Status (`pruefeStatus` aus E2,
ergänzt um den Text aus `message`), dann `status`/`code` im Inhalt. Aufnehmen mit `curl -s -w " HTTP %{http_code}" …`.

Unterschiede zu Alpha Vantage, die im Code zu beachten sind: Liste `values` statt Objekt je Datum; Feldnamen ohne
Nummern (`open` statt `1. open`); Fehler mit `status`/`code`/`message` statt `Error Message`/`Note`/`Information`.

## 5. Getrennte Ablage (entschieden 2026-11-16)

| Was | Alpha Vantage (Port 8080) | Twelve Data (Port 8081) |
|---|---|---|
| aufgezeichnete Kurse (offline) | `daten/alphavantage/<SYMBOL>.json` | `daten/twelvedata/<SYMBOL>.json` |
| Festplatten-Cache (E7, live) | `cache/alphavantage/` | `cache/twelvedata/` |
| API-Key | `ALPHAVANTAGE_API_KEY` | `TWELVEDATA_API_KEY` |
| Zeichnungen im Browser (F1) | `localhost:8080` | `localhost:8081` |
| **gemeinsam:** Zinsdaten FRED | `daten/zinsen/` (beide lesen nur) | `daten/zinsen/` |
| **gemeinsam:** Katalog | `config/katalog.json` (beide lesen nur) | `config/katalog.json` |

- **Zeichnungen sind automatisch getrennt:** Der Browser legt `localStorage` je Adresse **einschließlich Port** an.
- **Umzug:** Die heutigen Kursdateien in `daten/` ziehen nach `daten/alphavantage/`; `daten/zinsen/` bleibt. Tests mit
  `DATEN_DIR` und `daten/README.md` werden angepasst; der Standardwert von `--daten` wird `daten/alphavantage`.
- Zwei Prozesse teilen sich nur Dateien, die beide lesen. Schreiben (Cache) tut jeder nur in seinen eigenen Ordner.

## 6. Symbole je Anbieter und „nur US“ (entschieden 2026-11-16)

Optionales Feld `symbole` je Wertpapier. Steht ein Anbieter dort, liefert dieser Anbieter das Wertpapier unter diesem
Kürzel; steht er nicht dort, gibt es das Wertpapier bei diesem Anbieter nicht.

```json
{ "symbol": "IBM",     "name": "International Business Machines", "land": "USA", "waehrung": "USD",
  "symbole": { "alphavantage": "IBM", "twelvedata": "IBM" } },
{ "symbol": "MBG.DEX", "name": "Mercedes-Benz Group AG", "land": "Deutschland", "waehrung": "EUR",
  "symbole": { "alphavantage": "MBG.DEX" } }
```

- **Fehlt `symbole` ganz** (ältere Kataloge, Tests): Das Wertpapier gilt für den Standardanbieter Alpha Vantage mit
  `symbol` als Kürzel. Bestehende Kataloge und Tests funktionieren unverändert.
- Der Server mit `--quelle twelvedata` übernimmt **nur** Wertpapiere mit einem Eintrag `twelvedata`. Damit zeigt Port
  8081 nur **IBM und AAPL**; `/api/wertpapiere` listet nur diese, andere Symbole ergeben 404 wie ein unbekanntes Symbol.
- Angezeigt wird immer `symbol` (gleich in beiden Tabs); angefragt wird das Kürzel des jeweiligen Anbieters.
- Ein ungültiger Eintrag in `symbole` (kein Objekt, leeres Kürzel) wird wie andere fehlerhafte Katalogeinträge gemeldet
  und übersprungen (E2).

## 7. Zwei Server nebeneinander

    # Terminal 1: Alpha Vantage, alle Wertpapiere
    podman run --rm -it -p 8080:8080 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev \
      ./build/projekt --quelle alphavantage --daten daten/alphavantage --cache cache/alphavantage --port 8080 --host 0.0.0.0
    # Terminal 2: Twelve Data, nur USA
    podman run --rm -it -p 8081:8081 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev \
      ./build/projekt --quelle twelvedata --daten daten/twelvedata --cache cache/twelvedata --port 8081 --host 0.0.0.0

- Browser: Tab 1 `http://localhost:8080/`, Tab 2 `http://localhost:8081/`.
- **Anbieter sichtbar:** neues Feld `anbieter` in den Antworten von `/api/wertpapiere` und den Endpunkten je Wertpapier;
  die Kopfzeile zeigt z. B. „Kurse: Twelve Data“.
- Startmeldung nennt Anbieter und Anzahl Wertpapiere, z. B. `MarketLens: 2 Wertpapiere (Twelve Data) …`.
- Belegter Port wird erkannt (E9).

## 8. Betroffene Dateien

| Datei | Änderung |
|---|---|
| `src/twelvedata_quelle.hpp` / `.cpp` | **neu:** `TwelveDataQuelle` (Parsen, Fehler, Bewertung für E7) |
| `src/kursdaten.hpp`, `src/katalog.*` | optionales Feld `symbole`; Filter „nur Wertpapiere des Anbieters“ |
| `src/alphavantage_quelle.cpp`, `src/dateiquelle.cpp` | Kürzel des Anbieters statt `symbol` anfragen |
| `src/dateiquelle.*` | Parser je Anbieter (Alpha-Vantage- oder Twelve-Data-Format) |
| `src/main.cpp` | Optionen `--quelle`, `--cache`; Standard `--daten daten/alphavantage` |
| `src/anfragebehandlung.*` | Feld `anbieter` in den Antworten |
| `web/app.js`, `web/index.html`, `web/erklaerungen.js` | Anbieter in der Kopfzeile; Erklärung („?“) zum Anbieter |
| `config/katalog.json` | `symbole` je Wertpapier (IBM und AAPL mit `twelvedata`) |
| `daten/` | Umzug nach `daten/alphavantage/`; neu `daten/twelvedata/IBM.json`, `AAPL.json` |
| `tests/fixtures/twelvedata/`, `tests/test_twelvedata.cpp` | **neu** |
| Tests mit `DATEN_DIR` | auf `daten/alphavantage` |
| `docs/F3_…`, `docs/testprotokoll.md` | Erklärung und manuelle Testfälle |

Die Analyse (E3–E6), der Ergebnis-Cache (E9) und die Zeichenwerkzeuge (F1) bleiben unverändert: Sie kennen nur
`IPriceSource` bzw. die Schnittstelle.

## 9. Offene Entscheidungen

| Nr. | Frage | Empfehlung |
|---|---|---|
| 1 | Reihenfolge | **(a)** F3 offline mit aufgezeichneten Twelve-Data-Dateien (kein `libssl-dev` nötig), **(b)** danach der Live-HTTPS-Client für beide Anbieter. So ist (a) sofort testbar |
| 2 | Arbeit auf einem Git-Zweig? | Ja (`git switch -c zweiter-anbieter`); `main` bleibt bis zum Abschluss unverändert |
| 3 | Anbieter in der Oberfläche | Kopfzeile „Kurse: Alpha Vantage“ bzw. „Kurse: Twelve Data“ |
| 4 | Aufzeichnung der Twelve-Data-Antworten | IBM und AAPL mit `outputsize=100`, im WSL-Terminal mit eigenem Key; Key-freie Prüfung mit `grep` wie bisher |

## 10. Prüfung

- **Automatisch:** `TwelveDataQuelle` mit `FakeHttpClient` und Testdaten in `tests/fixtures/twelvedata/`: gültige Antwort
  (neuester Tag zuerst → aufsteigend), Zahlen als Text, `status: error` mit `code` 401/404/429/anderes (jeweils mit und
  ohne passenden HTTP-Status), fehlende `values`, fehlendes Feld, ungültiges Datum, inkonsistente Kerze (E1), fehlender
  Key vor der ersten Anfrage, aufgezeichnete echte Antworten (IBM, AAPL); Katalog mit `symbole` (vorhanden, fehlend,
  ungültig) und Filter je Anbieter; getrennte Ordner; Feld `anbieter` in der Schnittstelle. Alle bestehenden Tests
  bleiben grün.
- **Gegenprobe mit echten Daten:** Für IBM und AAPL liefern beide Anbieter an gleichen Tagen gleiche Schlusskurse bis auf
  Rundung (Test mit Toleranz, z. B. 0,01).
- **Manuell (neue Fälle im Testprotokoll):** beide Server gleichzeitig; Tab 1 zeigt 6 Wertpapiere und „Alpha Vantage“,
  Tab 2 zeigt 2 und „Twelve Data“; gleiche Indikatoren für IBM in beiden Tabs (bis auf Rundung); Zeichnungen getrennt;
  ein Server mit Strg+C beendet, der andere läuft weiter; zweiter Start auf demselben Port abgelehnt.
