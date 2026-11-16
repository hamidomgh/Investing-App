# F2 – "?" explanations for every number: what was built and why

This file explains feature F2, so you can explain it in the exam. The specification (German) with all decisions is
`Features/F2_Erklaerungen.md`. The code comments and the explanation texts are in German; this text is in English.

## 1. What it does

Next to the name of every indicator, key figure, table column and chart there is a small round **"?"**. When the
mouse **hovers** over it, a window appears with:
1. **was:** what the value is, in everyday language
2. **wie:** how MarketLens calculates it, exactly as the C++ code does, with the current periods
3. **lesen:** how it's usually read
4. **aktuell** (bold): a sentence with the current values, e.g. "Aktuell: RSI 14 am letzten Tag = 48,31."
5. for indicators with a usual reading: *"Übliche Deutung, keine Anlageberatung."*

Decisions (2026-11-16): open on hover; include current values; one "?" per chart; German; investment-advice note; also
explain the drawing tools from F1.

## 2. Where the "?" are (25 entries)

| Area | Keys |
|---|---|
| settings | `sma`, `ema`, `bollinger`, `niveaus`, `pivot`, `zinsmarkierung`, `rsi`, `macd` |
| key figures | `zeitraum`, `handelstage`, `veraenderung`, `volatilitaet`, `drawdown`, `pivot` (3×), `niveaus`, `waehrung` (new key figure) |
| charts | `kursdiagramm`, `rsi`, `macd` (top right of each chart) |
| toolbar (F1) | `zeichenwerkzeuge` |
| event study | `ereignisstudie`, `zentralbank`, `wirksamAb`, `leitzins`, `reaktionstag`, `kursVorher`, `kursNachher`, `veraenderungEreignis` |
| stale-data note | `veraltet` |

`rsi`, `macd`, `pivot` and `niveaus` appear in several places with the **same** text. A key is the single source, so the
texts can't drift apart.

**New key figure "Währung":** shows the currency of the security. Its explanation covers **GBX = pence** (Tesco), so the
"560,20 GBX" in the title isn't misunderstood.

## 3. Files

| File | Change |
|---|---|
| `web/erklaerungen.js` | **new**: all texts (`ERKLAERUNGEN`) and the hover window (`Hilfe`) |
| `web/app.js` | `aktuelleWerte()` provides the placeholder values; "?" in key figures and stale-data note; key figure "Währung"; `Hilfe.init(aktuelleWerte)` at start |
| `web/index.html` | "?" in settings, charts, toolbar, event study; `<script src="erklaerungen.js">` before `app.js` |
| `web/style.css` | round "?" button and the window, light and dark |
| `tests/test_http_server.cpp` | new test `Weboberflaeche.ErklaerungenVollstaendig`; delivery test includes `/erklaerungen.js` |
| `docs/testprotokoll.md` | 16 new manual cases F2-1 … F2-16 |

**No change to the server or the C++ logic.**

## 4. Texts with placeholders

```js
rsi: {
  titel: "RSI – Relative Strength Index",
  was:   "Misst, wie stark die Kursgewinne gegenüber den Verlusten … überwiegen. Werte von 0 bis 100.",
  wie:   "… geglättet nach Wilder: neuer Durchschnitt = (alter Durchschnitt × ({rsi} − 1) + heutiger Wert) ÷ {rsi} …",
  lesen: "Über 70 gilt ein Wert als „überkauft“, unter 30 als „überverkauft“ …",
  aktuell: "Aktuell: RSI {rsi} am letzten Tag = {rsiLetzter}.",
  beratung: true,
},
```
- `{rsi}` etc. are replaced by `fuelle()` with values from `aktuelleWerte()` in `app.js`.
- **Missing values:** in `was`/`wie`/`lesen` a missing value becomes "…". The `aktuell` sentence is **dropped completely** if
  any of its values is missing, e.g. when the indicators failed with 422. So the window never shows "undefined".
- **Periods** come from the server's answer (`parameter`), so they're exactly what was computed. If the indicators couldn't be
  loaded, they come from the input fields instead.

### The texts match the implementation
The texts describe exactly what the C++ code computes (docs E3–E6), not a textbook variant:

| Entry | Detail that matches our code |
|---|---|
| EMA | start value = SMA of the first n prices; α = 2/(n+1), shown as a number (0,0952 for 20) |
| RSI | Wilder smoothing; 100 without losses, 50 for constant prices; no value for the first n days |
| Bollinger | standard deviation divided by **n** (population) |
| Volatility | **sample** standard deviation (n − 1) of log returns × √252; unavailable with < 3 candles |
| Support/Resistance | highs from High, lows from Low; window 5; plateau counts once; edges skipped; tolerance relative to the **level's mean**; level ≤ last close = support |
| MACD | line starts on day *slow*, signal/histogram on day *slow + signal − 1* |
| Rate decisions | the **effective** date (FRED), not the meeting; unchanged decisions invisible; Fed = DFEDTARU upper limit, ECB = deposit rate ECBDFR |
| Fibonacci (F1) | 0 % at the second click, 100 % at the first |

## 5. The hover window (`Hilfe`)

Same module pattern as `Zeichnen` in F1: an IIFE returns `{ init, knopf }`, and the state stays private.

- **One window for all "?"** (`<div id="erklaerung" role="tooltip">` at the end of the page).
- **Event delegation:** the listeners are on `document`, not on each "?". `e.target.closest(".hilfe")` finds the "?".
  "?" buttons that `app.js` creates later (key figures, stale-data note) work automatically, without registering anything.
- **Hover:** `mouseover` on a "?" opens it. `mouseout` from the "?" or the window schedules closing after **200 ms**.
  `mouseover` on the window cancels that, so you can move from the "?" into the window to read it without flicker.
- **Without a mouse** (in addition to hover, not instead): keyboard focus (Tab) opens it, Esc closes it. On touch screens,
  which have no hover, a tap on the "?" opens it and a tap anywhere else closes it. `preventDefault()` on that click stops a
  surrounding `<label>` from toggling its checkbox.
- **Position** (`positioniere`):
  1. First move the window to the top-left and remove any earlier height limit, **then measure** it. Measured at its old
     place (e.g. near the right edge), the browser would make it narrower, the text would wrap differently, and the
     measured height wouldn't match. That was a real bug: the browser test showed windows low on the page being cut off.
  2. **Below** the "?" if it fits, otherwise **above** if it fits.
  3. If it fits neither (a long text near the bottom of a low window): on the side with more room, with `max-height`
     limited to that room; the rest scrolls **inside** the window (`overflow-y: auto`, `overscroll-behavior: contain`, so
     scrolling inside doesn't scroll the page and close the window).
  4. Always at least 8 px from the screen edges. Page coordinates = viewport coordinates + scroll offset. It closes on page
     scroll or resize, because the position would no longer be right.
- **Accessibility:** each "?" is a real `<button>` (reachable with Tab), with `aria-label="Erklärung zu RSI …"` and
  `aria-expanded` true/false.
- **Security:** all text goes in with `textContent`, never `innerHTML` (same rule as E10).

## 6. The new automated test

`Weboberflaeche.ErklaerungenVollstaendig` reads `index.html`, `app.js` and `erklaerungen.js` **as text** with `std::regex`.
There's no browser or JavaScript engine in the container, but the three most likely mistakes can still be caught statically:

1. **Every key used** (`data-hilfe="…"` in the HTML, `Hilfe.knopf("…")` and key-figure rows in `app.js`) **has an entry** in
   `ERKLAERUNGEN`. Otherwise the "?" would show nothing.
2. **Every entry is used.** This catches forgotten or misspelled texts.
3. **Every placeholder `{name}` in the texts is provided by `aktuelleWerte()`.** Otherwise the text would show "…".

The test only searches the `ERKLAERUNGEN` block. On the first run it also flagged `{name}` from a *comment* in the window code,
so it was narrowed to that block. To check it really catches errors, I temporarily misspelled one key (`leitzinss`). The test
failed in both directions ("no entry for 'leitzinss'", "entry 'leitzins' is not used"), and it passed again after restoring.

## 7. Checks

- **Automatic:** 193 of 193 tests pass (1 new). A full rebuild shows 0 warnings. The delivery test now also requires
  `/erklaerungen.js`, loaded before `app.js`.
- **Without a browser:** bracket/string balance of all three JS files; every element id used exists in `index.html`; the real
  server delivers all five web files with the right content type.
- **In a browser (2026-11-16):** cases F2-1 … F2-16 passed except that windows low on the page were sometimes cut off.
  Fixed (section 5, Position); the new case **F2-17** checks exactly that and passed after the fix.

## 8. Possible exam questions

- *Why one text table instead of texts in the HTML?* → One source per explanation; reused in several places; easy to check.
- *How do the texts show current values?* → Placeholders `{name}`, filled from `aktuelleWerte()`; the "aktuell" sentence is dropped if
  a value is missing.
- *What is event delegation?* → One listener on the document uses `closest()` to find the "?"; it also works for elements created later.
- *How do you avoid flicker between the "?" and the window?* → Closing is delayed by 200 ms and cancelled when the mouse enters the window.
- *How does it work without a mouse?* → Focus with Tab opens, Esc closes; tap opens on touch screens.
- *Why `preventDefault()` on the click?* → The "?" sits inside a `<label>`; without it, clicking could toggle the checkbox.
- *How can a C++ test check JavaScript?* → As text: keys and placeholders are compared with `std::regex`; behaviour is checked manually.
- *Why do the texts say "keine Anlageberatung"?* → The usual readings (e.g. RSI > 70 "overbought") are conventions, not recommendations.
