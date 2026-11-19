# F5 – Live-Zugriff auf beide Anbieter (Alpha Vantage, Twelve Data)

Bestätigt im Gespräch am 2026-11-18 (Option „a“, `libssl-dev` freigegeben, eigener Zweig `live-zugriff`).
Nummer F5, weil F4 (Verwaltungsseite) parallel auf dem Zweig `verwaltungsseite` entsteht.

## 1. Ziel

Beide Server (`--quelle alphavantage`, `--quelle twelvedata`) holen neue Kurse selbstständig aus dem Netz, statt nur
aufgezeichnete Dateien aus `daten/` zu lesen. Der Laptop muss dafür nicht ständig laufen.

## 2. Umfang

Dazu gehört:
- echter HTTP(S)-Client `LiveHttpClient : IHttpClient` (cpp-httplib + OpenSSL) – **Teil 1**
- CMake-Option `MARKETLENS_LIVE_HTTP` (Standard AUS), `libssl-dev` im `Containerfile` – **Teil 1**
- Verdrahtung in `main`: Option `--live`, `--cache`, `--gueltigkeit` – **Teil 2**
- Fall im Testprotokoll für den echten Abruf – **Teil 2**

Gehört nicht dazu: Hintergrund-Timer (Option „b“), Kurse innerhalb des Handelstags, Zusammenführen beider Anbieter.

## 3. Verhalten (Option „a“: Abruf bei Bedarf)

- Kette: `AlphaVantageQuelle`/`TwelveDataQuelle` → `CachingHttpClient` (E7) → `LiveHttpClient` → Internet.
- Ist der Eintrag im Cache jünger als die Gültigkeit, wird nichts abgerufen. Sonst ein Abruf je Wertpapier.
- Jeder Abruf liefert die letzten 100 Handelstage. Nach einer Pause (Laptop aus) fehlen deshalb keine Tage;
  es braucht keine Nachhol-Logik.
- Netzfehler oder erschöpftes Kontingent: alte Daten aus dem Cache mit Hinweis (gibt es schon seit E7).
- Ohne `--live` bleibt alles wie bisher (offline aus `daten/`).

## 4. Betroffene Dateien

| Datei | Teil |
|---|---|
| `src/live_http_client.hpp/.cpp` (neu), `tests/test_live_http_client.cpp` (neu) | 1 |
| `CMakeLists.txt`, `container/Containerfile` | 1 |
| `src/live_kursquelle.hpp/.cpp` (neu), `tests/test_live_kursquelle.cpp` (neu) | 2 |
| `src/main.cpp`, `.gitignore` (`cache/`), `docs/testprotokoll.md` | 2 |

## 5. Entscheidungen

| Nr. | Frage | Entscheidung |
|---|---|---|
| 1 | Abruf bei Bedarf oder Timer? | **Bei Bedarf** (Option „a“): wenig Code, wenige Abrufe, passt zum Laptop |
| 2 | Neue Abhängigkeit OpenSSL? | **Ja**, aber nur mit `MARKETLENS_LIVE_HTTP=ON`; der Standardbuild bleibt netzfrei |
| 3 | Wie testen ohne Internet? | Der Client kann auch `http://`; die Tests sprechen einen **lokalen Testserver** an |
| 4 | Gültigkeit | Alpha Vantage **12 h** (≈ 25 Abrufe/Tag), Twelve Data **1 h**; änderbar mit `--gueltigkeit` (1–168 h) |
| 5 | Unpassende Optionen | Werden abgelehnt: `--daten` nur offline, `--cache`/`--gueltigkeit` nur mit `--live` |
| 6 | `--live` ohne OpenSSL | Abbruch schon beim Start mit Hinweis auf die CMake-Option |

## 6. Prüfung

- **Automatisch (Teil 1):** 8 Tests `LiveHttpClient*` gegen einen lokalen Testserver: Zerlegung der URL, gültige
  Antwort, Fehlerstatus wird zurückgegeben, keine Verbindung, Zeitüberschreitung, HTTPS ohne OpenSSL, Kette mit Cache.
- **Automatisch (Teil 2):** 7 Tests `LiveKursquelle*` mit `FakeHttpClient` und `FakeUhr`: Gültigkeit je Anbieter,
  unbekannter Anbieter, fehlender Key, Abruf erst nach Ablauf, Twelve-Data-Kette, Kontingent → alte Daten, ohne Netz.
- **Manuell (Teil 2):** Fälle F5-1 … F5-10 in `docs/testprotokoll.md` (echter Abruf, Neustart ohne neuen Abruf,
  Netz weg, fehlender Key, unpassende Optionen).
