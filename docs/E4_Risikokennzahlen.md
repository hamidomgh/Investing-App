# E4 – Volatility and risk metrics: what was built and why

This file explains the E4 implementation, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> Bollinger bands, ATR, simple and logarithmic daily returns, annualised volatility and
> maximum drawdown are calculated. The values match hand-calculated reference values, and
> special cases (constant prices, series with only one candle) are defined and handled.

## 2. Files

| File | Purpose |
|---|---|
| `src/analyse/risikokennzahlen.hpp` | Declaration: class `Risikokennzahlen`, struct `BollingerErgebnis` |
| `src/analyse/risikokennzahlen.cpp` | Implementation of all six metrics |
| `tests/test_risikokennzahlen.cpp` | 14 tests with hand-calculated values, special cases and errors |
| `CMakeLists.txt` | New `.cpp` added to `projekt_lib`, new test file to `tests` |

## 3. Design decisions

### 3.1 Same pattern as E3
`Risikokennzahlen rk(reihe);` follows the same pattern as `Trendindikatoren`: an `explicit`
constructor, one method per metric, `Indikatorreihe` (`std::vector<std::optional<double>>`)
for anything that has one value per day, and `std::invalid_argument` for invalid input.

Difference: `Risikokennzahlen` stores the whole `Kursreihe`, not only the closing prices,
because the ATR needs High, Low and Close.

### 3.2 Reuse of E3
The middle Bollinger band *is* the SMA. So `bollinger()` calls
`Trendindikatoren(reihe_).sma(periode)` instead of writing the SMA again. That call also
checks the period and the length of the series, so those errors come for free.

### 3.3 Prices must be positive for returns and drawdown
E1 allows a close price of 0 (it only checks Low ≤ Close ≤ High). But returns divide by
yesterday's price, `ln(0)` is undefined, and the drawdown divides by the peak. So these
methods call the private helper `pruefePositiveKurse()`, which throws with index and value.
Bollinger and ATR don't divide by prices, so they don't need this check.

### 3.4 How the special cases are defined

| Case | Bollinger | ATR | Returns | Volatility | Max drawdown |
|---|---|---|---|---|---|
| Constant prices | all bands = middle | true range 0 → ATR 0 | all 0 | 0 | 0 |
| One candle | only with period 1 (band = price) | ATR(1) = High − Low | one entry, `nullopt` | `nullopt` | 0 |
| Empty series | error (too short) | error (too short) | empty vector | `nullopt` | 0 |

Why volatility returns `std::optional<double>` and not an error: having too few data points is
a normal situation (for example a newly listed security), not a programming mistake. The caller
can then show "not available".

## 4. The metrics

Notation: `c[i]` = close, `h[i]` = high, `l[i]` = low on day `i`, `n` = period.

### 4.1 Bollinger bands (`bollinger(n, k)`, typical 20, 2)
- Middle = SMA(n).
- σ = standard deviation of the same `n` closes, **population** formula (divide by `n`). This
  is the convention John Bollinger uses.
- Upper = middle + k·σ, lower = middle − k·σ.
- `k < 0` is rejected, because it would swap upper and lower.

The inner loop recomputes σ for each window: O(N·n). That's simple to explain and fast
enough for daily data.

### 4.2 ATR – Average True Range (`atr(n)`, typical 14)
- True range: `TR[i] = max(h − l, |h − c[i−1]|, |l − c[i−1]|)`. It includes overnight gaps:
  if the price jumps from 12 to 14–16, the range was really 4, not 2. On day 0 there is no
  previous close, so `TR[0] = h − l`.
- First ATR at index `n − 1` = average of `TR[0..n−1]`.
- After that, Wilder smoothing (same as RSI in E3): `ATR = (ATR_old · (n − 1) + TR) / n`.

### 4.3 Returns
- Simple: `r[i] = c[i] / c[i−1] − 1` (0.1 = +10 %).
- Log: `r[i] = ln(c[i] / c[i−1])`. Log returns add up over time, which makes them the
  standard input for volatility.
- Index 0 is `nullopt` (no previous day). The result keeps the same length as the series, like
  all indicators.

### 4.4 Annualised volatility (`annualisierteVolatilitaet(handelstage = 252)`)
1. Take the log returns (without the `nullopt` at index 0).
2. **Sample** standard deviation (divide by `count − 1`), because the returns are a sample of
   the unknown "true" distribution. That needs at least 2 returns, so 3 candles.
3. Multiply by √252. Volatility grows with the square root of time, and a year has about 252
   trading days. The parameter lets you use other values (e.g. 365 for crypto).

Note the difference to Bollinger: Bollinger uses the population formula (`/ n`), volatility
the sample formula (`/ (n − 1)`). Both are the standard conventions for their metric.

### 4.5 Maximum drawdown (`maxDrawdown()`)
Walk through the prices once, remember the highest price so far (`hoechststand`) and the
largest relative drop `(peak − c) / peak`. Result between 0 and 1 (0.5 = −50 %). One pass,
O(N).

## 5. Tests and reference values

The hand calculation is written as a comment above each test.

| Test | Checks |
|---|---|
| `BollingerReferenzwerte` | Bollinger(3, 2) on `2,4,6,8,4`: variance 8/3, bands = middle ± 2·√(8/3) |
| `BollingerKonstanteKurse` | σ = 0 → all bands equal |
| `BollingerFehlerfaelle` | period 0, k < 0, period longer than series |
| `AtrReferenzwerte` | ATR(2) with an upward gap: TR 3, 3, 4, 3 → ATR 3, 3.5, 3.25 |
| `AtrEineKerze` | ATR(1) of one candle = High − Low |
| `AtrFehlerfaelle` | period 0, series too short |
| `RenditenReferenzwerte` | `100, 110, 99` → simple 0.1 / −0.1, log ln 1.1 / ln 0.9 |
| `RenditenEineKerze` | one candle → one `nullopt`, no error |
| `RenditenLehnenNichtPositiveKurseAb` | close 0 → returns, volatility and drawdown throw |
| `VolatilitaetReferenzwert` | `100, 110, 99` → (ln 1.1 − ln 0.9)/√2 · √252 ≈ 2.2525 |
| `VolatilitaetSonderfaelle` | constant → 0, one or two candles → `nullopt` |
| `VolatilitaetFehlerfaelle` | 0 trading days |
| `MaxDrawdownReferenzwert` | `100,120,90,110,60,130` → 0.5 (from 120 to 60) |
| `MaxDrawdownSonderfaelle` | constant, one candle, empty, only rising → 0 |

The test helpers `reiheAusOhlc` (full candles, for the ATR) and `reiheAus` (close only) are
in an anonymous namespace, so they can't clash with helpers in other test files.

## 6. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 46 of 46 tests pass
(14 new), no compiler warnings.

## 7. Possible exam questions

- *Why population σ for Bollinger but sample σ for volatility?* → Those are the standard
  conventions. Bollinger describes the current window; volatility estimates an unknown
  distribution from a sample.
- *Why √252?* → Variance grows linearly with time, so standard deviation grows with √time.
  252 is the typical number of trading days per year.
- *Why does the true range use the previous close?* → It captures gaps between days, which the
  daily high–low range misses.
- *Why log returns for volatility?* → They add up over time: the sum of the daily log returns
  is exactly the log return of the whole period. Simple returns don't: +10 % and −10 % add up
  to 0, but the price went from 100 to 99. The log returns ln 1.1 + ln 0.9 = ln 0.99 show the
  real loss.
- *Why `std::optional` for volatility instead of an exception?* → Too few data points is a
  normal situation, not an error.
- *Why does a close of 0 throw here when E1 allows it?* → E1 only checks that the candle is
  consistent. Returns and drawdown divide by prices, so they need prices > 0.
