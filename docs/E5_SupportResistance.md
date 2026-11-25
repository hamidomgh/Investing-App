# E5 – Support/resistance levels and pivot points: what was built and why

This file explains the E5 implementation, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> Local highs and lows are detected with an adjustable window (edges without a full window
> and flat plateaus handled correctly). Nearby prices are combined into levels with a touch
> count within a relative tolerance, and each level is classified as support or resistance
> relative to the last close and sorted ascending. Classic pivot points (P, R1, R2, S1, S2)
> match hand-calculated values.

## 2. Files

| File | Purpose |
|---|---|
| `src/analyse/supportresistance.hpp` | Declaration: class `SupportResistance`, structs `Extremum`, `Niveau`, `PivotPunkte`, enums `ExtremumArt`, `NiveauArt` |
| `src/analyse/supportresistance.cpp` | Implementation: extrema detection, clustering, pivot points |
| `tests/test_supportresistance.cpp` | 16 tests with hand-checked expectations, special cases and errors |
| `CMakeLists.txt` | New `.cpp` added to `projekt_lib`, new test file to `tests` |

## 3. Overview: three steps

```
Kursreihe ──► lokaleExtrema(fenster) ──► niveaus(fenster, toleranz)
                 highs & lows               sort prices, cluster,
                 (index, price, type)       classify vs. last close

Kerze ──────► pivotAus(kerze)  /  pivotPunkte()  (last candle)
```

The same pattern as E3/E4: `explicit` constructor, one method per result,
`std::invalid_argument` for invalid input.

**`enum class`** instead of a plain `enum` or a string: `ExtremumArt::Hoch` and
`NiveauArt::Support` are type-safe. You can't mix up the two enums or compare them with an
integer by accident.

## 4. Local highs and lows (`lokaleExtrema(fenster)`)

### 4.1 Rule
A day is a **local high** if the `fenster` days before it **and** the `fenster` days after it
all have a **lower** High. A **local low** is the same with the Low values and "higher".

- Highs are searched on the **High** values, lows on the **Low** values (not on Close), because
  those are the real turning points of the price.
- Result: a list of `Extremum {index, preis, art}`, sorted by index (time order).

### 4.2 Edges
A day needs `fenster` days on **both** sides. The first and last `fenster` days can't have a
full window, so they are skipped. Reason: we don't know what comes after the last day, so we
can't say it's a peak yet.

### 4.3 Plateaus (the tricky part)
Problem: `1, 3, 3, 3, 1`. Is every 3 a high? With a naive "≥ all neighbours" rule you get 3
highs; with "> all neighbours" you get none. Both are wrong: there is **one** peak.

Solution in `findeMaxima`: treat a run of equal values (`a..b`) as **one point**.
1. Find the plateau end `b` (while the next value equals `werte[a]`).
2. Check that the `fenster` values **before `a`** and **after `b`** are all strictly lower.
3. If so, report **one** extremum at index `a` (the first day of the plateau).
4. Continue at `b + 1`.

This also handles `1, 3, 3, 4, 2`: the plateau `3, 3` is followed by 4, so it isn't a peak.
Only the 4 is.

### 4.4 Lows reuse the high search
`findeMaxima` only finds maxima. For lows it's called with the **negated** Low values: the
minimum of `x` is the maximum of `−x`. One algorithm, no duplicated code.

Complexity: O(N · fenster).

## 5. Levels (`niveaus(fenster, toleranz)`)

1. Get all extrema and take their prices (highs and lows together, because a former
   resistance can become support later).
2. **Sort** the prices ascending, so close prices are next to each other.
3. Walk through them and build clusters: a price joins the current level if
   `|price − mean| ≤ toleranz · |mean|`, where `mean` is the current average of the level.
   Otherwise the level is closed and a new one starts.
4. Each level stores `preis` (the mean) and `beruehrungen` (how many extrema it contains).
5. **Classification:** `preis ≤ last close` → **Support**, otherwise **Resistance**.
   A level exactly at the last close counts as support (our definition: the price is sitting on it).
6. Because the prices were processed in sorted order, the levels are already **sorted ascending**.

`toleranz` is **relative** (0.02 = 2 %), so the same value works for a stock at 10 € and an
index at 18 000 points. With `toleranz = 0` only exactly equal prices are combined.

No extrema (constant prices, empty or too short series) → empty list, no error.

Complexity: O(E log E) for sorting the E extrema.

## 6. Pivot points

Classic formulas from High `H`, Low `L` and Close `C` of one candle (usually the previous
day, so they act as levels for "today"):

| Value | Formula |
|---|---|
| P | (H + L + C) / 3 |
| R1 | 2P − L |
| S1 | 2P − H |
| R2 | P + (H − L) |
| S2 | P − (H − L) |

- `pivotAus(kerze)` is `static`: it only needs one candle, not the object.
- `pivotPunkte()` uses the **last** candle of the series. An empty series throws.

## 7. Tests

| Test | Checks |
|---|---|
| `ExtremaFensterEins` | `1,3,2,5,4`, window 1: highs at 1 and 3, low at 2, sorted by time |
| `ExtremaFensterZwei` | `1,2,5,3,2,4,6`, window 2: high at 2, low at 4 |
| `ExtremaRaenderWerdenUebersprungen` | `9,5,7,5,1`: the 9 and the 1 at the edges don't count |
| `ExtremaPlateauZaehltEinmal` | `1,3,3,3,1` → one high at index 1 |
| `ExtremaPlateauOhneAbfallIstKeinHoch` | `1,3,3,4,2` → only the 4 |
| `ExtremaPlateauBeiTiefs` | `5,2,2,5` → one low at index 1 |
| `ExtremaSonderfaelle` | constant prices, empty series, window too large → no extrema |
| `ExtremaFehlerfaelle` | window 0 and negative |
| `NiveausReferenzwerte` | `10,20,11,20.2,10.1,15`, 2 %: levels 10.1 (S), 11 (S), 20.1 (R, 2 touches) |
| `NiveausGroessereToleranz` | same series, 10 %: levels 10.55 and 20.1, each with 2 touches |
| `NiveausToleranzNull` | only exactly equal prices are merged |
| `NiveauAufLetztemKursIstSupport` | level = last close → support |
| `NiveausOhneExtremaSindLeer` | constant or empty series → empty list |
| `NiveausFehlerfaelle` | negative tolerance, window 0 |
| `PivotReferenzwerte` | H 110, L 90, C 105 → P 101.667, R1 113.333, S1 93.333, R2 121.667, S2 81.667 |
| `PivotAusLetzterKerze` | uses the last candle; empty series throws |

**A note on honesty:** in my first run, `ExtremaRaenderWerdenUebersprungen` failed. I had
wrongly expected index 3 in `9,5,7,5,1` to be a low, but its right neighbour (1) is lower, so
it isn't. The code was right and my expectation was wrong, so I corrected that **new** test.
No existing test was changed.

## 8. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 62 of 62 tests pass
(16 new), no compiler warnings.

## 9. Possible exam questions

- *How do you handle a flat top?* → The run of equal values is treated as one point; its
  neighbours before and after the run must be lower. Reported once, at the first day.
- *Why skip the edges?* → Without a full window we don't know yet whether the price will
  continue past that point.
- *How do you find lows without writing a second algorithm?* → Negate the Low values: a
  minimum of x is a maximum of −x.
- *Why a relative tolerance?* → So the same setting works at any price level.
- *Why are levels sorted without an extra sort?* → The prices are sorted before clustering,
  and clusters are created in that order.
- *Why are highs and lows combined into the same levels?* → A price area that was resistance
  can later act as support ("role reversal"). What matters is how often the price reacted there.
- *Why `enum class`?* → Type-safe: no mixing of the two enums, no implicit conversion to int.
