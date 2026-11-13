# E8, part 1 – Request handling of the HTTP JSON interface (without network)

This file explains the first part of E8, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`) and what this part covers

> The server offers at least endpoints for the list of securities, the price data of a security, all
> indicators including support/resistance, and the event study. Parameters (symbol, period, periods) are
> checked; unknown symbols, invalid parameters and unknown paths return suitable HTTP status codes
> (404, 400) with a JSON error message. Request handling and computation are separable from the network
> and testable without a socket.

| DoD point | Status |
|---|---|
| 4 endpoints | **done** (`Anfragebehandlung`) |
| parameters checked | **done** (symbol, von/bis, all periods) |
| 404 unknown symbol / path, 400 invalid parameter, JSON error | **done** |
| testable without a socket | **done**: 15 tests, no network at all |
| real HTTP server | **part 2** (cpp-httplib, new `main.cpp`) |

## 2. Files

| File | Purpose |
|---|---|
| `src/anfrage.hpp` | `Anfrage` (method, path, parameters) and `Antwort` (status, content type, body) |
| `src/anfragebehandlung.hpp` / `.cpp` | class `Anfragebehandlung` with all endpoints |
| `src/price_source.hpp` | **changed:** new `Kursdaten` and `IPriceSource::ladeKursdaten` (see section 6) |
| `src/alphavantage_quelle.hpp` / `.cpp` | **changed:** overrides `ladeKursdaten`, passes the cache notice on |
| `tests/test_anfragebehandlung.cpp` | 15 tests |
| `CMakeLists.txt` | new `.cpp` and test file; tests may include `nlohmann/json` to read the answers |

## 3. The key idea: no network inside

```
     part 2                    part 1 (this file)
browser ──HTTP──► cpp-httplib ──► Anfrage ──► Anfragebehandlung::behandle ──► Antwort ──► cpp-httplib ──► browser
                  (translate)                       │                                     (translate)
                                                    ▼
                                    IPriceSource, Trendindikatoren, Risikokennzahlen,
                                    SupportResistance, Ereignisstudie
```

`Anfrage` and `Antwort` are plain structs that don't depend on any network library. The network layer
(part 2) only **translates**: HTTP request → `Anfrage`, `Antwort` → HTTP response. Everything else (which path,
which parameters, which status code, what JSON) is in `Anfragebehandlung` and can be tested by building an
`Anfrage` directly:

```cpp
Antwort a = api.behandle(Anfrage{"GET", "/api/kurse", {{"symbol", "IBM"}}});
EXPECT_EQ(a.status, 200);
```

That fulfils "separable from the network and testable without a socket". It also means the network library
could be replaced without touching any endpoint logic.

## 4. Endpoints

All endpoints are `GET` and return JSON.

### `GET /api/wertpapiere`
No parameters. `{"wertpapiere": [{"symbol","name","land","waehrung"}, …]}` in catalog order.

### `GET /api/kurse?symbol=IBM&von=2026-01-10&bis=2026-01-19`
`von`/`bis` are optional, format `YYYY-MM-DD`, both **inclusive**.
```json
{ "symbol": "IBM", "name": "…", "land": "USA", "waehrung": "USD",
  "veraltet": false, "hinweis": null,
  "kerzen": [ {"datum": "2026-01-10", "open": …, "high": …, "low": …, "close": …, "volumen": …}, … ] }
```

### `GET /api/indikatoren?symbol=IBM`
Optional parameters with defaults:

| Parameter | Default | Meaning |
|---|---|---|
| `sma`, `ema` | 20 | period |
| `rsi`, `atr` | 14 | period |
| `macd` | `12,26,9` | fast, slow, signal |
| `bollinger` | `20,2` | period, width k |
| `fenster` | 5 | window for local extrema (E5) |
| `toleranz` | 0.02 | relative tolerance for levels (E5) |
| `von`, `bis` | – | time range |

The answer contains the same header fields as `/api/kurse`, plus:
- `parameter`: the values actually used (including defaults), so the UI can show them
- `daten`: the date of every candle. All series below have **exactly one entry per date**, so index `i` of every
  series belongs to `daten[i]`.
- `sma`, `ema`, `rsi`, `atr`: arrays; the warm-up phase is `null` (the JSON form of `std::nullopt`)
- `macd`: `{linie, signal, histogramm}`; `bollinger`: `{mitte, oben, unten}`; `renditen`: `{einfach, log}`
- `volatilitaet` (number or `null` if too few candles), `maxDrawdown`
- `niveaus`: `[{"preis", "beruehrungen", "art": "support"|"resistance"}]` ascending
- `pivot`: `{p, r1, r2, s1, s2}` from the last candle

### `GET /api/ereignisstudie?symbol=IBM`
`{…header…, "ereignisse": [{"zentralbank","datum","leitzins","reaktionstag","kursVorher","kursNachher","veraenderung"}]}`.
No matching central bank → empty list with status 200 (DoD E6).

## 5. Status codes and errors

Every error is JSON: `{"fehler": "parameter 'sma' must be a positive integer, got '0'", "status": 400}`.

| Status | When |
|---|---|
| 200 | success |
| **400** | missing `symbol`, unknown parameter (typo like `smaa`), period ≤ 0 or not a whole number, MACD fast ≥ slow, wrong number of values in `macd`/`bollinger`, negative `k` or tolerance, invalid date, `von` after `bis` |
| **404** | unknown path, unknown symbol |
| 405 | method other than GET |
| 422 | parameters valid, but the data can't support them (e.g. SMA 100 on 60 days). The message comes from E3–E5 |
| 502 | the price provider failed: network, HTTP error, invalid JSON (`QuellenFehler`) |
| 503 | the provider's quota is exhausted and the cache has nothing (`KontingentFehler`) |
| 500 | unexpected internal error. The message is **not** passed on, so no internal details leak |

How it works: inside the file, every check throws a small internal `ApiFehler{status, meldung}`. The single
function `behandle` catches all exceptions **in one place** and turns each kind into the matching status:

```cpp
catch (const ApiFehler& f)          → f.status
catch (const KontingentFehler& f)   → 503
catch (const QuellenFehler& f)      → 502     // order matters: KontingentFehler is a QuellenFehler
catch (const std::invalid_argument&)→ 422
catch (const std::exception&)       → 500
```

The order of the `catch` blocks matters: the more specific class must come first, otherwise
`catch (const QuellenFehler&)` would also catch the quota error.

`behandle` **never throws**. Whatever happens, the caller (the server) gets an `Antwort`.

### Design decisions
- **Check everything before loading.** All parameters are validated first, and only then does the handler ask
  the source. An invalid request never uses up one of the 25 free Alpha Vantage requests. The tests check
  `quelle.aufrufe() == 0` after every invalid request. The same goes for unknown symbols: they're looked up in
  the catalog, not at the provider.
- **Unknown parameters are rejected.** `?smaa=50` would otherwise silently use the default 20, and the user
  would wonder why the line looks wrong.
- **422 vs 400:** 400 means "your request is wrong no matter what data exists" (period 0). 422 means "your request
  is fine, but there isn't enough data for it" (period 100 on 60 days). The UI can show different messages.
- **502/503:** the problem is "behind" our server, at the provider. 503 for the quota also tells the client
  "try again later".
- **Numbers with `std::from_chars`:** the whole text must be a number (`20abc` → 400), and it uses a dot as the
  decimal separator independent of the system language.

## 6. Passing the cache notice on (open point from E7)

E7 marks old data (`HttpAntwort::veraltet`, `hinweis`), but `IPriceSource::ladeKurse` only returned a
`Kursreihe`. Now:

```cpp
struct Kursdaten { Kursreihe reihe; bool veraltet = false; std::string hinweis; };

class IPriceSource {
    virtual Kursreihe ladeKurse(const Wertpapier&) = 0;
    virtual Kursdaten ladeKursdaten(const Wertpapier& wp) { return Kursdaten{ladeKurse(wp)}; }  // new
};
```

- `ladeKursdaten` is **virtual but not pure**: it has a default that calls `ladeKurse` and reports "fresh".
  Existing classes (the test fakes in E2) don't need to change, and **no existing test was modified**.
- `AlphaVantageQuelle` overrides it and copies `veraltet`/`hinweis` from the `HttpAntwort` of the cache.
  Its `ladeKurse` now just returns `ladeKursdaten(wp).reihe`, so there is only one code path.
- The API puts both into every answer (`"veraltet": true, "hinweis": "Stale data from …"`).

`Kursdaten` got a constructor for the same reason as `HttpAntwort` in E7: `Kursdaten{reihe}` otherwise causes
`-Wmissing-field-initializers` warnings.

## 7. Tests (`tests/test_anfragebehandlung.cpp`)

`FakeQuelle` is a test `IPriceSource`: per symbol it returns a fixed 60-day series (a sine wave plus a slight
upward trend, so there are highs and lows), stale data, or throws a quota, network or internal error. It counts
how often it was called.

| Test | Checks |
|---|---|
| `UnbekannterPfad` | `/api/gibtsnicht` and `/` → 404 JSON |
| `NurGetErlaubt` | POST → 405 |
| `InhaltstypIstJson` | content type for success and error |
| `Wertpapierliste` | 3 entries in order with all fields; nothing loaded; parameters rejected |
| `Kurse` | header, 60 candles, first/last date, prices identical to the series |
| `KurseImZeitraum` | `von`+`bis` inclusive (10 days), only `von`, range without data → empty list |
| `KurseParameterfehler` | missing symbol, month 13, wrong format, von > bis, unknown parameter; **0 loads** |
| `UnbekanntesSymbol` | 404 on all three endpoints; **0 loads** |
| `VeralteteDatenMitHinweis` | `veraltet: true` and the notice in `/api/kurse` and `/api/indikatoren` |
| `FehlerDerQuelle` | quota → 503, network → 502, internal → 500 without the internal text; other symbols still work |
| `IndikatorenMitStandardwerten` | defaults, every series has 60 entries, warm-up `null`, values **identical** to calling the E3–E5 classes directly, niveaus labelled |
| `IndikatorenMitEigenenPerioden` | `sma=5`, `macd=3,6,2`, `bollinger=10,1.5` are used |
| `IndikatorenParameterfehler` | 13 invalid parameters → 400 naming the parameter; **0 loads** |
| `IndikatorenZuWenigDaten` | SMA 100 on 60 days, MACD on 10 days → 422 |
| `Ereignisstudie` | 2 Fed events with date, reaction day and change; Japanese stock → empty list; `von` filters |

The test `IndikatorenMitStandardwerten` compares the API's values with direct calls to `Trendindikatoren` and
`SupportResistance`. The API must not calculate anything differently; it only packages the results.

## 8. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 142 of 142 tests pass (15 new).
A full rebuild shows 0 warnings.

## 9. Next: part 2

- add `third_party/httplib.h` (cpp-httplib, header-only)
- a thin `HttpServer` class that translates `httplib::Request` → `Anfrage` and `Antwort` → `httplib::Response`
- new `main.cpp`: read the catalog, set up cache and source, start the server on a port
- live HTTPS needs OpenSSL headers (`libssl-dev`), which aren't in the container. That's a change to the
  `Containerfile`, so I'll ask before doing it. Without it the server still runs and serves data from the cache.

## 10. Possible exam questions

- *How is the interface testable without a socket?* → `Anfrage`/`Antwort` are plain structs; tests call
  `behandle()` directly. The network layer only translates.
- *Why does the order of the `catch` blocks matter?* → `KontingentFehler` is a `QuellenFehler`; the more specific
  one must come first, or it would be caught as 502.
- *Why validate parameters before loading?* → An invalid request must not cost a request of the provider's quota.
- *Why reject unknown parameters?* → A typo would otherwise silently use a default value.
- *What's the difference between 400 and 422 here?* → 400: invalid regardless of data. 422: valid, but not
  computable with the available data.
- *Why isn't the internal message returned for a 500?* → It may contain internal details; the client only needs
  to know that something went wrong on the server.
- *Why is `ladeKursdaten` not pure virtual?* → So existing implementations keep working unchanged; only sources
  that know about staleness override it.
- *Why does every indicator array have the same length as `daten`?* → So the UI can draw all lines on the same x-axis by index.
