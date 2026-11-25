# E2, part 3 – Catalog and loading many securities

This file explains the third part of E2, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. The two E2 requirements covered here

> A catalog (configuration file) describes symbol, country and currency of any number of securities;
> a new country can be added without changing code, and a faulty security doesn't prevent the
> evaluation of the others.

"Faulty security" can mean two different things, and both are handled:

| Where it fails | Example | Handled by |
|---|---|---|
| **in the catalog file** | missing field, empty name, duplicate symbol | `Katalog` skips the entry, records it in `fehler()` |
| **when loading prices** | unknown symbol, network down, quota, invalid JSON | `ladeAlleKurse` skips the security, records it in `Ladeergebnis::fehler` |

## 2. E2 status after part 3

| E2 Definition of Done | Status |
|---|---|
| Prices via `IPriceSource`, network via `IHttpClient` | done (part 1 + 2) |
| JSON → validated candles (E1) | done (part 2) |
| Network error / timeout, HTTP 401/404, quota 429, invalid JSON, missing fields | done (part 1 + 2) |
| Missing API key reported before the first request | done (part 2) |
| Catalog with any number of securities | **done** (part 3) |
| New country without code change | **done** (part 3) |
| A faulty security doesn't stop the others | **done** (part 3) |

That's the complete E2 Definition of Done. Still to do from the project description (not part of E2's DoD):
the **real HTTPS client** behind a CMake option (cpp-httplib + OpenSSL), and a manual test protocol for live access.

## 3. Files

| File | Purpose |
|---|---|
| `config/katalog.json` | the real catalog: 6 securities from 5 countries |
| `src/modell/katalog.hpp` / `.cpp` | class `Katalog`, struct `KatalogFehler` |
| `src/quellen/kursabruf.hpp` / `.cpp` | function `ladeAlleKurse`, structs `GeladenesWertpapier`, `Ladefehler`, `Ladeergebnis` |
| `tests/fixtures/katalog/gemischt.json` | test catalog with 3 valid and 5 faulty entries |
| `tests/test_katalog.cpp` | 8 tests |
| `CMakeLists.txt` | two new `.cpp` files, new test file, constant `KONFIG_DIR` |

## 4. The catalog file

```json
{
    "wertpapiere": [
        { "symbol": "IBM",     "name": "International Business Machines", "land": "USA",         "waehrung": "USD" },
        { "symbol": "MBG.DEX", "name": "Mercedes-Benz Group AG",          "land": "Deutschland", "waehrung": "EUR" },
        ...
    ]
}
```

- `config/katalog.json` contains IBM and Apple (USA), Mercedes-Benz (Deutschland), Tesco (Großbritannien),
  Shopify (Kanada) and Reliance (Indien). The symbols use Alpha Vantage's exchange suffixes
  (`.DEX` Xetra, `.LON` London, `.TRT` Toronto, `.BSE` Bombay). Whether each one returns data on the free tier
  can only be checked in live mode. If one doesn't, it shows up as a `Ladefehler` and the others still work.
- **`symbol` is the provider's symbol.** I decided not to add a separate "display symbol" for now, because
  nothing needs it yet. It can be added later as an optional field without breaking existing catalogs.
- The top level is an **object** with a list, not a bare list. That leaves room for more sections later,
  e.g. `"zentralbanken"` with their countries (the PLAN.md decision for E6).
- **Why `config/` and not `tests/`:** this is the catalog the program will really use. The test
  `EchterKatalogIstFehlerfrei` makes sure it stays valid.

## 5. `Katalog`

### 5.1 Creating a catalog
```cpp
Katalog k = Katalog::ausDatei("config/katalog.json");
for (const Wertpapier& wp : k.wertpapiere()) { ... }
for (const KatalogFehler& f : k.fehler())    { /* show: position, symbol, message */ }
```
- **Static factory functions** (`ausJson`, `ausDatei`) instead of a public constructor. The constructor is
  `private`, so a `Katalog` can only come from parsing. That matches "named constructors": the name says
  where the data comes from.
- `ausDatei` only reads the file and passes the text to `ausJson`, so all the logic is tested with strings.
  A missing file → `KonfigurationsFehler` with the path.

### 5.2 Two levels of errors
| Problem | Result |
|---|---|
| not valid JSON, top level isn't an object, no `"wertpapiere"` array | `ParseFehler`: **whole catalog** rejected. Without the list there is nothing to evaluate |
| one entry is bad | entry skipped, `KatalogFehler{position, symbol, meldung}` recorded, **the rest continues** |

Checks per entry, in order:
1. must be a JSON object
2. `symbol`, `name`, `land`, `waehrung` must exist and be strings (`leseText`)
3. `Wertpapier` constructor (E1) rejects empty fields: the E1 validation is reused, not rewritten
4. a symbol already seen → `"duplicate symbol"`. The **first** one wins, so adding a duplicate at the end
   can't silently replace an existing entry

### 5.3 How "skip and continue" works
Inside the loop, each entry has its own `try`/`catch`:
- `EintragFehler` is a small **internal** struct used as an exception inside `katalog.cpp` only (anonymous
  namespace). It never leaves the file: it's caught in the same loop and turned into a `KatalogFehler`.
- `std::invalid_argument` comes from the `Wertpapier` constructor and is turned into a `KatalogFehler` too.

The symbol is stored in the error if it could be read, so the message can say *which* entry is wrong. Position
counts from 0, like the JSON array.

### 5.4 A new country without code changes
No code contains a list of countries or currencies. `land` and `waehrung` are just strings. The test
`NeuesLandOhneCodeaenderung` adds "Neuseeland/NZD" only in JSON, and `gemischt.json` adds Japan.

## 6. `ladeAlleKurse`

```cpp
Ladeergebnis e = ladeAlleKurse(quelle, katalog.wertpapiere());
// e.erfolgreich: {wertpapier, kurse} for each success
// e.fehler:      {symbol, meldung} for each failure
```
- Works with **any** `IPriceSource` (Alpha Vantage, a fake, a future provider).
- Each security has its own `try`/`catch (const QuellenFehler&)`. Because all source errors derive from
  `QuellenFehler` (part 1), **one** catch covers network, HTTP, quota and JSON problems.
- **Only `QuellenFehler` is caught.** A `std::logic_error` or `std::bad_alloc` is a bug or a real emergency,
  not "this security has no data". Catching everything (`catch (...)`) would hide bugs. The test
  `AndereFehlerWerdenNichtVerschluckt` checks exactly that.
- A free function, not a class: it has no state of its own. It only combines a source with a list.
- `std::move(kurse)` moves the price series into the result instead of copying it (the local variable isn't
  needed afterwards).
- When the quota is exhausted, the remaining securities are still tried. Each fails quickly, and later
  (with the E7 cache) they may still succeed from cached data.

## 7. Tests (`tests/test_katalog.cpp`)

| Test | Checks |
|---|---|
| `Katalog.EchterKatalogIstFehlerfrei` | `config/katalog.json`: 0 errors, 6 securities, MBG.DEX is Deutschland/EUR |
| `Katalog.FehlerhafteEintraegeVerhindernNichtDieUebrigen` | `gemischt.json`: 3 valid in file order, 5 errors with the right positions, symbols and messages; first IBM wins |
| `Katalog.NeuesLandOhneCodeaenderung` | Neuseeland/NZD only in JSON is accepted |
| `Katalog.UnbrauchbarerKatalog` | invalid JSON, no `wertpapiere`, not a list, top-level array → `ParseFehler`; missing file → `KonfigurationsFehler` |
| `Katalog.LeererKatalogUndUnbekanntesSymbol` | empty list is fine; `finde` returns `nullptr` |
| `Kursabruf.FehlerEinesWertpapiersVerhindertNichtDieUebrigen` | real catalog, 6 securities: 2 succeed, 4 fail (unknown symbol, network, quota, invalid JSON); all 6 were requested; order kept |
| `Kursabruf.AndereFehlerWerdenNichtVerschluckt` | a `std::logic_error` reaches the caller |
| `Kursabruf.LeereListe` | no securities → no request, empty result |

The last two tests define small `IPriceSource` classes **inside the test function** (local classes). That's
enough for a one-off test double and keeps them out of the shared code.

## 8. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 109 of 109 tests pass (8 new),
no compiler warnings.

## 9. Possible exam questions

- *How does a new country get added?* → One new JSON entry. No code has a list of countries.
- *What happens with a broken entry?* → It's skipped and reported with position, symbol and reason; the others load.
- *Why is a broken file rejected as a whole, but a broken entry not?* → Without a valid `wertpapiere` list
  there's nothing to continue with; a single bad entry still leaves usable data.
- *Why does the first duplicate win?* → Appending an entry shouldn't silently replace an existing one.
- *Why only `catch (const QuellenFehler&)` and not `catch (...)`?* → Other exceptions are bugs; hiding them would
  make errors invisible.
- *Why static factory functions and a private constructor?* → A catalog can only be created by parsing; the name
  (`ausJson`, `ausDatei`) says where it comes from.
- *Why is `ausDatei` so short?* → It only reads the file; the logic is in `ausJson`, which is easy to test with strings.
