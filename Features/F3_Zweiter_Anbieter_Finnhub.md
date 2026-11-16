# F3 – Zweiter Kursdaten-Anbieter (Finnhub) mit zwei Servern nebeneinander

## 1. Ziel

MarketLens kann seine Kurse außer von Alpha Vantage auch von **Finnhub** beziehen (kostenloses Kontingent: 60 Anfragen
pro Minute statt 25 pro Tag). Beide Varianten laufen **gleichzeitig** als zwei getrennte Server (Port 8080 und 8081),
damit man sie in zwei Browser-Tabs vergleichen kann. Ihre Daten stören sich gegenseitig nicht.

## 2. Schritt 0: Voraussetzung prüfen (vor jeder Umsetzung)

**Risiko:** Nach Finnhubs eigener Dokumentation und mehreren Berichten ist der Endpunkt für historische Tageskurse
(`/stock/candle`, „Candles (OHLCV)“) **nur im bezahlten Tarif** enthalten; ein kostenloser Key erhält dann HTTP 403.
Eine Quelle von 2026 behauptet das Gegenteil, ist aber in sich widersprüchlich. Außerdem sind Kurse außerhalb der USA
bei Finnhub häufig kostenpflichtig. Ohne Tageskurse (Open, High, Low, Close, Volumen) kann MarketLens keine Indikatoren
berechnen; dann bringt Finnhub hier nichts.

**Prüfung mit dem eigenen Key** (im WSL-Terminal, 2 von 60 Anfragen):

    export FINNHUB_API_KEY=...
    # USA (Apple), 2026-06-01 bis 2026-11-13 als Unix-Zeit
    curl -s "https://finnhub.io/api/v1/stock/candle?symbol=AAPL&resolution=D&from=1780272000&to=1794528000&token=$FINNHUB_API_KEY" | head -c 300; echo
    # Deutschland (Mercedes-Benz, Xetra)
    curl -s "https://finnhub.io/api/v1/stock/candle?symbol=MBG.DE&resolution=D&from=1780272000&to=1794528000&token=$FINNHUB_API_KEY" | head -c 300; echo

| Antwort | Bedeutung | Folge |
|---|---|---|
| `{"c":[…],"h":[…],"l":[…],"o":[…],"s":"ok","t":[…],"v":[…]}` | Tageskurse im kostenlosen Tarif | F3 wie beschrieben umsetzen |
| `{"error":"You don't have access to this resource."}` (HTTP 403) | nur im bezahlten Tarif | F3 mit Finnhub **nicht** sinnvoll; anderen Anbieter wählen (Abschnitt 9) |
| AAPL `ok`, MBG.DE 403 bzw. `no_data` | nur USA kostenlos | F3 nur für US-Wertpapiere; die übrigen bleiben bei Alpha Vantage |
| `{"s":"no_data"}` | Symbol oder Zeitraum ohne Daten | Symbol bzw. Zeitraum prüfen |

**Erst wenn dieser Schritt erfolgreich ist, geht es mit Abschnitt 3 weiter.**

## 3. Umfang

**Gehört dazu:**
- neue Kursquelle `FinnhubQuelle : IPriceSource` (gleiches Muster wie `AlphaVantageQuelle`): API-Key aus
  `FINNHUB_API_KEY` vor der ersten Anfrage, URL, Fehlerbehandlung wie E2, Umwandlung in validierte Kerzen (E1),
  Bewertung für den Festplatten-Cache (E7)
- Programmoption `--quelle alphavantage|finnhub` (Standard: `alphavantage`, also unverändertes Verhalten)
- getrennte Ablage je Anbieter (Abschnitt 5)
- je Wertpapier ein Symbol pro Anbieter im Katalog (Abschnitt 6)
- zwei Server nebeneinander (Abschnitt 7)
- Testdaten (aufgezeichnete Antworten) und Tests wie bei Alpha Vantage

**Voraussetzung, aber eigener Schritt:** der **Live-HTTPS-Client** hinter der CMake-Option (cpp-httplib + OpenSSL,
`libssl-dev` im `Containerfile`). Ohne ihn laufen beide Server offline aus den aufgezeichneten Dateien; dann spielt
das Kontingent (25/Tag oder 60/Minute) noch keine Rolle. Er ist für beide Anbieter derselbe und wird einmal gebaut.

**Gehört nicht dazu:** Zusammenführen der Daten beider Anbieter, automatische Wahl des Anbieters, Umrechnung von Währungen.

## 4. Finnhub-Schnittstelle (laut Dokumentation, in Schritt 0 zu bestätigen)

    GET https://finnhub.io/api/v1/stock/candle?symbol=<SYMBOL>&resolution=D&from=<Unix-Sekunden>&to=<Unix-Sekunden>&token=<KEY>

| Teil der Antwort | Bedeutung | Abbildung in MarketLens |
|---|---|---|
| `"s": "ok"` | Daten vorhanden | weiter |
| `"s": "no_data"` | keine Daten für Symbol/Zeitraum | `HttpFehler(404)` (wie unbekanntes Symbol, E2) |
| `t`, `o`, `h`, `l`, `c`, `v` | **getrennte Listen** gleicher Länge: Unix-Zeit, Open, High, Low, Close, Volumen | je Index eine `Kerze`; ungleiche Längen → `ParseFehler` |
| HTTP 401 / `"error"` | Key ungültig | `HttpFehler(401)` mit dem Text von Finnhub |
| HTTP 403 | im Tarif nicht enthalten | `HttpFehler(403)` mit verständlicher Meldung („nicht im kostenlosen Tarif“) |
| HTTP 429 | 60/Minute überschritten | `KontingentFehler` (E7 liefert dann alte Daten mit Hinweis) |

Unterschiede zu Alpha Vantage, die im Code zu beachten sind:
- **Spaltenweise statt zeilenweise:** sechs Listen statt eines Objekts je Tag.
- **Datum als Unix-Zeit** (Sekunden, UTC) statt Text `YYYY-MM-DD`; umzurechnen in den Kalendertag.
- **Zahlen als echte JSON-Zahlen** statt als Text.
- **Fehler mit HTTP-Status** statt „Fehler mit Status 200“ wie bei Alpha Vantage.
- **Zeitraum ist Pflicht** (`from`, `to`): Vorschlag letzte 150 Kalendertage, damit es etwa 100 Handelstage sind wie bei
  Alpha Vantage „compact“.

## 5. Getrennte Ablage (entschieden 2026-11-16)

| Was | Alpha Vantage | Finnhub |
|---|---|---|
| aufgezeichnete Kurse (offline) | `daten/alphavantage/<SYMBOL>.json` | `daten/finnhub/<SYMBOL>.json` |
| Festplatten-Cache (E7, live) | `cache/alphavantage/` | `cache/finnhub/` |
| API-Key | `ALPHAVANTAGE_API_KEY` | `FINNHUB_API_KEY` |
| Zeichnungen im Browser (F1) | Server auf Port 8080 | Server auf Port 8081 |
| **gemeinsam:** Zinsdaten FRED | `daten/zinsen/` (beide lesen nur) | `daten/zinsen/` |
| **gemeinsam:** Katalog | `config/katalog.json` | `config/katalog.json` |

- **Zeichnungen sind automatisch getrennt:** Der Browser legt `localStorage` je Adresse **einschließlich Port** an;
  `localhost:8080` und `localhost:8081` sehen sich nicht.
- **Umzug:** Die heutigen Dateien in `daten/` (ohne `zinsen/`) ziehen nach `daten/alphavantage/`. Tests und
  `daten/README.md` werden angepasst. Der Standardwert von `--daten` wird `daten/alphavantage`.
- Zwei Prozesse teilen sich nichts außer Dateien, die beide nur lesen (Katalog, FRED). Schreiben (Cache) tut jeder nur in
  seinen eigenen Ordner.

## 6. Symbole je Anbieter im Katalog

Dasselbe Wertpapier hat bei beiden Anbietern unterschiedliche Kürzel. Vorschlag: ein optionales Feld `symbole`;
fehlt es für einen Anbieter, gilt `symbol`.

```json
{ "symbol": "MBG.DEX", "name": "Mercedes-Benz Group AG", "land": "Deutschland", "waehrung": "EUR",
  "symbole": { "finnhub": "MBG.DE" } }
```

- Der Server zeigt weiterhin `symbol` (wie heute) und fragt beim Anbieter das passende Kürzel an.
- Finnhub-Kürzel außerhalb der USA (z. B. `.DE` Xetra, `.L` London, `.TO` Toronto, `.BO`/`.NS` Indien) sind **in Schritt 0
  bzw. mit einer Anfrage je Wertpapier zu prüfen**; ist ein Wertpapier bei Finnhub nicht verfügbar, liefert der
  Finnhub-Server dafür 502 mit verständlicher Meldung, die übrigen funktionieren (E2).
- Ältere Kataloge ohne `symbole` funktionieren unverändert.

## 7. Zwei Server nebeneinander

    # Terminal 1: Alpha Vantage
    podman run --rm -it -p 8080:8080 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev \
      ./build/projekt --quelle alphavantage --daten daten/alphavantage --cache cache/alphavantage --port 8080 --host 0.0.0.0
    # Terminal 2: Finnhub
    podman run --rm -it -p 8081:8081 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev \
      ./build/projekt --quelle finnhub --daten daten/finnhub --cache cache/finnhub --port 8081 --host 0.0.0.0

- Browser: Tab 1 `http://localhost:8080/`, Tab 2 `http://localhost:8081/`.
- Die Seite zeigt im Titel bzw. in der Kopfzeile, **welcher Anbieter** dahintersteht (neues Feld `anbieter` in den
  Antworten von `/api/wertpapiere` und `/api/kurse`, z. B. „Kurse: Finnhub“), damit man die Tabs nicht verwechselt.
- Belegter Port wird erkannt (E9): Beide auf denselben Port zu starten ist nicht möglich.

## 8. Betroffene Dateien

| Datei | Änderung |
|---|---|
| `src/finnhub_quelle.hpp` / `.cpp` | **neu:** `FinnhubQuelle` |
| `src/katalog.*`, `src/kursdaten.hpp` | optionales Feld `symbole` je Wertpapier |
| `src/alphavantage_quelle.cpp`, `src/dateiquelle.cpp` | Anbieter-Kürzel statt `symbol` anfragen |
| `src/main.cpp` | Optionen `--quelle`, `--cache`; Standard `--daten daten/alphavantage` |
| `src/anfragebehandlung.cpp` | Feld `anbieter` in den Antworten |
| `web/app.js`, `web/index.html` | Anbieter in der Kopfzeile anzeigen |
| `daten/` | Umzug nach `daten/alphavantage/`, neu `daten/finnhub/` |
| `tests/fixtures/finnhub/`, `tests/test_finnhub.cpp` | **neu:** aufgezeichnete und handgeschriebene Antworten, Tests |
| Tests mit `DATEN_DIR` | auf `daten/alphavantage` umstellen |
| `docs/F3_…`, `docs/testprotokoll.md` | Erklärung und manuelle Testfälle |

Die Analyse (E3–E6), der Ergebnis-Cache (E9) und die Zeichenwerkzeuge (F1) bleiben unverändert: Sie kennen nur `IPriceSource`.

## 9. Offene Entscheidungen

| Nr. | Frage | Empfehlung |
|---|---|---|
| 0 | Ergebnis von Schritt 0 | **zuerst prüfen**; bei 403 nicht mit Finnhub weitermachen |
| 1 | Falls Finnhub die Tageskurse nicht kostenlos liefert: anderer zweiter Anbieter? | Kandidaten mit kostenlosen Tageskursen zuerst ebenso mit einer Testanfrage prüfen (z. B. Twelve Data, Tiingo, EODHD; Kontingente und Länder siehe `docs/E2_Optionen.html`) |
| 2 | Live-HTTPS-Client vorher bauen? | Ja, sonst bringt das größere Kontingent nichts; erfordert `libssl-dev` im `Containerfile` und `./dev.sh build` |
| 3 | Feld `symbole` im Katalog | wie in Abschnitt 6 |
| 4 | Zeitraum der Finnhub-Anfrage | letzte 150 Kalendertage (≈ 100 Handelstage) |
| 5 | Anbieter in der Oberfläche anzeigen? | Ja, in der Kopfzeile |
| 6 | Arbeit auf einem Git-Zweig? | Ja (`git switch -c zweiter-anbieter`), `main` bleibt bis zum Abschluss unverändert |

## 10. Prüfung

- **Automatisch:** `FinnhubQuelle` mit `FakeHttpClient` und Testdaten in `tests/fixtures/finnhub/`: gültige Antwort
  (spaltenweise, Unix-Zeit), `no_data`, 401, 403, 429, ungleich lange Listen, fehlende Liste, ungültige Kerze (E1),
  fehlender Key vor der ersten Anfrage, aufgezeichnete echte Antwort; `symbole` im Katalog (vorhanden, fehlend,
  ungültig); getrennte Ordner (zwei `DateiQuelle` sehen nur ihre eigenen Dateien); `anbieter` in den API-Antworten.
  Alle bestehenden Tests bleiben grün.
- **Manuell (neue Fälle im Testprotokoll):** beide Server gleichzeitig starten; zwei Tabs zeigen ihren Anbieter;
  gleiche Indikatoren-Logik auf beiden Seiten; Zeichnungen in Tab 1 erscheinen nicht in Tab 2; ein Server beendet
  (Strg+C), der andere läuft weiter; zweiter Start auf demselben Port wird abgelehnt.
