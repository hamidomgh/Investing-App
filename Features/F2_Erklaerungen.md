# F2 – Erklärungen („?“) zu allen Zahlen und Indikatoren

## 1. Ziel

Neben dem Namen jeder Zahl und jedes Indikators der Weboberfläche steht ein kleines „?“. Fährt man mit der Maus
darüber, erscheint eine kurze Erklärung: **was** der Wert ist, **wie** er berechnet wird und **wie** man ihn liest.
So versteht auch jemand ohne Vorwissen die Seite.

## 2. Umfang

Erklärungen für alle folgenden Stellen (Stand der Weboberfläche nach E10):

| Bereich | Einträge |
|---|---|
| Einstellungen | SMA, EMA, Bollinger-Bänder, Support/Resistance, Pivot-Punkte, Zinsentscheidungen, RSI, MACD, sowie die Eingabefelder „Periode“, „Periode,k“ und „schnell,langsam,signal“ |
| Kennzahlen | Zeitraum, Handelstage, Veränderung im Zeitraum, Volatilität (annualisiert), Maximaler Drawdown, Pivot P, R1/R2, S1/S2, Niveaus (S/R) |
| Kursdiagramm | Kerze (Open, High, Low, Close; grün/rot), Beschriftung „S …/R … (n×)“ der Niveaus, Markierung der Zinsentscheidung |
| Unterdiagramme | RSI (Linien 30/50/70), MACD (Linie, Signal, Histogramm) |
| Ereignisstudie | Zentralbank, Wirksam ab, Leitzins, Reaktionstag, Kurs vorher, Kurs nachher, Veränderung |
| Hinweise | „veraltete Daten“ (Cache, E7), Währung GBX (Pence) |

**Gehört nicht dazu:** Anlageempfehlungen, Kauf-/Verkaufssignale, Erklärungen in anderen Sprachen.

## 3. Inhalt einer Erklärung

Jede Erklärung hat dieselben drei Teile, kurz (zusammen etwa 3–6 Sätze):

1. **Was ist das?** – ein Satz in Alltagssprache.
2. **Wie wird es hier berechnet?** – die Formel bzw. Regel **so, wie MarketLens sie tatsächlich rechnet**, mit
   den aktuell gewählten Perioden (z. B. „SMA 20 = Durchschnitt der letzten 20 Schlusskurse“).
3. **Wie liest man es?** – die übliche Deutung, z. B. RSI über 70 gilt als „überkauft“; ausdrücklich als übliche
   Deutung formuliert, nicht als Empfehlung.

Wo es hilft, enthält die Erklärung den **aktuellen Wert**, z. B. „Der maximale Drawdown von −32,78 % bedeutet:
Vom höchsten Kurs im Zeitraum ging es zwischendurch um 32,78 % nach unten.“

Die Texte müssen genau zur Implementierung passen (Quelle: `docs/E3_…` bis `docs/E6_…`), insbesondere:

| Eintrag | Was die Erklärung sagen muss |
|---|---|
| EMA | Startwert ist der SMA der ersten n Kurse; α = 2/(n+1) |
| RSI | Glättung nach Wilder; bei konstanten Kursen 50, ohne Verluste 100 |
| Bollinger | Mitte = SMA; Breite = k Standardabweichungen (geteilt durch n) |
| Volatilität | Standardabweichung der Log-Renditen (geteilt durch n − 1) × √252 |
| Max. Drawdown | größter Rückgang vom bisherigen Höchststand, als Anteil |
| Support/Resistance | lokale Hochs/Tiefs mit Fenster 5, zusammengefasst mit 2 % Toleranz; Support ≤ letzter Schlusskurs; „n×“ = Anzahl Berührungen |
| Pivot-Punkte | P = (H + L + C)/3 der letzten Kerze, R1 = 2P − L, S1 = 2P − H, R2 = P + (H − L), S2 = P − (H − L); gelten für den nächsten Handelstag |
| Zinsentscheidung | Datum = Tag der **Wirksamkeit** laut FRED, nicht der Sitzungstag; unveränderte Zinsen sind nicht sichtbar |
| Veränderung (Ereignis) | Schlusskurs am ersten Handelstag nach der Wirksamkeit gegenüber dem Vortag |
| Einschwingphase | warum Linien erst nach n Tagen beginnen |

## 4. Verhalten im Detail

- **Das „?“** ist ein kleiner runder Knopf (`<button>`) direkt hinter dem Namen, nicht hinter der Zahl selbst,
  damit Zahlenspalten bündig bleiben. Im Diagramm (SVG) gibt es kein „?“ an jeder Linie; stattdessen ein „?“
  pro Diagramm, das die Elemente dieses Diagramms erklärt.
- **Öffnen (entschieden 2026-11-16): beim Überfahren mit der Maus.** Das Erklärungsfenster erscheint neben dem
  „?“, solange die Maus darüber (oder über dem Fenster selbst) ist, und verschwindet kurz nach dem Verlassen.
  Eine kleine Verzögerung beim Schließen (etwa 200 ms) verhindert Flackern, wenn die Maus vom „?“ ins Fenster wechselt.
  Es ist immer höchstens eine Erklärung offen.
- **Ersatz ohne Maus** (ergänzt das Überfahren, ersetzt es nicht): Erreicht man das „?“ mit der Tab-Taste, öffnet
  es sich ebenfalls, Esc schließt es. Auf Touch-Geräten, die kein Überfahren kennen, öffnet Antippen das Fenster und
  Antippen außerhalb schließt es. So bleibt die Erklärung für alle erreichbar.
- **Barrierefreiheit:** `aria-label="Erklärung zu RSI"`, `aria-expanded`, Fokus bleibt bedienbar.
- **Platz:** Das Fenster bleibt im sichtbaren Bereich (auch am rechten Rand und auf schmalen Bildschirmen).
- **Eine Quelle für alle Texte:** ein Objekt `ERKLAERUNGEN` in einer eigenen Datei `web/erklaerungen.js`.
  Einträge können Platzhalter für aktuelle Werte enthalten (z. B. `{sma}` für die Periode).
- **Sicherheit:** Texte werden mit `textContent` eingefügt (wie in E10), nie mit `innerHTML`.

## 5. Betroffene Dateien

| Datei | Änderung |
|---|---|
| `web/erklaerungen.js` | **neu:** alle Texte |
| `web/app.js` | „?“-Knöpfe erzeugen, Popover öffnen/schließen, Platzhalter füllen |
| `web/index.html` | „?“ in den Einstellungen und Tabellenköpfen; `<script src="erklaerungen.js">` |
| `web/style.css` | Knopf und Popover, helles und dunkles Schema |
| `docs/testprotokoll.md` | neue Testfälle (Abschnitt 7) |

**Server und C++: keine Änderung.** Alle benötigten Werte (Perioden, Kennzahlen) liefert die Schnittstelle schon.

## 6. Offene Entscheidungen

| Nr. | Frage | Empfehlung |
|---|---|---|
| 1 | Öffnen per Klick oder beim Überfahren? | **Entschieden (2026-11-16): beim Überfahren mit der Maus**; zusätzlich Tab-Fokus und Antippen als Ersatz ohne Maus |
| 2 | Aktuellen Wert in die Erklärung einbauen? | Ja, wo es sinnvoll ist |
| 3 | Ein „?“ pro Diagramm oder pro Linie? | Pro Diagramm (sonst wird das Diagramm unübersichtlich) |
| 4 | Sprache der Texte | Deutsch, wie die Oberfläche |
| 5 | Hinweis „keine Anlageberatung“? | Ja, einmal am Ende jeder Erklärung zu Indikatoren mit üblicher Deutung |

## 7. Prüfung

- **Automatisch:** `Weboberflaeche.KeineExternenAdressen` deckt `erklaerungen.js` mit ab;
  `Weboberflaeche.WirdVomServerAusgeliefert` wird um `erklaerungen.js` erweitert.
  Zusätzlich ein Test, der prüft, dass jeder Schlüssel, den `app.js`/`index.html` verwendet, in `ERKLAERUNGEN`
  vorkommt und umgekehrt (als einfacher Textvergleich in C++, analog zum Test auf externe Adressen).
- **Manuell (neue Fälle im Testprotokoll):** Überfahren jedes „?“ öffnet die passende Erklärung, Verlassen schließt sie
  (ohne Flackern beim Wechsel ins Fenster); Tab-Fokus öffnet, Esc schließt; Antippen auf einem Touch-Gerät; aktuelle Werte und Perioden
  stimmen mit der Anzeige überein (z. B. SMA auf 5 stellen → Erklärung nennt 5); nur eine Erklärung gleichzeitig; Fenster bleibt am rechten Rand sichtbar;
  schmaler Bildschirm; dunkles Schema.
