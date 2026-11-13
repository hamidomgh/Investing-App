# E10 – Web interface: what was built and why

This file explains the E10 implementation, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`)

> A page delivered by the server lets you select a security and shows the price chart, selected indicators,
> support/resistance lines and the event study. Error messages of the interface are shown understandably, and the page works
> without external services (all scripts are in the repository). It is checked with a manual test protocol.

| DoD point | Where |
|---|---|
| page delivered by the server | `web/` is mounted at `/` by `HttpServer` (E8); test `Weboberflaeche.WirdVomServerAusgeliefert` |
| select a security | `<select id="symbol">`, filled from `/api/wertpapiere` |
| price chart, selected indicators, S/R lines | SVG candlestick chart with SMA, EMA, Bollinger, levels, pivot points; RSI and MACD below; on/off switches |
| event study | vertical markers in the chart + table |
| errors shown understandably | red box: area + plain-language explanation per status + the server's exact message |
| no external services | plain HTML/CSS/JS, charts drawn by our own code; test `Weboberflaeche.KeineExternenAdressen` |
| manual test protocol | `docs/testprotokoll.md`, 20 cases, **to be carried out by you in a browser** |

## 2. Files

| File | Purpose |
|---|---|
| `web/index.html` | structure of the page: header with selection, settings, three charts, key figures, event table |
| `web/style.css` | layout (CSS grid), light and dark colour scheme via CSS variables |
| `web/app.js` | all logic: loading, drawing the SVG charts, error display |
| `docs/testprotokoll.md` | manual test protocol |
| `tests/test_http_server.cpp` | **2 new tests** for the web folder |
| `CMakeLists.txt` | constant `WEB_DIR` for those tests |

No build step, no library, no CDN. The browser loads exactly three files from our own server.

## 3. How the page works

```
start()  ──► GET /api/wertpapiere ──► fill <select> ──► lade()
lade()   ──► three requests AT THE SAME TIME (Promise.allSettled):
               GET /api/kurse          ?symbol&von&bis
               GET /api/indikatoren    ?symbol&von&bis&sma&ema&rsi&macd&bollinger
               GET /api/ereignisstudie ?symbol&von&bis
         ──► zustand = {kurse, indikatoren, ereignisse}   (null for each that failed)
         ──► zeigeFehler, zeigeHinweis, zeigeTitel, zeichne, zeigeKennzahlen, zeigeEreignisse
```

### 3.1 Parallel requests, partial results
`Promise.allSettled` waits for all three and reports **each one's** success or failure. With `Promise.all`, one 422 for the
indicators would throw away the prices too. This way the page shows what worked plus a message for what didn't. For example,
with a range that's too short you still see the candles and the event study, and only "Indikatoren: zu wenige Kursdaten …".

The three requests are processed in parallel by the server's thread pool (E9).

### 3.2 Out-of-date answers are discarded
If you click "Anzeigen" twice quickly, the first answers could arrive **after** the second ones and overwrite them. Each call
of `lade()` gets a number (`ladeNummer`). Only answers carrying the latest number are shown.

### 3.3 Switching on/off without a new request
The checkboxes only call `zeichne()`, which redraws from `zustand`. Changing a **period** needs new numbers from the server, so
that happens on "Anzeigen". The page says so ("Perioden wirken nach „Anzeigen“, Ein/Aus sofort").

## 4. Errors

`holeJson` turns every problem into an `ApiFehler {status, message}`:

| Situation | `status` | Explanation shown (`erklaere`) |
|---|---|---|
| server not reachable (`fetch` throws) | 0 | "Keine Verbindung zum Server. Läuft MarketLens (./build/projekt)?" |
| 400 | 400 | "Ungültige Eingabe. Bitte die Einstellungen prüfen." |
| 404 | 404 | "Nicht gefunden." |
| 422 | 422 | "… zu wenige Kursdaten (z. B. Zeitraum verlängern oder Periode verkürzen)." |
| 502 | 502 | "Die Datenquelle liefert für dieses Wertpapier keine Daten." |
| 503 | 503 | "Das Anfragekontingent … ist erschöpft. Bitte später erneut versuchen." |
| other | … | "Unerwarteter Fehler auf dem Server." |

Each message shows **area + explanation** in bold, followed by the server's exact text and HTTP status, e.g.
**Indikatoren: Ungültige Eingabe. Bitte die Einstellungen prüfen.** (HTTP 400: parameter 'macd': fast period must be smaller than slow period).
Users get a plain sentence, and you still see exactly what the server said.

Stale data (E7): if an answer has `"veraltet": true`, a yellow note shows the cache's `hinweis` ("Stale data from …").

### Security: `textContent`, never `innerHTML`
Every text that comes from the server (names, messages) is inserted with `textContent` (or `createElement` + `textContent`).
`innerHTML` would interpret the text as HTML. A catalog name like `<img src=x onerror=…>` could then run code in the browser
(**cross-site scripting**). `app.js` never uses `innerHTML`.

## 5. Drawing the charts (SVG, own code)

**Why SVG instead of a chart library:** the DoD forbids external services, and a library copied into the repo would be a new
dependency. SVG is part of every browser. Lines, rectangles and text are elements you create with
`document.createElementNS(SVG_NS, "line")`. The browser handles scaling and sharp rendering.

### 5.1 Coordinates
- `rahmen(svg, n)` clears the SVG, reads its real size, and sets `viewBox` so that 1 unit = 1 pixel. It returns `x(i)`, the
  centre of day `i`. **All three charts use the same left/right margins**, so day `i` is at the same x position in every chart and
  the RSI/MACD line up exactly under the candles.
- `y(v)` maps a price to a pixel: a high price means a small y (top).
- `bereich(werte)`: min/max of all **visible** values (candles + switched-on lines + levels + pivots) with 5 % headroom. Constant
  prices are widened by ±1, so there's no division by 0.
- `schoeneTicks`: tick spacing of 1, 2 or 5 × a power of ten (e.g. 228, 230, 232 … instead of 228.37 …).

### 5.2 Elements
| Element | How |
|---|---|
| candle | `line` from high to low (wick) + `rect` from open to close (body, at least 1 px); green if close ≥ open |
| SMA, EMA | one `path`; `null` (warm-up) **breaks** the line (`M` starts a new piece, `L` continues) |
| Bollinger | a `polygon`: upper band left→right, then lower band right→left = a closed, semi-transparent area; dashed middle line |
| support/resistance | horizontal dashed `line` across the width; label on the right "S 228,18 (2×)" (green below the last close, red above) |
| pivot points | dotted lines in the last quarter (they are levels for the *next* day), labels P, R1, R2, S1, S2 |
| rate decision | vertical dashed line on the **reaction day**, label "Fed 4,00 %" |
| RSI | fixed scale 0–100, grid at 30/50/70 |
| MACD | histogram bars from 0 (green/red), MACD line and signal line |

### 5.3 Aligning by date, not by position
The indicators come in their own request. `ausrichten(daten, werte, indexNachDatum, n)` puts each value at the position of the
candle **with the same date**. Today both answers have the same dates, but the chart doesn't silently depend on that.

### 5.4 Crosshair
A transparent `rect` on top receives mouse events. On `mousemove`, the mouse x position gives the day index. A vertical line
moves there, and the info box shows date, O/H/L/C, SMA, EMA and RSI of that day.

### 5.5 Redrawing
Charts are redrawn after loading, on every switch, and on window resize. `requestAnimationFrame` makes sure resize redraws at most
once per frame.

## 6. Layout and colours (`style.css`)

- **CSS grid**: settings (220 px) left, charts right. Key figures and table go over the full width. Below 760 px everything stacks.
- **CSS variables** (`--sma`, `--steigt` …) hold all colours. `@media (prefers-color-scheme: dark)` swaps them for a dark scheme.
  The SVG elements use `style="stroke:var(--sma)"`, so the charts switch too.
- Numbers use `font-variant-numeric: tabular-nums` (all digits the same width), so columns line up.
- The table sits in a box with `overflow-x: auto`: on a phone it scrolls sideways instead of widening the page.

## 7. What was checked, and what you need to check

**Automatically** (190 of 190 tests pass, 2 new):
- `Weboberflaeche.WirdVomServerAusgeliefert`: `/` → `text/html` with `src="app.js"` and `href="style.css"`; `/app.js` → `javascript`;
  `/style.css` → `text/css`; API alongside.
- `Weboberflaeche.KeineExternenAdressen`: no `http://`, `https://` or `"//` in any file of `web/`, except the SVG namespace identifier
  `http://www.w3.org/2000/svg`. That string is only a name, and nothing is loaded from it.

**By me, without a browser** (the container has no browser and no Node.js):
- the real server delivers all three files with the right content type, and all API calls the page makes answer 200 for IBM;
- a script checked that all brackets, strings and template literals in `app.js` are balanced;
- every `el("…")` id used in `app.js` exists in `index.html`, and vice versa.

**By you, in a browser:** `docs/testprotokoll.md`, 20 cases including all error types and "no external requests" in the browser's
network tab. Only that shows that the page really renders and behaves as described. Please fill in the result column and the
date/browser line.

## 8. Possible exam questions

- *Why no chart library?* → The DoD requires everything in the repo without external services; SVG is built into every browser and
  our drawing code is short enough to explain.
- *Why `Promise.allSettled` and not `Promise.all`?* → One failing request (e.g. 422 for indicators) shouldn't hide the parts that worked.
- *How do you prevent old answers from overwriting new ones?* → Each load gets a number; only answers with the latest number are shown.
- *Why `textContent` instead of `innerHTML`?* → Server text must never be interpreted as HTML (cross-site scripting).
- *How do the RSI and MACD line up with the candles?* → Same left/right margins and the same `x(i)` in all charts; values aligned by date.
- *How is a gap in the SMA drawn?* → `null` values end the current piece of the path; the next value starts a new one with `M`.
- *How does the dark mode work?* → All colours are CSS variables, redefined in `@media (prefers-color-scheme: dark)`.
- *How is "no external services" checked?* → Automatically by a test that scans `web/` for addresses, and manually in the browser's network tab.
