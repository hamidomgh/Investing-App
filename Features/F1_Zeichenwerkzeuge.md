# F1 – Zeichenwerkzeuge im Kursdiagramm

## 1. Ziel

Der Benutzer kann im Kursdiagramm selbst zeichnen, wie in der technischen Analyse üblich: Trendlinien,
waagrechte Linien, Zonen und Fibonacci-Retracements. Die Zeichnungen bleiben erhalten, wenn er das Fenster
verändert, Indikatoren umschaltet oder die Seite später wieder öffnet.

## 2. Umfang

**Gehört dazu (bestätigt am 2026-11-16):**

| Werkzeug | Bedienung | Ergebnis |
|---|---|---|
| Trendlinie | zwei Klicks: Start- und Endpunkt | gerade Linie zwischen zwei Punkten (Datum, Preis) |
| Strahl | zwei Klicks | wie Trendlinie, aber bis zum rechten Rand verlängert |
| Waagrechte Linie | ein Klick | Linie über die ganze Breite bei diesem Preis, rechts mit Preis beschriftet |
| Zone (Rechteck) | zwei Klicks: gegenüberliegende Ecken | halbdurchsichtiges Rechteck, z. B. für einen Unterstützungsbereich |
| Fibonacci-Retracement | zwei Klicks: Hoch und Tief | waagrechte Linien bei 0 %, 23,6 %, 38,2 %, 50 %, 61,8 %, 78,6 %, 100 % mit Preis |
| Auswählen und Löschen | Klick auf eine Zeichnung, dann Entf-Taste oder Knopf „Löschen“ | Zeichnung entfernt |
| Alles löschen | Knopf „Alle löschen“ mit Bestätigung in der Seite | alle Zeichnungen dieses Wertpapiers entfernt |
| Rückgängig | Strg+Z oder Knopf | letzte Änderung zurück |

**Gehört nicht dazu:** Freihandzeichnen, Text-Notizen, Farbwahl je Zeichnung, Verschieben von Endpunkten per
Ziehen, Teilen mit anderen Benutzern. Diese Punkte können später als eigene Features folgen.

## 3. Verhalten im Detail

- **Werkzeugleiste** über dem Kursdiagramm: ein Knopf je Werkzeug plus „Auswählen“ (Standard). Das gewählte
  Werkzeug ist hervorgehoben; Esc bricht eine angefangene Zeichnung ab und wechselt zurück zu „Auswählen“.
- **Vorschau:** Nach dem ersten Klick folgt die Zeichnung der Maus, bis der zweite Klick sie festlegt.
- **Koordinaten in Daten, nicht in Pixeln:** Jeder Punkt wird als (Datum, Preis) gespeichert. Dadurch bleiben
  Zeichnungen richtig, wenn sich die Fenstergröße, der Zeitraum (von/bis) oder der sichtbare Preisbereich ändert.
  Liegt ein Punkt außerhalb des angezeigten Zeitraums, wird die Zeichnung abgeschnitten bzw. ausgeblendet.
- **Magnet (eingeschaltet, abschaltbar):** Ein Klick nahe einer Kerze rastet auf deren High, Low,
  Open oder Close ein, je nachdem, was am nächsten liegt. Ohne Magnet zählt die genaue Mausposition.
- **Ein Datum pro Kerze:** Die x-Position wird auf den nächsten Handelstag gerundet (es gibt keine Werte
  zwischen zwei Tagen).
- **Je Wertpapier getrennt:** Zeichnungen von IBM erscheinen nicht bei Apple.
- **Speichern:** im Browser (`localStorage`), Schlüssel z. B. `marketlens.zeichnungen.IBM`. Ist der Speicher
  nicht verfügbar (privates Fenster), funktioniert alles, nur ohne Speichern; die Seite zeigt dann einen Hinweis.
- **Zusammenspiel mit dem Fadenkreuz:** Im Modus „Auswählen“ bleibt das Fadenkreuz aus E10 wie bisher; in einem
  Zeichenmodus zeigt es zusätzlich den Preis an der Mausposition.
- **Dunkles Schema:** Zeichnungen verwenden eine eigene Farbvariable (`--zeichnung`) und sind in beiden Schemata gut sichtbar.
- **Sicherheit:** Gespeicherte Daten werden beim Laden geprüft (Datum gültig, Preis endliche Zahl, bekannter
  Werkzeugtyp); fehlerhafte Einträge werden verworfen statt die Seite zu stören.

## 4. Betroffene Dateien

| Datei | Änderung |
|---|---|
| `web/index.html` | Werkzeugleiste über dem Kursdiagramm |
| `web/style.css` | Werkzeugleiste, Farbe `--zeichnung`, ausgewählte Zeichnung |
| `web/app.js` | Zeichenmodus, Umrechnung Pixel ↔ (Datum, Preis), Zeichnen der Objekte, Auswahl, Löschen, Rückgängig, Speichern |
| `docs/testprotokoll.md` | neue Testfälle (Abschnitt 6) |
| `docs/F1_…` bzw. Erklärungsdatei | Erklärung wie bei den anderen Eigenschaften |

**Server und C++: keine Änderung** (Speichern nur im Browser, Entscheidung 1).
`app.js` wird größer; es bietet sich an, die Zeichenlogik in eine eigene Datei `web/zeichnen.js` zu legen.

## 5. Offene Entscheidungen

| Nr. | Frage | Empfehlung / Entscheidung |
|---|---|---|
| 1 | Wo werden Zeichnungen gespeichert? (a) im Browser, (b) auf dem Server als Datei/JSON über neue Endpunkte | **Entschieden (2026-11-16): (a) nur im Browser** (`localStorage`). Damit nur Weboberfläche, kein neuer Endpunkt |
| 2 | Welche Werkzeuge? | **Entschieden (2026-11-16):** die fünf aus Abschnitt 2, dazu Auswählen/Löschen, Alle löschen, Rückgängig |
| 3 | Magnet an Kerzenpreise? | **Entschieden (2026-11-16):** ja, standardmäßig an, mit Schalter |
| 4 | Fibonacci-Stufen | **Entschieden (2026-11-16):** 0, 23,6, 38,2, 50, 61,8, 78,6, 100 % |
| 5 | Zeichnungen auch in RSI/MACD? | **Entschieden (2026-11-16):** nein, nur im Kursdiagramm |
| 6 | Bedienung auf Touch-Geräten | **Entschieden (2026-11-16):** Antippen statt Klicken genügt; keine Gesten |

## 6. Prüfung

- **Automatisch:** Der bestehende Test `Weboberflaeche.KeineExternenAdressen` deckt neue Dateien in `web/` mit
  ab; `Weboberflaeche.WirdVomServerAusgeliefert` wird um `zeichnen.js` erweitert, falls diese Datei entsteht.
- **Manuell (neue Fälle im Testprotokoll):** jedes Werkzeug zeichnen; Vorschau beim Zeichnen; Esc bricht ab;
  Auswählen und Löschen; Rückgängig; „Alle löschen“ mit Bestätigung; Fenstergröße ändern → Zeichnung bleibt an
  denselben Daten/Preisen; Zeitraum ändern → Zeichnung passt sich an; anderes Wertpapier → eigene Zeichnungen;
  Seite neu laden → Zeichnungen wieder da; Fibonacci-Preise stimmen mit Handrechnung überein; Magnet rastet auf
  High/Low ein; dunkles Schema; privates Fenster ohne Speicher → Hinweis, keine Fehler.
