# Manuelles Testprotokoll – Weboberfläche (E10)

`PROJEKT.md` verlangt für E10 eine Prüfung über ein manuelles Testprotokoll. Die Spalten **Ergebnis**
(✅ / ❌ mit kurzer Notiz) und am Ende **Datum, Browser, Prüfer** werden beim Durchführen ausgefüllt.

## Vorbereitung

1. Container mit veröffentlichtem Port starten (im WSL-Terminal, im Projektordner):
   ```bash
   FAUP_RUN_ARGS="-p 8080:8080" ./dev.sh
   ```
2. Im Container bauen und starten:
   ```bash
   cmake -B build && cmake --build build && ctest --test-dir build
   ./build/projekt --host 0.0.0.0
   ```
   Erwartete Ausgabe: `MarketLens: 6 Wertpapiere, … 2 Zentralbanken (515 Zinseinträge) …` und keine Warnung zu den Zinsdaten.
3. Im Browser unter Windows/WSL öffnen: **http://localhost:8080/**

Vorhandene Offline-Daten: Kurse nur für **IBM** (`daten/IBM.json`, 2026-06-25 bis 2026-11-13), Zinsen für Fed und EZB.

## Testfälle

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| 1 | Seite öffnen | Kopfzeile „MarketLens“; Auswahl enthält 6 Wertpapiere (IBM, AAPL, MBG.DEX, TSCO.LON, SHOP.TRT, RELIANCE.BSE) mit Land und Währung; IBM ist gewählt und wird geladen | |
| 2 | IBM angezeigt | Titel „IBM · International Business Machines · USA · 247,14 USD“; Kerzen (grün steigend, rot fallend), Achsen mit deutschen Zahlen und Daten; keine Fehlermeldung | |
| 3 | SMA und EMA | Blaue SMA-Linie und orange EMA-Linie, Legende „SMA 20“, „EMA 20“; beide beginnen erst nach der Einschwingphase (nicht am linken Rand) | |
| 4 | Support/Resistance | Gestrichelte waagrechte Linien, rechts beschriftet „S …“ (grün, unter dem letzten Kurs) bzw. „R …“ (rot, darüber) mit Berührungszahl | |
| 5 | Zinsentscheidung im Diagramm | Senkrechte gelbe gestrichelte Linie am 18.09.26, beschriftet „Fed 4,00 %“ | |
| 6 | Ereignisstudie-Tabelle | Eine Zeile: Fed, wirksam ab 17.09.26, 4,00 %, Reaktionstag 18.09.26, 237,75 → 229,55, **−3,45 %** in Rot; darüber der Hinweis, dass das Datum der Tag der Wirksamkeit ist | |
| 7 | RSI und MACD | Unter dem Kursdiagramm RSI (Raster bei 30/50/70) und MACD (Balken grün/rot, MACD- und Signallinie); Tage stehen genau unter denen des Kursdiagramms | |
| 8 | Kennzahlen | Zeitraum, Handelstage 100, Veränderung im Zeitraum, Volatilität, maximaler Drawdown (negativ), Pivot P, R1/R2, S1/S2, Anzahl Niveaus | |
| 9 | Maus über dem Kursdiagramm | Senkrechte Linie folgt der Maus; Kasten zeigt Datum, O/H/L/C, SMA, EMA und RSI des Tages | |
| 10 | Ein/Aus ohne Neuladen | „Bollinger“ einschalten → Band und gestrichelte Mittellinie erscheinen sofort; „Pivot-Punkte“ → gepunktete Linien R2…S2 rechts; „RSI“/„MACD“ aus → Diagramme verschwinden; „Zinsentscheidungen“ aus → senkrechte Linie verschwindet | |
| 11 | Perioden ändern | SMA auf 5 setzen, „Anzeigen“ → Legende „SMA 5“, Linie beginnt früher und folgt den Kursen enger | |
| 12 | Zeitraum | von = 2026-08-01, bis = 2026-10-31, „Anzeigen“ → nur August bis Oktober (etwa 65 Handelstage, genug für MACD 12,26,9); Ereignis vom 18.09. weiterhin sichtbar | |
| 13 | **Fehler 400** (ungültige Eingabe) | MACD auf `26,12,9` setzen, „Anzeigen“ → roter Kasten „Indikatoren: Ungültige Eingabe …“ mit der Meldung des Servers (HTTP 400, „fast period must be smaller …“); Kerzen und Ereignisstudie werden trotzdem angezeigt | |
| 14 | **Fehler 422** (zu wenige Daten) | MACD zurück auf `12,26,9`, von = 2026-11-01, „Anzeigen“ → „Indikatoren: Für diese Einstellungen gibt es zu wenige Kursdaten …“ (HTTP 422); Kerzen werden angezeigt | |
| 15 | **Fehler 502** (keine Daten) | Zeitraum leeren, „MBG.DEX“ wählen → drei Meldungen „… Die Datenquelle liefert für dieses Wertpapier keine Daten“ (HTTP 502, „No offline data for MBG.DEX …“); Diagramm zeigt „Keine Kursdaten“; die Seite bleibt bedienbar | |
| 16 | Zurück zu einem gültigen Wertpapier | „IBM“ wählen → Fehlermeldungen verschwinden, alles wird wieder angezeigt | |
| 17 | **Server nicht erreichbar** | Im Container Strg+C (Ausgabe „MarketLens beendet.“), dann im Browser „Anzeigen“ → „Keine Verbindung zum Server. Läuft MarketLens (./build/projekt)?“ | |
| 18 | Ohne externe Dienste | Server wieder starten; Browser-Entwicklerwerkzeuge (F12) → Reiter „Netzwerk“, Seite neu laden → alle Anfragen gehen an `localhost:8080` (index.html, style.css, app.js, /api/…); keine fremde Adresse | |
| 19 | Fenstergröße | Fenster schmal ziehen (< 760 px) → Einstellungen über den Diagrammen, Diagramme werden neu gezeichnet, Tabelle scrollt seitlich statt die Seite zu verbreitern | |
| 20 | Dunkles Schema | Betriebssystem auf dunkles Design stellen → Seite dunkel, alle Linien und Texte gut lesbar | |

## Automatisch geprüft (zur Ergänzung, nicht Teil des manuellen Tests)

- `Weboberflaeche.WirdVomServerAusgeliefert`: `/`, `/app.js`, `/style.css` mit Status 200 und richtigem Inhaltstyp
- `Weboberflaeche.KeineExternenAdressen`: keine Datei in `web/` enthält `http://`, `https://` oder `"//` (außer der SVG-Namensraum-Kennung)
- Die Endpunkte, die die Seite benutzt, sind in E8/E9 getestet (Status, JSON, Fehlerfälle).

## Durchführung

| Datum | Browser und Version | Betriebssystem | Prüfer | Ergebnis gesamt |
|---|---|---|---|---|
| | | | | |
