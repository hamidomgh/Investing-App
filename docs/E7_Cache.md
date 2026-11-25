# E7 – Disk cache for API responses: what was built and why

This file explains the E7 implementation, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> Responses of the web APIs are stored locally and reused on the next start as long as they haven't
> expired; expired entries are fetched again. A damaged or incomplete cache file is detected and ignored
> (refetch instead of crash), and date and expiry are testable through a replaceable clock. When the quota
> is exhausted or the network is down, existing data (even expired) is delivered with a corresponding notice.

| DoD point | Where |
|---|---|
| store locally, reuse on next start | `CachingHttpClient` writes one file per URL; test `UeberlebtNeustart` |
| expired → fetch again | `istAktuell`; test `AbgelaufenerEintragWirdNeuAbgerufen` |
| damaged/incomplete file detected and ignored, no crash | `lese` checks every part; test `BeschaedigteDateienWerdenIgnoriert` (6 kinds of damage) |
| replaceable clock | `IUhr`, `SystemUhr`, `FakeUhr` |
| quota/network failure → old data with notice | `HttpAntwort::veraltet` + `hinweis`; tests for network, timeout, 429 and Alpha Vantage's 200-quota |

## 2. Files

| File | Purpose |
|---|---|
| `src/basis/uhr.hpp` | interface `IUhr`, real clock `SystemUhr` |
| `src/http/http_cache.hpp` / `.cpp` | `CachingHttpClient`, `Antwortbewertung`, `standardBewertung` |
| `src/http/http_client.hpp` | **changed:** `HttpAntwort` got `veraltet`, `hinweis` and two constructors |
| `src/quellen/alphavantage_quelle.hpp` / `.cpp` | **changed:** new static function `bewerte` |
| `tests/fake_uhr.hpp` | `FakeUhr`: a clock the test sets and moves forward |
| `tests/test_http_cache.cpp` | 18 tests |
| `CMakeLists.txt` | new `.cpp` and test file |

## 3. Design: a decorator around `IHttpClient`

```
AlphaVantageQuelle ──► CachingHttpClient ──► real HTTPS client (or FakeHttpClient in tests)
        (IPriceSource)       (IHttpClient)            (IHttpClient)
```

`CachingHttpClient` **is** an `IHttpClient` and **has** an `IHttpClient` (the inner one). That is the
**decorator pattern**: it adds behaviour (caching) without changing the class it wraps or the classes that
use it. `AlphaVantageQuelle` doesn't know whether it talks to the cache or directly to the network.

Usage (later in `main`):
```cpp
SystemUhr uhr;
HttpsClient netz;                                          // comes with the live CMake option
CachingHttpClient cache(netz, uhr, "cache", std::chrono::hours{24}, AlphaVantageQuelle::bewerte);
AlphaVantageQuelle quelle(cache);
```

Why at the HTTP level and not at the `IPriceSource` level: the DoD says "Antworten der Web-APIs werden
gespeichert". Storing the raw response also means the cache works the same way for the rate source (E6/FRED)
without any new code.

## 4. What `get(url)` does

```
read the file for this URL ──► valid and not expired? ──yes──► return it (no request)
                                       │ no
                                       ▼
                           ask the inner client
            ┌──────────────────────┼──────────────────────────┐
     NetzwerkFehler          answer comes back                 │
     (incl. timeout)               │                           │
            │            bewerter_(answer) says:               │
            │     Verwertbar │ KontingentErschoepft │ Unbrauchbar
            │        save +  │                      │  return unchanged,
            │        return  │                      │  don't save
            ▼                ▼                      
   old entry exists?  ──yes──► return the OLD entry, veraltet = true, hinweis = "Stale data from …"
            │ no
            ▼
   network error: rethrow  /  quota: return the answer unchanged (the source then reports the quota error)
```

### 4.1 Why a "Bewerter" (rating function)?
A plain cache would store every HTTP 200. But **Alpha Vantage reports an exhausted quota with HTTP 200**
and a `"Note"`. A plain cache would store that note as if it were price data and serve it for 24 hours.

So the cache asks a function `Antwortbewertung bewerter(const HttpAntwort&)`:
- `standardBewertung` (default): 2xx → `Verwertbar`, 429 → `KontingentErschoepft`, else `Unbrauchbar`.
- `AlphaVantageQuelle::bewerte`: 429 or a quota note in the JSON → `KontingentErschoepft`; parses into a
  `Kursreihe` → `Verwertbar`; anything else (`"Error Message"`, broken JSON, 404 …) → `Unbrauchbar`.

The function is a `std::function`, so any callable works (a free function, a static member function, a lambda).
The cache stays generic; only the provider knows what "usable" means for its own answers.

### 4.2 Why `Unbrauchbar` never returns old data
A 404 means "this symbol doesn't exist". Delivering old data for it would hide a real problem. The DoD only
asks for the fallback on **quota** and **network** failures.

## 5. The cache file

One file per URL in the cache directory. **Name = 64-bit FNV-1a hash of the URL** as 16 hex digits, plus `.cache`.

```
MARKETLENS-CACHE 1                  identification + format version
url 3f2a9c0d5e7b1a46                hash of the URL (proves the file belongs to this URL)
abgerufen 1794571200                time of the request, seconds since 1970 (UTC)
status 200
laenge 21300                        number of bytes of the body
pruefsumme 9b0e44d1c2a73f58         hash of the body (detects changed bytes)
<body, exactly 'laenge' bytes>
```

### 5.1 The API key never reaches the disk
The URL contains `apikey=…`. That's why neither the file name nor the content contains the URL, only its
**hash**. A hash can't be turned back into the URL. The test `KeinApiKeyAufDerPlatte` checks names and contents.

### 5.2 Why FNV-1a and not `std::hash`?
`std::hash<std::string>` is allowed to give different results on different compilers or versions. The cache
must find its files again "on the next start", possibly after a compiler update. FNV-1a is a few lines of code
and gives the same result everywhere. (It isn't a cryptographic hash, but that isn't needed here: the goal is
stable names and detecting accidental damage, not protecting against attackers.)

### 5.3 How damage is detected (`lese`)
Each check that fails means "no entry" (`std::nullopt`), which leads to a normal refetch:

| Damage | Detected by |
|---|---|
| empty file, wrong format | first line isn't `MARKETLENS-CACHE 1` |
| header cut off | a header line is missing |
| header values broken | `alsZahl` fails (whole text must be a number) |
| file belongs to another URL | `url` hash doesn't match |
| body cut off (crash while writing, disk full) | fewer than `laenge` bytes |
| extra bytes appended | something after the body (`peek() != EOF`) |
| bytes changed | `pruefsumme` doesn't match the body |

**No exception, no crash.** After the refetch, the file is overwritten with a valid one. The test checks this
for six kinds of damage, and that the file is byte-identical to a good one afterwards.

### 5.4 Writing is "atomic" (`schreibe`)
1. Write everything to `<name>.cache.tmp`.
2. `std::filesystem::rename(tmp, real)`. On Linux, renaming within one directory replaces the old file in one step.

If the program crashes in step 1, the real file is still the old, complete one. Only the `.tmp` is broken, and
it's never read. If writing fails (e.g. disk full), the answer is **still returned**: a cache problem must not
break a successful request.

## 6. Expiry and the clock

- `istAktuell`: `age = jetzt − abgerufen`. Fresh if `age < gueltigkeit`. **Exactly at** `gueltigkeit` it's
  expired (test checks 23:59 vs 24:00).
- A timestamp **in the future** (the clock was set back) is treated as expired. Otherwise a wrong clock could
  keep an entry "fresh" for a very long time.
- `gueltigkeit = 0` means "always fetch", but the stored answer is still the reserve for a network failure.
- Negative validity → `std::invalid_argument`.

**`IUhr`** has one function, `jetzt()`. `SystemUhr` asks the real clock. `FakeUhr` (tests only) returns a time the
test sets and moves with `vorstellen(48h)`. So "two days later" is tested in milliseconds, and every run gives
the same result. All tests start at a fixed time: 2026-11-13 12:00 UTC.

## 7. The notice (`veraltet`, `hinweis`)

`HttpAntwort` got two fields:
- `veraltet = true` when old data is returned because a new request wasn't possible
- `hinweis`, e.g. `Stale data from 2026-11-13 12:00 UTC (network error: …)` or `(… request quota exhausted)`

The time is formatted with C++20 `std::format("{:%Y-%m-%d %H:%M} UTC", …)`.

**Side change:** with the two new fields, every short `HttpAntwort{200, "…"}` caused
`-Wmissing-field-initializers` warnings (on `-Wextra`). I added two constructors, `HttpAntwort()` and
`HttpAntwort(int status, std::string body)`. All existing code compiles unchanged and the warnings are gone.
`status` now also defaults to 0, so an empty `HttpAntwort` never has an undefined status.

**Open point:** `AlphaVantageQuelle::ladeKurse` returns a `Kursreihe`, so the notice stops there. To show it
in the web UI, the server (E8) needs it. The natural fix is to let `ladeKurse` return the price series
together with `veraltet`/`hinweis`. That changes `IPriceSource`, so I'd do it together with E8, when it's
clear what the server needs.

## 8. Tests (`tests/test_http_cache.cpp`)

The fixture class `CacheTest` creates its **own empty folder** in the system temp directory for each test
(name = process ID + test name) and deletes it afterwards. Tests can't see each other's files.

| Test | Checks |
|---|---|
| `TrefferOhneNeueAnfrage` | second call within 24 h: old body, not stale, still 1 request |
| `UeberlebtNeustart` | a **new** cache object on the same folder finds the file |
| `AbgelaufenerEintragWirdNeuAbgerufen` | 23:59 fresh, 24:00 expired → refetch and replace |
| `ZeitpunktInDerZukunftGiltAlsAbgelaufen` | clock set back → refetch |
| `GueltigkeitNull` | always fetch, but reserve on network failure |
| `NetzwerkfehlerLiefertAlteDatenMitHinweis` | 48 h later, network down: old data, `veraltet`, notice with date and reason |
| `ZeitueberschreitungLiefertAlteDaten` | same for a timeout |
| `KontingentLiefertAlteDatenMitHinweis` | HTTP 429: old data with "quota" notice; 429 not stored |
| `OhneEintragKeineReserve` | nothing cached: network error is thrown, 429 is returned unchanged |
| `UnbrauchbareAntwortWirdNichtGespeichert` | 404: returned unchanged, no file |
| `BeschaedigteDateienWerdenIgnoriert` | 6 kinds of damage: no crash, refetch, file repaired |
| `DateiEinerAnderenUrlWirdNichtVerwendet` | file copied from another URL is rejected |
| `BeschaedigterEintragIstKeineReserve` | damaged file + network down → error, not damaged data |
| `KeinApiKeyAufDerPlatte` | neither key nor URL in file names or contents |
| `UngueltigeKonfiguration` | negative validity; folder path is a file |
| `AlphaVantageKontingentMitStatus200` | with `AlphaVantageQuelle::bewerte`: 200 + `"Note"` isn't stored; old prices with notice |
| `AlphaVantageBewertung.AlleFaelle` | `bewerte` for valid, both quota forms, 429, Error Message, broken JSON, 500 |
| `StandardBewertung.NachStatus` | 2xx, 429, 404, 500 |

## 9. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 127 of 127 tests pass (18 new). A full
rebuild (`--clean-first`) shows **0 warnings**.

## 10. Possible exam questions

- *What is a decorator?* → A class that implements an interface and wraps another object of the same interface,
  adding behaviour. Here: caching around any `IHttpClient`.
- *Why can't the cache just store every 200?* → Alpha Vantage sends quota messages as 200; they'd be cached as data.
- *How is a damaged file detected?* → Fixed header with format version, URL hash, length and checksum of the
  body; any mismatch means "no entry".
- *Why write to a temp file first?* → `rename` replaces the file in one step, so a crash never leaves a half-written cache file.
- *Why is the key not in the cache?* → File names and contents only contain a hash of the URL.
- *Why FNV-1a and not `std::hash`?* → `std::hash` may differ between compilers/versions; the cache must find its
  files again after a restart.
- *How do you test "one day later"?* → With `FakeUhr`: the cache only asks `IUhr::jetzt()`, and the test moves that clock.
- *Why is an entry exactly at the validity limit expired?* → The rule is `age < validity`; a clear boundary that the test checks.
- *Why doesn't a 404 fall back to old data?* → It means the resource doesn't exist; old data would hide that.
