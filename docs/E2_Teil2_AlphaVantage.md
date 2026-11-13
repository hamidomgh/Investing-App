# E2, part 2 – JSON library and the Alpha Vantage price source

This file explains the second part of E2, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. What this part adds

| E2 requirement | Status after part 2 |
|---|---|
| Price data via `IPriceSource` from a web API, network via `IHttpClient` | **done** (`AlphaVantageQuelle`) |
| JSON answer → validated candles (E1) | **done** |
| Network error, timeout | **done** (from part 1, now tested through the source) |
| HTTP status 401, 404, other | **done** |
| Quota exhausted (429 **and** Alpha Vantage's 200-with-note) | **done** |
| Invalid JSON, missing fields | **done** (`ParseFehler`) |
| Missing API key reported **before the first request** | **done** (constructor reads the key; test checks 0 requests) |
| Catalog, a faulty entry doesn't stop the others | part 3 |
| Real HTTPS client (CMake option) | later |
| A recorded real answer as fixture | needs your API key (see section 7) |

## 2. Files

| File | Purpose |
|---|---|
| `third_party/nlohmann/json.hpp` | JSON library, version 3.11.3, unchanged |
| `third_party/README.md` | version, licence (MIT), source URL and SHA-256 of the header |
| `src/alphavantage_quelle.hpp` / `.cpp` | class `AlphaVantageQuelle : IPriceSource` |
| `tests/fixtures/alphavantage/*.json` | 11 hand-written answers (one valid, ten error cases) + `README.md` |
| `tests/test_alphavantage.cpp` | 13 tests |
| `CMakeLists.txt` | new `.cpp`, include path for `third_party/`, `FIXTURES_DIR`, new test file |

## 3. The library in the repo

- Downloaded once from the official release:
  `https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp`.
- The SHA-256 (`9bea4c80…103ea6`) matches the checksum published for that release. So the file is the
  original and unchanged. `third_party/README.md` records this; `sha256sum` can check it again.
- **Why in the repo and not downloaded by CMake:** the default build must work **offline**
  (`PROJEKT.md`). `FetchContent` would need the network during the build.

CMake:
```cmake
target_include_directories(projekt_lib SYSTEM PRIVATE third_party)
```
- `SYSTEM`: the compiler treats it as a system header and doesn't show warnings from **its** code.
  Our own code still gets all warnings (`-Wall -Wextra -Wpedantic`).
- `PRIVATE`: only the `.cpp` files of `projekt_lib` see it. `alphavantage_quelle.hpp` doesn't include
  `json.hpp`, so code using the class doesn't depend on the JSON library. That's a deliberate design
  choice: nlohmann stays an **implementation detail**.

## 4. `AlphaVantageQuelle`

### 4.1 Construction and the API key
```cpp
FakeHttpClient fake;                       // or the real HTTPS client later
AlphaVantageQuelle quelle(fake);           // reads ALPHAVANTAGE_API_KEY here
Kursreihe reihe = quelle.ladeKurse(wp);
```
- The constructor calls `leseApiSchluessel` from part 1. If the key is missing, the **constructor**
  throws `KonfigurationsFehler`. A source object without a key can't exist, so a request without a
  key is impossible. That's how "reported before the first request" is guaranteed.
- The client is stored as a **reference** (`IHttpClient&`). The source doesn't own it. Whoever creates
  the source must keep the client alive longer (in `main` both are local variables, so that's natural).
  A reference instead of a pointer also means: it can't be null.
- The variable name is a parameter (default `ALPHAVANTAGE_API_KEY`). Tests use their own variable, so
  a real key in your environment can't interfere.

### 4.2 The URL
```
https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=<SYMBOL>&outputsize=compact&apikey=<KEY>
```
- `compact` = the last ~100 trading days (free tier). Enough for all indicators.
- Symbol and key are **percent-encoded** (`urlKodieren`): letters, digits and `- _ . ~` stay, everything
  else becomes `%XX`. So `SAP.DEX` stays readable, but an index like `^GDAXI` becomes `%5EGDAXI`, and a
  `&` can't break the URL.
- `url()` is public, so tests can register the fake answer under exactly the URL the source will call.

### 4.3 `ladeKurse` = three steps
1. `client_.get(url)`: throws `NetzwerkFehler` / `ZeitueberschreitungFehler` (part 1).
2. `pruefeStatus(antwort, ...)`: throws for 401/403/404/429/other (part 1).
3. `parse(body, symbol)`: see below.

### 4.4 `parse` – from JSON text to a `Kursreihe`
`parse` is `static`: it needs neither client nor key, so it can be tested on its own with a string.

Alpha Vantage answers like this (newest day first, **numbers as text**):
```json
{
  "Meta Data": { ... },
  "Time Series (Daily)": {
    "2024-01-05": { "1. open": "162.0000", "2. high": "163.5000", "3. low": "161.2500",
                    "4. close": "162.7500", "5. volume": "3912345" },
    ...
  }
}
```

Order of checks:

| Step | Problem | Result |
|---|---|---|
| `json::parse` | not valid JSON | `ParseFehler` (the nlohmann `parse_error` is translated) |
| top level | not an object (e.g. an array) | `ParseFehler` |
| `"Error Message"` present | unknown symbol / invalid call | `HttpFehler(404, …)` |
| `"Note"` present | quota exhausted (older form) | `KontingentFehler` |
| `"Information"` with "rate limit", "call frequency" or "requests per day" | quota exhausted (newer form) | `KontingentFehler` |
| other `"Information"` | e.g. premium endpoint | `QuellenFehler` with the provider's text |
| `"Time Series (Daily)"` missing | unusable answer | `ParseFehler` |
| per day: date | not `YYYY-MM-DD`, or a day that doesn't exist (30 Feb) | `ParseFehler` |
| per day: each field | missing, wrong type, or text that isn't a number | `ParseFehler` naming date and field |
| per day: `Kerze` constructor (E1) | inconsistent (High < Low, …) | `ParseFehler` with the E1 message |

Then the candles are **sorted by date** and put into a `Kursreihe`, which checks the strict order again (E1).

**Design decisions:**
- **Errors are translated** into our own classes. Callers never see `nlohmann::json::parse_error` or
  `std::invalid_argument` from a source, only `QuellenFehler` subclasses. That's what lets the catalog
  (part 3) say "catch `QuellenFehler`, skip this security, continue".
- **"Error Message" is treated like HTTP 404.** Alpha Vantage sends unknown symbols with status 200,
  but for the caller it means the same as 404. So code checking `status() == 404` works for every provider.
- **Two forms of the quota message.** Alpha Vantage used `"Note"` and now mostly `"Information"`. The
  "Information" key is also used for non-quota notes, so the text is checked.
- **Numbers via `std::from_chars`** instead of `std::stod`: no exceptions to catch, and it ignores the
  system locale. With a German locale, `stod("160.5")` could stop at the dot, because German uses a comma.
  The check `ptr == end` makes sure the whole text is a number (`"12abc"` is rejected).
- **Real JSON numbers are accepted too** (`10` instead of `"10.0000"`), in case the format changes.
- **Not relying on JSON order:** nlohmann sorts object keys alphabetically, and for ISO dates that happens
  to be chronological. The explicit `std::sort` makes the code correct regardless.
- **Empty time series → empty `Kursreihe`**, not an error. The indicators then report "series too short".

### 4.5 C++ details you may be asked about
- `if (const auto it = dokument.find("Note"); it != dokument.end())` is an **if with initializer**
  (C++17): `it` exists only inside the `if`.
- `for (const auto& [datumText, tag] : reihe->items())` uses **structured bindings** (C++17) to get key
  and value of each JSON entry.
- `kerzen.emplace_back(datum, open, …)` builds the `Kerze` directly inside the vector. `datum` is a
  `sys_days`, and it converts implicitly to `system_clock::time_point` (a finer unit, so no information is lost).
- `R"json( … )json"` in the tests is a **raw string literal** with a custom delimiter. The text contains
  `(Daily)"`, and with the plain `R"( … )"` form, that `)"` would end the string early. That's exactly the
  compile error that happened on the first build.

## 5. Fixtures and how tests find them

`CMakeLists.txt` passes the path as a constant:
```cmake
target_compile_definitions(tests PRIVATE FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
```
The helper `fixture("daily_ok.json")` reads `FIXTURES_DIR/alphavantage/daily_ok.json`. If a file is
missing, it throws instead of returning an empty text, so a typo shows up as a clear test failure.

The 11 files are listed in `tests/fixtures/alphavantage/README.md`. They are **hand-written** in the
documented Alpha Vantage format.

## 6. Tests (`tests/test_alphavantage.cpp`)

The tests use a **test fixture class** `AlphaVantageTest` (`TEST_F`). `SetUp()` sets a test key before
every test, and `TearDown()` removes it afterwards, so no test depends on another.

| Test | Checks |
|---|---|
| `FehlenderKeyVorDerErstenAnfrage` | no key → constructor throws, `fake.anfragen()` is empty |
| `UrlEnthaeltSymbolUndKey` | exact URL; `SAP.DEX` unchanged; `^A&B` → `%5EA%26B` |
| `GueltigeAntwortWirdZuKursreihe` | 3 candles, ascending although the file is newest-first, all 5 fields, 1 request |
| `NetzwerkfehlerUndZeitueberschreitung` | both reach the caller |
| `HttpFehlerstatus` | 401 → `HttpFehler`, 429 → `KontingentFehler`, 500 → `HttpFehler` |
| `ErrorMessageWieNichtGefunden` | `"Error Message"` → `HttpFehler` with status 404, message names the symbol |
| `KontingentHinweisMitStatus200` | `"Note"` and `"Information"` (rate limit) → `KontingentFehler` |
| `SonstigerHinweis` | premium note → `QuellenFehler`, but **not** `KontingentFehler` |
| `UngueltigeAntwortenWerdenAlsParseFehlerGemeldet` | truncated JSON, array, empty text, missing series, `"n/a"`, 30 Feb |
| `FehlendesFeldNenntDatumUndFeld` | message contains `2024-01-05` and `4. close` |
| `InkonsistenteKerzeWirdAbgelehnt` | High < Low → `ParseFehler` with the date |
| `ZahlenOhneAnfuehrungszeichenUndLeereReihe` | JSON numbers work; empty series → empty `Kursreihe` |
| `AlleFehlerSindQuellenFehler` | any source error can be caught as `QuellenFehler` |

## 7. Still open: a recorded real answer

`PROJEKT.md` says the tests use **recorded** provider answers. That needs a real request with your key,
which I can't do because I don't have your key. Steps, in your WSL terminal:

```bash
export ALPHAVANTAGE_API_KEY=...        # your key, never commit it
curl "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=IBM&outputsize=compact&apikey=$ALPHAVANTAGE_API_KEY" \
     -o tests/fixtures/alphavantage/aufgezeichnet_IBM.json
```

The key is only in the URL, not in the answer. Check the file with `grep -i apikey` anyway before
committing. Then I can add a test that parses it, which proves the parser works on real data and not
only on my hand-written format.

## 8. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 100 of 100 tests pass (13 new),
no compiler warnings.

## 9. Possible exam questions

- *Why is the JSON library in the repo?* → The default build must work offline, without installing packages.
- *Why `SYSTEM PRIVATE` in CMake?* → No warnings from foreign code; and the JSON library stays an
  implementation detail of `projekt_lib`.
- *How do you guarantee "missing key before the first request"?* → The constructor reads the key and
  throws; without a constructed object there can't be a request. The test checks 0 requests.
- *Why translate `nlohmann::json::parse_error` into `ParseFehler`?* → Callers only need to know our
  error classes; the JSON library could be replaced without changing them.
- *Why treat "Error Message" like 404?* → Same meaning for the caller, independent of the provider.
- *Why `from_chars` and not `stod`?* → No exceptions, independent of the locale, and it can check that
  the whole text was a number.
- *Why sort although the keys come sorted?* → The sort order of nlohmann objects is an implementation
  detail; the code shouldn't depend on it.
- *Why a reference to the client and not a copy?* → `IHttpClient` is an abstract interface; you can't
  copy it, and the fake must be the same object the test inspects.
