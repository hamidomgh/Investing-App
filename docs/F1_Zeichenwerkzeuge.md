# F1 – Drawing tools in the price chart: what was built and why

This file explains feature F1, so you can explain it in the exam. The specification (German) with all decisions is
`Features/F1_Zeichenwerkzeuge.md`. The code comments are in German; this text is in English.

## 1. What it does

A toolbar above the price chart: **Auswählen**, **Trendlinie**, **Strahl** (ray), **Waagrechte** (horizontal line),
**Zone** (rectangle), **Fibonacci** (retracement), a **Magnet** switch, and **Löschen**, **Rückgängig** (undo) and
**Alle löschen**. Drawings are stored in the browser per security and survive a page reload.

Decisions (2026-11-16): stored only in the browser; these five tools; magnet on by default; Fibonacci levels 0, 23.6,
38.2, 50, 61.8, 78.6, 100 %; only in the price chart (not RSI/MACD); on touch devices tapping works like clicking.

## 2. Files

| File | Change |
|---|---|
| `web/zeichnen.js` | **new**: all drawing logic (state, tools, rendering, hit test, undo, storage) |
| `web/app.js` | small hooks: passes the chart's coordinate conversion to `Zeichnen`, forwards mouse events, loads drawings per security |
| `web/index.html` | toolbar; `<script src="zeichnen.js">` **before** `app.js` |
| `web/style.css` | toolbar, colour `--zeichnung` (light and dark) |
| `tests/test_http_server.cpp` | delivery test also checks `/zeichnen.js` and that it's loaded before `app.js` |
| `docs/testprotokoll.md` | 16 new manual cases F1-1 … F1-16 |

**No change to the server or the C++ logic.** All 192 tests still pass.

## 3. Structure of `zeichnen.js`

```js
const Zeichnen = (() => {
  const z = { werkzeug, zeichnungen, angefangen, vorschau, ausgewaehlt, verlauf, symbol, … };   // private state
  function punktAus(mx, my) { … }   // private helpers
  …
  return { init, setzeWertpapier, zeichne, bewege, verlasse, klick, aktiv };                    // public interface
})();
```
This is the **module pattern** with an immediately invoked function (IIFE). Only the returned object `Zeichnen` is
visible to `app.js`. The state and helpers are private, the same idea as `private` members of a C++ class. `app.js` only
talks to the seven public functions.

### 3.1 Working with `app.js`
```
app.js zeichneKurschart()
   ├─ draws candles and indicators, computes y(price) and its inverse
   ├─ Zeichnen.zeichne(svg, kontext)      ← kontext = { r, y, preisAusY, kerzen, indexNachDatum }
   └─ fadenkreuz(...)                     ← transparent rect on top receives the mouse
          mousemove  → Zeichnen.bewege(mx, my)   (preview)
          click      → Zeichnen.klick(mx, my)    (set point / select)
          mouseleave → Zeichnen.verlasse()
```
`preisAusY` is the inverse of `y`: `y(v) = top + (ymax − v)/(ymax − ymin) · height`, so
`v = ymax − (py − top)/height · (ymax − ymin)`.

The drawings sit in their own SVG group `<g>`. During drawing, only this group is redrawn on every mouse move, not the whole
chart, which keeps it fast. The group is clipped to the plot area (`clipPath`), so a ray or a zone never runs over the axes.

## 4. Data coordinates, not pixels

Every point is stored as **(date, price)**, e.g. `{ datum: "2026-09-17", preis: 237.75 }`.
- **Window resize:** pixels change, dates and prices don't. On redraw, `pixel(p)` converts with the current `x(i)` and `y(v)`.
- **Different time range:** a point whose date isn't in the shown range has no position, so that drawing is hidden. A
  horizontal line only needs a price and is always shown. Clearing the range brings everything back.
- **x snaps to trading days:** a click is rounded to the nearest candle. There are no values between two trading days.

### Magnet
With the magnet on, the clicked price snaps to the nearest of the candle's **open, high, low, close**, if it's within 15 px.
Otherwise the exact mouse price is used. That's how you draw a trend line exactly through two lows.

## 5. The tools

| Tool | Points | Drawn as |
|---|---|---|
| Trendlinie | 2 | segment between the points |
| Strahl | 2 | from point 1 through point 2 to the edge of the plot area (`strahlEnde`: same line equation, x = edge) |
| Waagrechte | 1 | line across the full width, price label on the right |
| Zone | 2 | rectangle between two opposite corners, 12 % filled |
| Fibonacci | 2 | 7 horizontal lines from the left point to the right edge, labelled "61,8 %  219,10" |

**Fibonacci formula** (`fibPreise`): with first click p1 and second click p2,
`price(s) = p2.preis + (p1.preis − p2.preis) · s` for s ∈ {0, 0.236, 0.382, 0.5, 0.618, 0.786, 1}.
So 0 % is at the second point and 100 % at the first, as in common charting programs. Drawing from a low to a high therefore
shows the usual "retracement" levels below the high.
Example: low 200, high 250 → 61.8 % = 250 + (200 − 250) · 0.618 = **219.10**, 50 % = 225.00, 38.2 % = 230.90.

**Drawing flow:** after the first click, a dashed preview follows the mouse. The second click completes the drawing, and the
tool switches back to "Auswählen", as in common charting programs. **Esc** cancels at any time.

## 6. Selecting, deleting, undo

- **Hit test** (`trefferTest`): in "Auswählen" mode a click selects the drawing within 6 px. Distance to a segment is computed
  by projecting the point onto it (`abstandZuStrecke`, parameter t clamped to [0, 1]). A zone counts if the click is inside it,
  and Fibonacci counts if the click is near any level line. If drawings overlap, the topmost (last drawn) one wins.
- **Delete:** the Entf/Rücktaste key or the button. Key presses inside input fields are ignored, so Backspace in the "SMA" field
  deletes a digit, not your drawing.
- **Undo:** before every change (add, delete, delete all) the current list is saved as a JSON string on a stack (`verlauf`,
  max 50). Strg+Z (Cmd+Z on a Mac) restores the last one. A JSON string is a **real copy**; pushing the array itself would only
  store a reference that later changes would modify too. Undo is per security and is reset when you switch.
- **Delete all** has an **in-page confirmation**: the first click turns the button into "Wirklich alle löschen?", a second
  click within 4 s deletes, and otherwise it resets. Undo can bring everything back.

## 7. Storage in the browser

- `localStorage` key `marketlens.zeichnungen.<SYMBOL>`. Value: a JSON list of `{typ, punkte}` (without the internal ids).
- **Every access is in `try/catch`.** In private windows or with blocked site data, `localStorage` can throw. Then the
  tools still work, but nothing is saved, and the toolbar says "Browser-Speicher nicht verfügbar …".
- **Loaded data is validated** (`istGueltig`): known type, correct number of points, date `YYYY-MM-DD`, finite price.
  Broken or edited entries are dropped instead of breaking the page. That's the same principle as the cache in E7.
- Storage is per browser and per computer. That was the decision; sharing would need a server endpoint.

## 8. Checks

- **Automatic:** `Weboberflaeche.WirdVomServerAusgeliefert` now also requests `/zeichnen.js` (200, JavaScript) and checks it's
  loaded before `app.js`. `Weboberflaeche.KeineExternenAdressen` covers the new file automatically (4 files now).
- **Without a browser** (none in the container): a script checked bracket/string/template balance of `zeichnen.js` and
  `app.js`, and that every element id they use exists in `index.html`.
- **In a browser (2026-11-16):** all 16 cases **F1-1 … F1-16** of `docs/testprotokoll.md` passed, including the Fibonacci hand calculation.

## 9. Possible exam questions

- *Why store (date, price) instead of pixels?* → Pixels change with window size and range; dates and prices don't.
- *How do you convert a mouse position into a price?* → The inverse of the y mapping: `ymax − (py − top)/height · (ymax − ymin)`.
- *What is the module pattern here?* → An IIFE returns an object with the public functions; state stays private, like a class.
- *How is a click on a line detected?* → Distance from the point to the segment via projection, threshold 6 px.
- *Why save a JSON string for undo?* → It's a deep copy; the array itself would be a reference that later changes modify.
- *What happens if localStorage is blocked?* → `try/catch`; everything works, just unsaved, with a visible note.
- *How is the Fibonacci level computed?* → `p2 + (p1 − p2) · s`; 0 % at the second click, 100 % at the first.
