# E6 – Central bank rate decisions and event study: what was built and why

This file explains the E6 implementation, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> Rate decisions of several central banks (central bank, country, date, policy rate) are
> loaded from a web API via `IRateSource` and `IHttpClient` and validated; errors are handled
> as in E2. For each security, the price change on the trading day after each decision of the
> central bank of its country is calculated. Decisions on non-trading days are mapped to the
> next trading day, decisions outside the data period are skipped, and securities without a
> matching central bank return an empty result instead of an error.

## 2. Status: what is done and what is still missing

| Part of the DoD | Status |
|---|---|
| Data model `Zinsentscheidung` (bank, country, date, rate) with validation | **done** |
| Event study: reaction on the next trading day | **done** |
| Non-trading days → next trading day | **done** |
| Decisions outside the data period are skipped | **done** |
| Security without matching central bank → empty result | **done** |
| Loading via `IRateSource` + `IHttpClient` from a web API | **open**, comes with E2 |
| Error handling "as in E2" (network, HTTP status, quota, invalid JSON) | **open**, comes with E2 |

Why the split: loading needs a JSON library and the error classes that E2 defines. Both are
still waiting for decisions (C++ standard, header-only libraries, API provider). The event study
doesn't care where the decisions come from. It takes a `std::vector<Zinsentscheidung>`, so
later an `IRateSource` only has to deliver that vector.

## 3. Files

| File | Purpose |
|---|---|
| `src/zinsentscheidung.hpp` / `.cpp` | Struct `Zinsentscheidung` with validating constructor |
| `src/ereignisstudie.hpp` / `.cpp` | Struct `Ereignis`, class `Ereignisstudie` |
| `tests/test_ereignisstudie.cpp` | 11 tests (3 for the data model, 8 for the event study) |
| `CMakeLists.txt` | Both `.cpp` files added to `projekt_lib`, the test file to `tests` |

## 4. `Zinsentscheidung`

Fields: `zentralbank` (e.g. "Fed"), `land` (e.g. "USA"), `datum` (`std::chrono::sys_days`),
`leitzins` (percent).

Same idea as `Kerze` in E1: the constructor calls `validieren()`, so an invalid decision
can't be created. Rules:
- central bank and country must not be empty
- the rate must be **finite** (`std::isfinite`: not NaN, not infinite). That catches broken
  parsing later in E2.
- **negative rates are allowed**: the ECB had −0.5 % (2019), the Bank of Japan −0.1 %. A check
  like "rate ≥ 0" would be wrong.

Error messages name the bank and the date (`YYYY-MM-DD`), like E1.

**`sys_days` instead of `system_clock::time_point`:** a rate decision is a calendar day, not a
moment in time. `sys_days` (C++20) has no time of day, so comparing days is exact.
The candles still use `time_point` (E1); the event study converts them with
`std::chrono::floor<std::chrono::days>(...)`, which cuts off the time of day.

## 5. `Ereignisstudie`

### 5.1 Usage
```cpp
Ereignisstudie studie(alleEntscheidungen);          // Fed, EZB, BoE, BoJ … together
auto ergebnis = studie.auswerten(wertpapier, reihe); // per security
```
The constructor sorts the decisions by date once (`std::stable_sort`: decisions on the same
day keep their original order). Then the same object can be used for any number of securities.

### 5.2 Matching the central bank
Only decisions with `entscheidung.land == wertpapier.land` are used. No match (e.g. a Japanese
stock when only Fed and ECB decisions are loaded) → **empty list, no error**, as the DoD says.

### 5.3 Finding the reaction day
**Definition:** the reaction day is the **first trading day strictly after** the decision day.
The change is from the close of the trading day before it to the close of the reaction day:

```
veraenderung = close[reaction day] / close[trading day before] − 1
```

Implementation: `std::upper_bound(handelstage, datum)` finds the first trading day `> datum`
with binary search, O(log N).

This one rule covers all cases:

| Decision on | Reaction day | "Before" price |
|---|---|---|
| Wednesday (trading day) | Thursday | Wednesday close |
| Saturday or Sunday | Monday (next trading day) | Friday close |
| Before the first candle | (would be day 0) | none → **skipped** |
| On the last candle or later | none | → **skipped** |

So "non-trading days → next trading day" isn't a special case in the code. It follows from
"first trading day after".

A close of 0 on the "before" day would mean dividing by 0, so it throws `std::invalid_argument`
(same reasoning as the returns in E4).

### 5.4 The `Ereignis` result
Each result contains the decision itself, the reaction day, both prices and the change. That
gives the web UI (E10) everything it needs to show a table or mark the days on the chart.

## 6. Tests

All tests use two real trading weeks: Mon 1 – Fri 5 and Mon 8 – Tue 9 January 2024
(1 January 2024 was a Monday). Closes: 100, 102, 101, 105, 104, 110, 99.

| Test | Checks |
|---|---|
| `Zinsentscheidung.GueltigAuchMitNegativemZins` | normal rate and −0.5 % are accepted |
| `Zinsentscheidung.UngueltigeFelderWerdenAbgelehnt` | empty bank, empty country, NaN, infinity |
| `Zinsentscheidung.FehlermeldungNenntBankUndDatum` | message contains "Fed" and "2024-03-20" |
| `EntscheidungAnHandelstag` | Wed 3 → Thu 4: 105 / 101 − 1 |
| `NichtHandelstagWirdAufNaechstenHandelstagAbgebildet` | Sat 6 and Sun 7 → Mon 8: 110 / 104 − 1 |
| `AusserhalbDesZeitraumsWirdUebersprungen` | before start, on the last day, after the end |
| `EntscheidungAmErstenHandelstag` | Mon 1 → Tue 2: 102 / 100 − 1 = 0.02 |
| `NurZentralbankDesEigenenLandes` | US stock only gets Fed; Japanese stock → empty |
| `ErgebnisIstNachDatumSortiert` | unsorted input → sorted output |
| `LeereEingaben` | no prices or no decisions → empty |
| `KursNullWirdAbgelehnt` | close 0 before the reaction day → error |

## 7. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 73 of 73 tests pass
(11 new), no compiler warnings.

## 8. Open point for later: the euro area

Matching works by **country**, as `PROJEKT.md` says. But the ECB is responsible for many
countries. A German stock has `land = "Deutschland"`, an ECB decision `land = "Eurozone"`,
so they wouldn't match. Options for E2, when the catalog is designed:
1. match by **currency** instead (EUR → ECB, USD → Fed, GBP → BoE, JPY → BoJ), or
2. give each central bank a list of countries in the catalog.

Option 1 is simpler and fits the data, because every security in the catalog already has a
currency. This would need a small change to `PROJEKT.md` (E6 says "country"), so it's your
decision.

## 9. Possible exam questions

- *Why is a Saturday decision not a special case in the code?* → The rule "first trading day
  strictly after the decision" already gives Monday.
- *Why `upper_bound` and not `lower_bound`?* → `lower_bound` finds the first day `>=` the
  decision, which would be the decision day itself. We want the day **after**, so `>`.
- *Why is a security without a central bank not an error?* → It's a normal case (e.g. a country
  whose central bank isn't loaded). The UI just shows "no events".
- *Why are negative rates allowed?* → They really happened (ECB, BoJ, SNB).
- *Why `sys_days` for the decision?* → A decision is a calendar day; no time-of-day problems
  when comparing.
- *Why `stable_sort`?* → Decisions on the same day keep their input order, so the output is
  deterministic.
