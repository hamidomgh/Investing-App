# F5 (Part 2) – Live sources wired into the server: what was built and why

This file explains part 2 of feature F5 so you can explain it in the exam. Part 1 (the network client itself) is
`docs/F5_Teil1_LiveClient.md`; the specification (German) is `Features/F5_Live_Zugriff.md`.
Built on the Git branch **`live-zugriff`**.

## 1. What it does

```
./build-live/projekt --live --quelle alphavantage --port 8080     # 6 securities, cache/alphavantage/, 12 h
./build-live/projekt --live --quelle twelvedata   --port 8081     # IBM, AAPL,     cache/twelvedata/,   1 h
```

With `--live`, prices come from the provider instead of `daten/`. They are fetched **on demand** (option "a"):
only when the browser asks for a security **and** its cached answer is older than the validity time. Without `--live`
nothing changes, so the offline mode and all earlier tests stay as they were.

The whole chain in live mode:

```
HttpServer → ErgebnisCache (5 min) → Anfragebehandlung → SynchronisierteQuelle
  → LiveKursquelle → AlphaVantageQuelle / TwelveDataQuelle → CachingHttpClient (disk, 12 h / 1 h) → LiveHttpClient → internet
```

## 2. Files

| File | Change |
|---|---|
| `src/quellen/live_kursquelle.hpp` / `.cpp` | **new**: `LiveKursquelle : IPriceSource`, builds the chain for one provider |
| `tests/test_live_kursquelle.cpp` | **new**: 7 tests with `FakeHttpClient` and `FakeUhr` |
| `src/main.cpp` | options `--live`, `--cache`, `--gueltigkeit`; chooses offline or live source; start message |
| `CMakeLists.txt` | new source and test file |
| `.gitignore` | `cache/` (downloaded data is not committed) |
| `docs/testprotokoll.md` | manual cases F5-1 … F5-10 |

## 3. `LiveKursquelle`

It is an `IPriceSource`, so everything above it (`SynchronisierteQuelle`, `Anfragebehandlung`) is unchanged. Inside it
owns two things:

| Member | Role |
|---|---|
| `CachingHttpClient cache_` | disk cache around the network client, with the **rating function of the provider** (`AlphaVantageQuelle::bewerte` or `TwelveDataQuelle::bewerte`) |
| `std::unique_ptr<IPriceSource> quelle_` | `AlphaVantageQuelle` or `TwelveDataQuelle`, asking **through** `cache_` |

**Member order matters:** members are constructed in the order they are declared. `quelle_` gets a reference to
`cache_`, so `cache_` must be declared first. A comment in the header says so.

**The provider is checked first**, in the initializer list (`pruefeAnbieter`). So a typo like `finnhub` throws before
the cache folder is created. A test checks that no folder is left behind.

**Recommended validity** (`standardGueltigkeit`): Alpha Vantage **12 h**, because the free plan allows only about
25 calls a day and there are 6 securities. Twelve Data **1 h**, with about 800 calls a day and 2 securities.
`--gueltigkeit H` overrides it (1–168 hours).

**Why a class and not code in `main`?** `main.cpp` is not part of `projekt_lib`, so code there can't be tested.
In the class, the tests use `FakeHttpClient` instead of the internet and `FakeUhr` instead of waiting 12 hours.

## 4. Changes in `main`

- `--live` is the only option **without** a value, so it is handled before the "needs a value" check.
- **Contradicting options are rejected** instead of silently ignored: `--daten` only works offline, and `--cache` and
  `--gueltigkeit` only work live. (In F3 we also left out an option that would have had no effect, because it confuses.)
- **`--live` without OpenSSL stops at startup**, naming the CMake option. That is clearer than starting fine and failing
  at the first click.
- **A missing API key stops at startup:** the source constructors read the key (DoD E2), throw `KonfigurationsFehler`,
  and the existing `catch (const QuellenFehler&)` prints it.
- **Choosing the source at runtime:** `std::unique_ptr<IPriceSource> kursquelle` holds either a `DateiQuelle` or a
  `LiveKursquelle`. Both are used through the interface (polymorphism).
- `SystemUhr uhr` moved up, because the disk cache now needs it as well as the result cache.
- `LiveHttpClient netz` is always created, but in offline mode nobody calls it. It has no state, so creating it costs
  nothing.

## 5. Tests (7 new, `tests/test_live_kursquelle.cpp`)

| Test | Checks |
|---|---|
| `LiveKursquelleGueltigkeit.JeAnbieter` | 12 h / 1 h, unknown provider → `KonfigurationsFehler` |
| `LiveKursquelleTest.UnbekannterAnbieter` | error, and no cache folder is created |
| `LiveKursquelleTest.FehlenderSchluessel` | missing key fails at construction, no request is made |
| `LiveKursquelleTest.AlphaVantageAbrufBeiBedarf` | 1st load → 1 request; after 11 h still 1; after 13 h → 2 (the core of option "a") |
| `LiveKursquelleTest.TwelveDataNutztEigeneQuelle` | request goes to `api.twelvedata.com`, Twelve Data parser used |
| `LiveKursquelleTest.KontingentErschoepftLiefertAlteDaten` | Alpha Vantage "Information" (HTTP 200!) → old data, `veraltet`, note; shows the right rating function is used |
| `LiveKursquelleTest.OhneNetzUndOhneCacheFehler` | no network and no cache → `NetzwerkFehler` reaches the caller (the server makes it HTTP 502) |

**Results:** **224 of 224** tests pass (217 after part 1 + 7 new), no warnings. Checked by hand in the normal build:
`--live` (HTTPS error), `--cache x`, `--gueltigkeit 5`, `--live --daten daten`, `--gueltigkeit 0/169`,
`--gueltigkeit` without a value, `--hilfe`, and an offline start (unchanged message).

**Still open:** the real fetch (F5-1 … F5-8 in `docs/testprotokoll.md`). It needs the new container with `libssl-dev`.

## 6. Possible exam questions

- *When does the server call the API?* → Only when a security is requested and its disk-cache entry is older than the validity time (12 h / 1 h).
- *What happens after 5 days with the laptop off?* → The first request finds an expired entry and makes one call. The answer has the last 100 trading days, so no days are missing.
- *Why 12 h for Alpha Vantage?* → About 25 free calls a day for 6 securities; 12 h means at most 12 calls a day.
- *Why must `cache_` be declared before `quelle_`?* → Members are initialized in declaration order; `quelle_` holds a reference to `cache_`.
- *How does the cache know that Alpha Vantage's HTTP 200 is a quota error?* → `LiveKursquelle` passes `AlphaVantageQuelle::bewerte` as the rating function.
- *How do you test "12 hours later" without waiting?* → `FakeUhr::vorstellen`; the cache asks the clock, not the system time.
- *Why reject `--cache` without `--live`?* → An option without effect is confusing; a clear error is better.
- *What does the browser show without internet?* → Old data with the stale note (E7), or HTTP 502 if the security was never loaded.
