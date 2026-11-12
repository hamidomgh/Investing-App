# E3 – Trend and momentum indicators: what was built and why

This file explains the E3 implementation step by step, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> For a price series, SMA, EMA, MACD (line, signal, histogram) and RSI are calculated with
> adjustable periods. The values match hand-calculated reference values, values before the
> warm-up phase is over are marked as "not available", and series that are too short or
> invalid periods are rejected with an error.

## 2. Files

| File | Purpose |
|---|---|
| `src/trendindikatoren.hpp` | Declaration: the class `Trendindikatoren`, the type `Indikatorreihe`, the struct `MacdErgebnis` |
| `src/trendindikatoren.cpp` | Implementation of all four indicators |
| `tests/test_trendindikatoren.cpp` | 10 GoogleTest tests with hand-calculated reference values and error cases |
| `CMakeLists.txt` | The new `.cpp` file was added to `projekt_lib`, the new test file to `tests` |

## 3. Design decisions

### 3.1 One class, created from a `Kursreihe`
`Trendindikatoren ind(reihe);` copies the closing prices once in the constructor. Every
indicator then works on `schlusskurse_` (a `std::vector<double>`). Reason: all four
indicators only use the close price, and we don't want to extract it again every time.

The constructor is `explicit`, so a `Kursreihe` is never silently converted into a
`Trendindikatoren` object.

Because a `Kursreihe` is already validated (E1), the indicators don't need to check the
prices again.

### 3.2 "Not available" = `std::optional<double>`
`using Indikatorreihe = std::vector<std::optional<double>>;`

- The result always has **the same length as the price series**: index `i` of the result
  belongs to day `i`. This makes it easy to draw later (E10) and to combine indicators.
- During the warm-up phase there is not enough data, so the entry is `std::nullopt`.
  The alternative, using `0` or `NaN`, would be ambiguous: 0 can be a real value, and NaN
  is easy to miss.
- Reading a value: `sma[i].has_value()` checks it, `*sma[i]` reads it.

### 3.3 Errors are exceptions
Invalid input throws `std::invalid_argument` with a readable message, for example
`"SMA: series too short, need 4 values, got 3"`. This is the same style as E1.
The shared checks live in the private helper `pruefePeriode`, so the messages are consistent.

### 3.4 Reuse: `emaUeber`
The EMA is needed twice: on the prices (`ema()`) and on the MACD line (signal line).
So the EMA math lives in one private `static` helper `emaUeber(werte, periode)` that works on
any `std::vector<double>`. It is `static` because it doesn't use any member of the object.

## 4. The indicators

Notation: `c[i]` = closing price on day `i`, `n` = period.

### 4.1 SMA – Simple Moving Average
`SMA[i] = (c[i-n+1] + ... + c[i]) / n`, available from index `n-1`.

Implementation trick: a **sliding window**. Instead of adding up `n` values for every day, the
code keeps a running `summe`: add the new price, subtract the price that drops out
(`c[i-n]`). That is O(length) instead of O(length · n).

Errors: `n <= 0`, or fewer than `n` prices.

### 4.2 EMA – Exponential Moving Average
- Smoothing factor `alpha = 2 / (n + 1)`.
- Start value at index `n-1` is the SMA of the first `n` prices (a common convention).
- After that: `EMA[i] = alpha · c[i] + (1 − alpha) · EMA[i−1]`.

Newer prices weigh more, so the EMA reacts faster than the SMA. The test shows this: on
`2, 4, 6, 8, 4` the last SMA(3) is 6, but the last EMA(3) is 5.

Errors: same as SMA.

### 4.3 MACD – Moving Average Convergence Divergence
Three periods: fast (`schnell`, typically 12), slow (`langsam`, 26), signal (9).

1. `Linie[i] = EMA_fast[i] − EMA_slow[i]`, available from index `slow − 1`.
2. `Signal` = EMA(signal) **of the MACD line**. The available line values are collected
   into a gap-free vector `linienwerte`, `emaUeber` runs on it, and the results are mapped
   back with offset `start = slow − 1` (`i = start + j`).
3. `Histogramm[i] = Linie[i] − Signal[i]`.

Minimum length: `slow + signal − 1` prices, so that all three series have at least one value.

Errors: any period `<= 0`, `fast >= slow` (a "fast" EMA that is not faster makes no sense),
series too short.

### 4.4 RSI – Relative Strength Index (Wilder)
1. Daily change `d[i] = c[i] − c[i−1]`. Positive part = gain, negative part = loss
   (stored as a positive number).
2. First averages at index `n`: plain average of gains and losses over the first `n` changes.
3. After that, **Wilder smoothing**: `avg = (avg_old · (n − 1) + today) / n`.
4. `RS = avgGain / avgLoss`, `RSI = 100 − 100 / (1 + RS)`, always between 0 and 100.

`n` changes need `n + 1` prices, so the first value is at index `n` and the minimum length is `n + 1`.

Special cases (division by zero), defined in the lambda `rsiAus`:
- no losses → RSI = 100
- no gains **and** no losses (constant prices) → RSI = 50 (neutral, our definition)

## 5. Tests and reference values

All reference values are calculated by hand. The calculation is written as a comment above
each test, so you can check it on paper. Small periods (2, 3) and small numbers keep the
math easy.

| Test | Checks |
|---|---|
| `SmaReferenzwerte` | SMA(3) on `2,4,6,8,4` → `–,–,4,6,6` |
| `SmaPeriodeEinsIstDerKurs` | SMA(1) equals the price, no warm-up |
| `SmaFehlerfaelle` | period 0, negative, longer than series |
| `EmaReferenzwerte` | EMA(3) on `2,4,6,8,4` → `–,–,4,6,5` |
| `EmaFehlerfaelle` | period 0, series too short |
| `MacdReferenzwerte` | MACD(2,3,2) on `2,4,6,8,4,10`: line, signal, histogram incl. fractions like 5/6 |
| `MacdFehlerfaelle` | periods ≤ 0, fast ≥ slow, exactly minimal length (OK), one too short |
| `RsiReferenzwerte` | RSI(3) on `10,11,12,11,13,12` → 66.67, 83.33, 60.61 |
| `RsiSonderfaelle` | only rising prices → 100, constant → 50 |
| `RsiFehlerfaelle` | period 0, `n` prices for RSI(n) too short, `n+1` OK |

The test helper `reiheAus` builds a `Kursreihe` from a list of closing prices (one day each,
starting 2023‑01‑01, with Open = High = Low = Close, so every candle is valid). It sits in an
**anonymous namespace**: `test_kursdaten.cpp` already defines a global `make_time`, and two
global functions with the same name in one test program would cause a linker error.

`EXPECT_NEAR(..., eps)` with `eps = 1e-9` is used instead of `EXPECT_EQ`, because floating-point
results like 1/3 are never exactly equal.

## 6. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 32 of 32 tests pass,
no compiler warnings (`-Wall -Wextra -Wpedantic`).

## 7. Possible exam questions

- *Why `std::optional` instead of 0?* → 0 is a valid value. `nullopt` clearly means "not available".
- *Why is the EMA start value the SMA?* → The EMA needs a previous value. The SMA of the first
  `n` prices is the usual convention.
- *Why `static` for `emaUeber`?* → It only needs its parameters, not the object's data.
- *What is the complexity of the SMA?* → O(N) thanks to the sliding window.
- *Why must fast < slow in the MACD?* → Otherwise the "fast" EMA isn't faster, and the
  difference has no meaning.
- *What happens with constant prices in the RSI?* → 0/0. We define it as 50 (neutral).
