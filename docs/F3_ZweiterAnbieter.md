# F3 – Second provider (Twelve Data) and two servers side by side: what was built and why

This file explains feature F3 (offline part), so you can explain it in the exam. The specification (German) with all
decisions is `Features/F3_Zweiter_Anbieter.md`. The code comments are in German; this text is in English.
Built on the Git branch **`zweiter-anbieter`**; `main` is unchanged until you merge.

## 1. What it does

```
Terminal 1:  ./build/projekt --quelle alphavantage --port 8080   →  6 securities, "Kurse: Alpha Vantage"
Terminal 2:  ./build/projekt --quelle twelvedata   --port 8081   →  IBM and AAPL,  "Kurse: Twelve Data"
```
Two independent processes, each with its own provider, its own price files, its own threads and result cache, and (in the
browser) its own drawings. Only the catalog and the FRED rate data are shared, and both are only read.

**Offline first** (your decision): both servers read recorded answers from `daten/<provider>/`. The live HTTPS client for both
providers is the next, separate step.

## 2. Why Twelve Data, and "US only"

Checked with your free keys before writing any code (step 0):

| Provider | Request | Result |
|---|---|---|
| Finnhub | daily candles AAPL | `"You don't have access to this resource."`, so daily prices are paid only → dropped |
| Twelve Data | daily AAPL | full daily prices → **usable** |
| Twelve Data | daily MBG on XETR | `"available starting with the Grow or Venture plan"` → non-US paid |

So the Twelve Data server only offers the US securities (IBM, AAPL).

## 3. Files

| File | Change |
|---|---|
| `src/twelvedata_quelle.hpp` / `.cpp` | **new**: `TwelveDataQuelle : IPriceSource` |
| `src/kursdaten.hpp` / `.cpp` | `Wertpapier` gets the optional `symbole` (symbol per provider), `bietet()`, `symbolFuer()` |
| `src/katalog.hpp` / `.cpp` | reads `symbole`; `fuerAnbieter(name)` filters the securities of a provider |
| `src/alphavantage_quelle.cpp` | **one line**: asks for `symbolFuer("alphavantage")` (= `symbol` when there's no `symbole`) |
| `src/dateiquelle.hpp` / `.cpp` | parser as a parameter (default: Alpha Vantage format) |
| `src/anfragebehandlung.hpp` / `.cpp` | optional provider name; field `anbieter` in every successful answer |
| `src/main.cpp` | option `--quelle alphavantage\|twelvedata`; default data folder `daten/<provider>`; catalog filtered |
| `config/katalog.json` | `symbole` for all 6; only IBM and AAPL have `twelvedata` |
| `daten/` | Alpha Vantage files moved to `daten/alphavantage/` (Git shows renames); new `daten/twelvedata/` (your recordings); `daten/README.md` overview |
| `web/index.html`, `app.js`, `style.css`, `erklaerungen.js` | header "Kurse: …", tab title "MarketLens – …", "?" with an explanation of the provider |
| `tests/test_twelvedata.cpp`, `tests/fixtures/twelvedata/` | **new**: 16 tests, 2 recorded and 12 hand-written answers |
| `tests/test_katalog.cpp` | the offline-data test reads `daten/alphavantage/` (folder move) and now also checks there are 6 |

## 4. Does it affect Alpha Vantage? No.

- The **Alpha Vantage parser** (`AlphaVantageQuelle::parse`) is **unchanged**. The two small helpers for dates and numbers
  are a separate copy in `twelvedata_quelle.cpp`, on purpose, so the tested Alpha Vantage code stays as it was.
- The only change in `AlphaVantageQuelle` asks for `symbolFuer("alphavantage")`. Without `symbole` that's `symbol`, and in
  the catalog it's the same symbol as before, so identical requests and results.
- **All 193 tests from before F3 still pass.** The only existing test that had to change is the offline-data test, because
  the files moved (`daten/` → `daten/alphavantage/`). It still loads all six securities, and it now also checks the count.
- Separate key (`TWELVEDATA_API_KEY`), separate quota, separate process and folders.

## 5. Symbols per provider (`symbole`)

```json
{ "symbol": "IBM", …, "symbole": { "alphavantage": "IBM", "twelvedata": "IBM" } },
{ "symbol": "MBG.DEX", …, "symbole": { "alphavantage": "MBG.DEX" } }
```
- `bietet(anbieter)`: with `symbole`, only the listed providers; **without** `symbole`, only the default `"alphavantage"`.
  That's why old catalogs and all existing tests work unchanged.
- `symbolFuer(anbieter)`: the provider's symbol, or `symbol` without `symbole`. It throws for a provider that doesn't carry the
  security. That would be a programming error, because `main` filters first.
- `Katalog::fuerAnbieter("twelvedata")` → IBM, AAPL. So on port 8081 the list has 2 entries, and `MBG.DEX` gives **404** like any
  unknown symbol (E8).
- Displayed is always `symbol`. Data files are named after `symbol` in every folder (`IBM.json` in both).
- Invalid `symbole` (not an object, value not a string, empty) → the entry is skipped and reported, like other catalog errors (E2).

## 6. `TwelveDataQuelle`

Same pattern as `AlphaVantageQuelle` (E2): the key is read in the constructor (missing → error **before** any request), all
access goes through `IHttpClient`, all errors are `QuellenFehler` subclasses, and the result is a validated `Kursreihe`.

**Format differences:** a list `values` (newest day **first**, so it gets sorted ascending), fields `datetime`, `open`, `high`,
`low`, `close`, `volume`, numbers as text (read with `std::from_chars`), errors as `{"code": …, "message": …, "status": "error"}`.

**Errors from two places:** I don't know for sure whether Twelve Data sets the real HTTP status for errors or sends 200 with
`"status":"error"`. Both are evaluated with the same mapping (`wirfFehler`):

| Code | Result |
|---|---|
| 401 | `HttpFehler(401)` "API key invalid" + Twelve Data's text |
| 403 | `HttpFehler(403)` |
| 404 | `HttpFehler(404)` "unknown symbol or not in the free plan" + text (e.g. "Grow or Venture plan") |
| 429 | `KontingentFehler` (8 per minute or the daily credits); E7 can then serve stale data |
| other | `HttpFehler(code)` + text |

Data problems → `ParseFehler`: invalid JSON, no `values`, a missing field (message names date and field), not a number, a date
with a time (no daily price), an inconsistent candle (E1 message), a duplicate day.

`bewerte()` for the disk cache (E7), as for Alpha Vantage and FRED: 429 in status or content → quota; parses → usable; else unusable.

## 7. Two servers side by side

`main` chooses by `--quelle`:
- the **parser** of `DateiQuelle` (Alpha Vantage or Twelve Data format),
- the **data folder** `daten/<quelle>` (unless `--daten` is given),
- the **catalog filter** `fuerAnbieter(quelle)`,
- the **name** for the answers and the startup message ("6 Wertpapiere (Alpha Vantage)", "2 Wertpapiere (Twelve Data)").

The page reads `anbieter` from `/api/wertpapiere` and shows "Kurse: Twelve Data" in the header and as the **tab title**
("MarketLens – Twelve Data"). **Drawings (F1)** are automatically separate, because the browser keeps `localStorage` per address
including the port.

**Deviation from the description:** the option `--cache` is **not** added yet. The disk cache is only used with the live
client, and an option without an effect would be confusing. It comes with the live client.

## 8. Tests (`tests/test_twelvedata.cpp`, 16 new)

| Test | Checks |
|---|---|
| `SymboleJeAnbieter.WertpapierOhneUndMitSymbolen` | default behaviour without `symbole`; per-provider symbols; unknown provider throws; empty values rejected |
| `SymboleJeAnbieter.EchterKatalogNachAnbieter` | real catalog: Alpha Vantage 6, Twelve Data IBM + AAPL, unknown 0 |
| `SymboleJeAnbieter.FehlerhafteSymboleImKatalog` | not an object, non-string, empty → skipped with position and message |
| `TwelveDataTest.FehlenderKeyVorDerErstenAnfrage` | no key → constructor throws, 0 requests |
| `TwelveDataTest.Url` | exact URL; encoding |
| `TwelveDataTest.GueltigeAntwort` | newest-first → ascending; all fields; 1 request |
| `TwelveDataTest.FragtKuerzelDesAnbietersAn` | requests the Twelve Data symbol, not the display symbol; security not carried → no request |
| `TwelveDataFehler.FehlerImInhalt` | 401, 404 (with "free plan" and Twelve Data's text), 400, 429 → quota, no code |
| `TwelveDataTest.FehlerImHttpStatus` | HTTP 401 with Twelve Data's text, 429, 500 without JSON, network |
| `TwelveDataFehler.UngueltigeAntworten` | truncated, array, no `values`, `n/a`, date with time, duplicate day, missing field (date + field), inconsistent candle |
| `TwelveDataFehler.Bewertung` | cache rating for all cases |
| `TwelveDataAufgezeichnet.IbmUndApple` | **real** recordings: 100 days, first/last dates and prices |
| `TwelveDataAufgezeichnet.GleicheKurseWieAlphaVantage` | **comparison**: same days, closes differ by at most 0.01 (only prices; providers count volume differently) |
| `TwelveDataAufgezeichnet.OfflineDatenVollstaendig` | every Twelve Data security of the catalog loads from `daten/twelvedata/` |
| `TwelveDataServer.NurUsWertpapiereUndAnbieterInDerAntwort` | built like `main` for port 8081: list = IBM, AAPL; `anbieter` in answers; MBG.DEX → 404 |
| `TwelveDataServer.OhneAnbieterNamenIstAnbieterNull` | without a name: `"anbieter": null` (existing behaviour) |

The text check from F2 (`Weboberflaeche.ErklaerungenVollstaendig`) also covers the new "?" for the provider.

## 9. Checks

- **Automatic:** **209 of 209** tests pass (16 new), full rebuild with 0 warnings.
- **Two servers really running side by side** (in the container, 2026-11-16): Alpha Vantage server 6 securities, IBM close 247.14,
  MBG.DEX 200; Twelve Data server IBM + AAPL, IBM close 247.13, MBG.DEX 404; every answer names its provider. A third server on
  the same port is rejected; `--quelle finnhub` gives a clear error; after stopping one server the other keeps answering.
- **Your recordings:** 100 trading days each, same days as Alpha Vantage, closes within 0.01, volumes differ, no key in the files.
- **In a browser (2026-11-16):** all 10 cases **F3-1 … F3-10** of `docs/testprotokoll.md` passed, with both servers running in
  separate containers on ports 8080 and 8081.

## 10. Next: live access

The live HTTPS client (cpp-httplib + OpenSSL behind a CMake option, `libssl-dev` in the `Containerfile`) is the same for both
providers. Then `main` would use `AlphaVantageQuelle` or `TwelveDataQuelle` over `CachingHttpClient` with `--cache cache/<provider>`
instead of `DateiQuelle`. The rating functions (`bewerte`) for the cache already exist for both.

## 11. Possible exam questions

- *How is a second provider added without touching the analysis?* → A new `IPriceSource` implementation; everything else only knows the interface.
- *How does the Twelve Data server show only US securities?* → `symbole` in the catalog; `fuerAnbieter("twelvedata")` keeps only entries with that provider.
- *Why does the old catalog still work?* → Without `symbole`, a security belongs to the default provider with `symbol` as its symbol.
- *Why are the date/number helpers duplicated?* → So the tested Alpha Vantage code stays unchanged by F3; the copies are small.
- *How do you know both providers deliver the same data?* → A test compares the closes of both recordings day by day (≤ 0.01).
- *Why are the drawings separate without extra code?* → `localStorage` is per origin, and the port is part of the origin.
- *Why no `--cache` option yet?* → It would have no effect offline; it comes with the live client.
