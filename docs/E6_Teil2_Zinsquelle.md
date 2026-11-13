# E6, part 2 – Loading central bank rates from FRED

This file explains the second part of E6, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.
Part 1 (data model and event study) is in `E6_Ereignisstudie.md`.

## 1. E6 status after part 2

| E6 DoD point | Status |
|---|---|
| decisions of **several** central banks (bank, country, date, rate) | **done**: Fed and ECB from the catalog |
| loaded through `IRateSource` and `IHttpClient` from a web API | **done**: `FredQuelle` |
| validated | **done**: `Zinsentscheidung` (part 1) + checks of the FRED answer |
| errors handled as in E2 | **done**: missing key before the first request, network, 400/404, 429, invalid JSON, missing fields |
| price change on the trading day after each decision of the **central bank of the security's country** | **done** (part 1), now with real country lists |
| non-trading days → next trading day; outside the data → skipped; no matching bank → empty | **done** (part 1) |

**E6 is complete**, including real data: you recorded both FRED answers on 2026-11-13 (section 8).

## 2. Your two decisions (2026-11-13)

1. **Dates: use FRED's dates and document it.** FRED stores the policy rate for every day. The date where it changes is the
   day the new rate takes **effect**, not the day of the meeting: usually the next day for the Fed, up to a week later for
   the ECB. So the event study measures the trading day after the **effective date**. This is written in the code
   (`fred_quelle.hpp`), in `daten/zinsen/README.md`, and in **every** `/api/ereignisstudie` answer as
   `"datumsart": "wirksam"`, so the UI (E10) can show it.
2. **Banks: Fed and ECB.** Both have **daily** series on FRED: `DFEDTARU` (upper limit of the Fed's target range) and
   `ECBDFR` (ECB deposit facility rate). For the Bank of England and the Bank of Japan only monthly series are known there.
   "Decision dates" on the 1st of a month would make the event study misleading. Because the series ID is a catalog entry,
   another bank is just one more entry, without code changes.

Another limitation follows from the data: **decisions that keep the rate unchanged are invisible**, because nothing changes
in the series.

## 3. Files

| File | Purpose |
|---|---|
| `src/zentralbank.hpp` / `.cpp` | `Zentralbank`: name, **list of countries**, currency, FRED series; validating constructor |
| `src/rate_source.hpp` | interface `IRateSource` (counterpart of `IPriceSource`) |
| `src/fred_quelle.hpp` / `.cpp` | `FredQuelle`: URL, errors, reading the daily values, deriving decisions, cache rating (E7) |
| `src/datei_zinsquelle.hpp` / `.cpp` | `DateiZinsquelle`: offline, reads `daten/zinsen/<SERIES>.json` |
| `src/zinsabruf.hpp` / `.cpp` | `ladeAlleEntscheidungen`: one faulty bank doesn't stop the others |
| `src/katalog.hpp` / `.cpp` | **extended:** optional section `"zentralbanken"`, `zentralbanken()`, `zentralbankFehler()` |
| `src/anfragebehandlung.cpp` | **extended:** `"datumsart": "wirksam"` in `/api/ereignisstudie` |
| `src/main.cpp` | **extended:** loads all rates at start (`--zinsen ORDNER`, default `daten/zinsen`) |
| `config/katalog.json` | **extended:** Fed (USA) and ECB (21 euro countries) |
| `daten/zinsen/README.md` | how to record the two real FRED answers |
| `tests/fixtures/fred/*`, `tests/fixtures/katalog/zentralbanken.json` | hand-written test data |
| `tests/test_fred.cpp` | 17 tests |

## 4. Countries: the euro area problem, solved without changing `PROJEKT.md`

E6 says "die Zentralbank **seines Landes**". A German stock has `land: "Deutschland"`, and the ECB is responsible for 21
countries. So in the catalog each bank has a **list** of countries:

```json
"zentralbanken": [
  { "name": "Fed", "laender": ["USA"], "waehrung": "USD", "fredSerie": "DFEDTARU" },
  { "name": "EZB", "laender": ["Belgien", "Bulgarien", "Deutschland", …, "Zypern"], "waehrung": "EUR", "fredSerie": "ECBDFR" }
]
```
(21 countries: Bulgaria joined the euro area on 1 January 2026.)

**How the event study uses it, without being changed:** for every decision, `FredQuelle` creates **one `Zinsentscheidung`
per country** of the bank. An ECB decision becomes 21 entries (EZB/Belgien, EZB/Bulgarien, EZB/Deutschland, …). The event
study from part 1 still just compares `entscheidung.land == wertpapier.land`, so a German stock gets each ECB decision
exactly once. Neither its code nor its tests had to change. The extra entries are tiny (21 × a few dozen decisions).

The section `"zentralbanken"` is **optional**. Older catalogs (and `tests/fixtures/katalog/gemischt.json`) still work
unchanged. Faulty bank entries are reported in their own list `zentralbankFehler()` and skipped, exactly like securities.

## 5. `FredQuelle`

### 5.1 Request
```
https://api.stlouisfed.org/fred/series/observations?series_id=DFEDTARU&api_key=<KEY>&file_type=json&observation_start=2015-01-01
```
- The key comes from `FRED_API_KEY`. The **constructor** reads it, so a missing key fails before any request (same pattern as E2).
- `observation_start` limits the size (daily values since 2015 ≈ 4000 entries).

### 5.2 Errors
FRED puts the reason into the body as JSON, e.g.
`{"error_code":400,"error_message":"Bad Request.  The value for variable api_key is not registered. …"}`.
So for error statuses `FredQuelle` reads `error_message` itself instead of calling `pruefeStatus`:

| Answer | Result |
|---|---|
| network down / timeout | `NetzwerkFehler` / `ZeitueberschreitungFehler` (from the client) |
| 429 | `KontingentFehler` |
| 400 with "does not exist" | `HttpFehler(404)`: unknown series, treated like an unknown symbol in E2 |
| other 4xx/5xx | `HttpFehler(status)`, message includes FRED's `error_message` (e.g. "api_key is not registered") |
| invalid JSON, no `observations`, field missing, value not a number, invalid or duplicate date | `ParseFehler` |

### 5.3 From daily values to decisions
`leseBeobachtungen` reads `observations: [{"date": "2024-09-19", "value": "5.00"}, …]`:
- `value` is **text**. `"."` means "no value for this day" (e.g. a holiday) and is **skipped**.
- Numbers are read with `std::from_chars` (whole text, independent of the locale, as in E2).
- The list is **sorted** by date, and a duplicate date is an error.

`leiteEntscheidungenAb` compares each value with the previous one. **A change is a decision**, with the new rate on that
date. The first day is never a decision, because we don't know what came before it. Negative rates are allowed (the ECB had
−0.50 % in 2019).

Example (`tests/fixtures/fred/fed_ok.json`, modelled on the Fed's 2024 cuts):
```
2024-09-16 5.50 | 09-17 5.50 | 09-18 5.50 | 09-19 5.00 ← decision | 09-20 5.00 | 09-21 "." (skipped)
2024-11-07 5.00 | 11-08 4.75 ← decision | 11-09 4.75
```

### 5.4 Cache (E7)
`FredQuelle::bewerte` is the counterpart of `AlphaVantageQuelle::bewerte`: 429 means quota, an answer that parses is usable, and
anything else isn't. So `CachingHttpClient` can be put in front of FRED later, exactly as for prices.

## 6. Offline and in `main`

`DateiZinsquelle` reads `daten/zinsen/<SERIES>.json` and evaluates it with the **same** functions as a live answer. Series names
may only contain letters, digits and `_` (no `../`).

At startup `main` loads all banks once (`ladeAlleEntscheidungen`):
```
Warnung: Zinsdaten von EZB nicht geladen: No offline rate data for EZB: file not found: daten/zinsen/ECBDFR.json
MarketLens: 6 Wertpapiere, …, 2 Zentralbanken (2 Zinseinträge), 8 Threads
```
A missing file only means no events for that bank. The server still starts.

Rates are loaded **once at startup**, not per request. Policy rates change a few times a year, and it saves the provider's quota.

## 7. Tests (`tests/test_fred.cpp`)

| Test | Checks |
|---|---|
| `Zentralbank.ZustaendigkeitUndValidierung` | `zustaendigFuer`; empty name, empty list, empty country, currency, series |
| `FredQuelle.LeseBeobachtungen` | 9 observations, `"."` skipped → 8, sorted although two days are swapped |
| `FredQuelle.LeiteEntscheidungenAb` | 2 decisions: 2024-09-19 5.00 and 2024-11-08 4.75 |
| `FredQuelle.EineEntscheidungProLand` | bank with 2 countries → 2 × 2 entries |
| `FredQuelle.OhneAenderungKeineEntscheidung` | empty, one value, constant → none; negative rate allowed |
| `FredQuelle.UngueltigeAntworten` | cut-off JSON, no `observations`, `"abc"`, 30 Feb, duplicate day, missing `value`, top-level array |
| `FredTest.FehlenderKeyVorDerErstenAnfrage` | no key → constructor throws, 0 requests |
| `FredTest.Url` | exact URL |
| `FredTest.LaedtUndLeitetAb` | via `FakeHttpClient`: 2 decisions, 1 request |
| `FredTest.Fehlerfaelle` | 400 key (message contains `api_key`), 400 unknown series → 404, 429, 500, network |
| `FredQuelle.Bewertung` | cache rating for valid, 429, 400, broken |
| `DateiZinsquelle.LiestUndPrueft` | reads a file; missing file; `../geheim` rejected |
| `Zinsabruf.FehlerEinerBankVerhindertNichtDieUebrigen` | 3 banks, one file missing → 4 decisions, 1 error |
| `KatalogZentralbanken.EchterKatalog` | Fed + ECB, 21 countries, `MBG.DEX` (Deutschland) belongs to the ECB |
| `KatalogZentralbanken.FehlerhafteEintraege` | 7 entries → 2 valid, 5 errors with positions, first duplicate wins |
| `KatalogZentralbanken.OptionalerAbschnitt` | catalog without banks is fine; non-array → `ParseFehler` |
| `FredAufgezeichnet.FedUndEzb` | **real** FRED answers: 4335 days each; Fed 32 changes (last 2026-09-17 → 4.00 %), and the 2024 cuts from the hand-written data really exist; ECB starts at −0.20 %, 23 changes × 2 countries |
| `FredAufgezeichnet.EreignisstudieMitEchtenDaten` | **real** Fed rates + **real** IBM prices: exactly one event, effective Thu 2026-09-17 → Fri 2026-09-18, 237.75 → 229.55, −3.45 % |
| `FredBisEreignisstudie.GanzeKette` | FRED data → decisions → `/api/ereignisstudie`: `datumsart: wirksam`, effective 2024-09-19 (Thu) → reaction Fri 2024-09-20, change 104/101 − 1; the November decision is outside the prices and skipped; a UK stock → empty list |

## 8. The real FRED answers (recorded 2026-11-13)

You recorded both series in WSL with your key. Checked before use:
- valid FRED JSON (`observations`, no `error_message`), and **no key** in either file (`grep -ci api_key` → 0)
- `DFEDTARU`: 4335 daily values 2015-01-01 … 2026-11-13, **32 changes**, last: 3.75 → 4.00 % effective 2026-09-17
- `ECBDFR`: 4335 daily values, **23 changes**, starting at −0.20 % (negative rates), last: 2.50 % effective 2026-09-16

So both series IDs are correct and daily. The files are in `daten/zinsen/` (used by the server) and copied to
`tests/fixtures/fred/aufgezeichnet_*.json` (used by the tests). That way the tests stay the same if `daten/zinsen/` is re-recorded.

**Real end-to-end result** (`./build/projekt`, then `GET /api/ereignisstudie?symbol=IBM`):
```json
{"datumsart":"wirksam","ereignisse":[{"zentralbank":"Fed","datum":"2026-09-17","leitzins":4.0,
 "reaktionstag":"2026-09-18","kursVorher":237.75,"kursNachher":229.55,"veraenderung":-0.03449…}], …}
```
At startup: `2 Zentralbanken (515 Zinseinträge)` = 32 Fed + 23 ECB × 21 countries.

## 9. Result

188 of 188 tests pass (19 new). A full rebuild shows 0 warnings. Manually checked: without files the server starts with two
warnings; with the real files it loads 515 entries and shows the real IBM event (section 8).

## 10. Possible exam questions

- *How do you get "decisions" from FRED?* → FRED has a value per day; a decision is a day where the value changes.
- *What's the limitation of that?* → The date is when the rate takes effect, not the meeting day; and "no change" decisions are invisible.
- *How does a German stock find the ECB?* → The catalog lists the ECB's countries; each decision is created once per country,
  so the event study can still compare countries.
- *Why didn't the event study code change?* → `IRateSource` delivers decisions per country, which is exactly what it already expected.
- *Why is an unknown series turned into 404?* → Same meaning as an unknown symbol, independent of FRED's choice of 400.
- *Why are rates loaded only at startup?* → They change rarely; it saves the quota and keeps requests fast.
- *How do you add the Bank of England later?* → One catalog entry with its countries and a daily series ID; no code change.
