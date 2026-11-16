# Manuelles Testprotokoll – Weboberfläche (E10)

`PROJEKT.md` verlangt für E10 eine Prüfung über ein manuelles Testprotokoll. Die Spalten **Ergebnis**
(✅ / ❌ mit kurzer Notiz) und am Ende **Datum, Browser, Prüfer** werden beim Durchführen ausgefüllt.

## Vorbereitung

Wichtig: Ein Port kann nur beim **Erzeugen** eines Containers veröffentlicht werden. Läuft der Container
von `./dev.sh` schon (z. B. weil darin Claude Code oder eine andere Shell offen ist), öffnet
`FAUP_RUN_ARGS="-p 8080:8080" ./dev.sh` nur eine weitere Shell darin, und der Port bleibt unveröffentlicht.
Deshalb läuft der Server für den Browsertest in einem eigenen, zweiten Container:

1. Im Container bauen und testen (wie immer):
   ```bash
   cmake -B build && cmake --build build && ctest --test-dir build
   ```
2. In einem **WSL-Terminal außerhalb des Containers**, im Projektordner:
   ```bash
   podman run --rm -it -p 8080:8080 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev ./build/projekt --host 0.0.0.0
   ```
   Erwartete Ausgabe: `MarketLens: 6 Wertpapiere, … 2 Zentralbanken (515 Zinseinträge) …`, keine Warnung zu den Zinsdaten,
   und `Im Browser öffnen: http://localhost:8080/`. Beenden mit Strg+C in diesem Terminal.
3. Im Browser **http://localhost:8080/** öffnen (nicht `0.0.0.0`: das ist keine Adresse, die ein Browser öffnen kann).

Vorhandene Offline-Daten: Kurse nur für **IBM** (`daten/IBM.json`, 2026-06-25 bis 2026-11-13), Zinsen für Fed und EZB.

## Testfälle

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| 1 | Seite öffnen | Kopfzeile „MarketLens“; Auswahl enthält 6 Wertpapiere (IBM, AAPL, MBG.DEX, TSCO.LON, SHOP.TRT, RELIANCE.BSE) mit Land und Währung; IBM ist gewählt und wird geladen | ✅ |
| 2 | IBM angezeigt | Titel „IBM · International Business Machines · USA · 247,14 USD“; Kerzen (grün steigend, rot fallend), Achsen mit deutschen Zahlen und Daten; keine Fehlermeldung | ✅ |
| 3 | SMA und EMA | Blaue SMA-Linie und orange EMA-Linie, Legende „SMA 20“, „EMA 20“; beide beginnen erst nach der Einschwingphase (nicht am linken Rand) | ✅ |
| 4 | Support/Resistance | Gestrichelte waagrechte Linien, rechts beschriftet „S …“ (grün, unter dem letzten Kurs) bzw. „R …“ (rot, darüber) mit Berührungszahl | ✅ |
| 5 | Zinsentscheidung im Diagramm | Senkrechte gelbe gestrichelte Linie am 18.09.26, beschriftet „Fed 4,00 %“ | ✅ |
| 6 | Ereignisstudie-Tabelle | Eine Zeile: Fed, wirksam ab 17.09.26, 4,00 %, Reaktionstag 18.09.26, 237,75 → 229,55, **−3,45 %** in Rot; darüber der Hinweis, dass das Datum der Tag der Wirksamkeit ist | ✅ |
| 7 | RSI und MACD | Unter dem Kursdiagramm RSI (Raster bei 30/50/70) und MACD (Balken grün/rot, MACD- und Signallinie); Tage stehen genau unter denen des Kursdiagramms | ✅ |
| 8 | Kennzahlen | Zeitraum, Handelstage 100, Veränderung im Zeitraum, Volatilität, maximaler Drawdown (negativ), Pivot P, R1/R2, S1/S2, Anzahl Niveaus | ✅ |
| 9 | Maus über dem Kursdiagramm | Senkrechte Linie folgt der Maus; Kasten zeigt Datum, O/H/L/C, SMA, EMA und RSI des Tages | ✅ |
| 10 | Ein/Aus ohne Neuladen | „Bollinger“ einschalten → Band und gestrichelte Mittellinie erscheinen sofort; „Pivot-Punkte“ → gepunktete Linien R2…S2 rechts; „RSI“/„MACD“ aus → Diagramme verschwinden; „Zinsentscheidungen“ aus → senkrechte Linie verschwindet | ✅ |
| 11 | Perioden ändern | SMA auf 5 setzen, „Anzeigen“ → Legende „SMA 5“, Linie beginnt früher und folgt den Kursen enger | ✅ |
| 12 | Zeitraum | von = 2026-08-01, bis = 2026-10-31, „Anzeigen“ → nur August bis Oktober (etwa 65 Handelstage, genug für MACD 12,26,9); Ereignis vom 18.09. weiterhin sichtbar | ✅ |
| 13 | **Fehler 400** (ungültige Eingabe) | MACD auf `26,12,9` setzen, „Anzeigen“ → roter Kasten „Indikatoren: Ungültige Eingabe …“ mit der Meldung des Servers (HTTP 400, „fast period must be smaller …“); Kerzen und Ereignisstudie werden trotzdem angezeigt | ✅ |
| 14 | **Fehler 422** (zu wenige Daten) | MACD zurück auf `12,26,9`, von = 2026-11-01, „Anzeigen“ → „Indikatoren: Für diese Einstellungen gibt es zu wenige Kursdaten …“ (HTTP 422); Kerzen werden angezeigt | ✅ |
| 15 | **Fehler 502** (keine Daten) | Bei der Durchführung am 2026-11-16 hatte MBG.DEX noch keine Datei: „MBG.DEX“ wählen → drei Meldungen „… Die Datenquelle liefert für dieses Wertpapier keine Daten“ (HTTP 502, „No offline data for MBG.DEX …“); Diagramm „Keine Kursdaten“; Seite bleibt bedienbar. **Für spätere Durchläufe** (inzwischen haben alle Wertpapiere Daten): Server mit leerem Datenordner starten, `… ./build/projekt --host 0.0.0.0 --daten /tmp`, dann ein beliebiges Wertpapier wählen → gleiche Meldungen | ✅ |
| 16 | Zurück zu einem gültigen Wertpapier | „IBM“ wählen → Fehlermeldungen verschwinden, alles wird wieder angezeigt | ✅ |
| 17 | **Server nicht erreichbar** | Im WSL-Terminal des Servers Strg+C (Ausgabe „MarketLens beendet.“), dann im Browser „Anzeigen“ → „Keine Verbindung zum Server. Läuft MarketLens (./build/projekt)?“ | ✅ |
| 18 | Ohne externe Dienste | Server wieder starten; Browser-Entwicklerwerkzeuge (F12) → Reiter „Netzwerk“, Seite neu laden → alle Anfragen gehen an `localhost:8080` (index.html, style.css, app.js, /api/…); keine fremde Adresse | ✅ |
| 19 | Fenstergröße | Fenster schmal ziehen (< 760 px) → Einstellungen über den Diagrammen, Diagramme werden neu gezeichnet, Tabelle scrollt seitlich statt die Seite zu verbreitern | ✅ |
| 20 | Dunkles Schema | Betriebssystem auf dunkles Design stellen → Seite dunkel, alle Linien und Texte gut lesbar | ✅ |

## F1 – Zeichenwerkzeuge (Feature, siehe `Features/F1_Zeichenwerkzeuge.md`)

Vorbereitung wie oben, Wertpapier IBM, kein Zeitraum.

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| F1-1 | Werkzeugleiste ansehen | Über dem Kursdiagramm: Auswählen (hervorgehoben), Trendlinie, Strahl, Waagrechte, Zone, Fibonacci, Magnet (an), Löschen, Rückgängig, Alle löschen (diese drei zunächst gesperrt) | ✅ |
| F1-2 | „Trendlinie“, zwei Punkte klicken | Nach dem ersten Klick: Anleitung in der Leiste, Mauszeiger als Fadenkreuz, gestrichelte Vorschau folgt der Maus, Infokasten zeigt „Preis an der Maus“. Nach dem zweiten Klick: feste Linie, Werkzeug wieder „Auswählen“ | ✅ |
| F1-3 | „Strahl“, zwei Punkte klicken | Linie vom ersten Punkt durch den zweiten bis zum Rand der Zeichenfläche | ✅ |
| F1-4 | „Waagrechte“, einmal klicken | Linie über die ganze Breite, rechts mit dem Preis beschriftet | ✅ |
| F1-5 | „Zone“, zwei Ecken klicken | Halbdurchsichtiges Rechteck zwischen den Ecken | ✅ |
| F1-6 | „Fibonacci“, erst ein Tief, dann ein Hoch klicken | 7 Linien mit „0 %“ beim Hoch, „100 %“ beim Tief, dazwischen 23,6 / 38,2 / 50 / 61,8 / 78,6 %. **Handrechnung:** jede Stufe s = Preis(0 %) + (Preis(100 %) − Preis(0 %)) × s; z. B. 0 % = 250,00, 100 % = 200,00 → 61,8 % = 219,10, 50 % = 225,00, 38,2 % = 230,90 | ✅ |
| F1-7 | Magnet | Mit Magnet: Klick knapp neben das Hoch einer Kerze → Preis der Zeichnung ist genau das High (Infokasten/Beschriftung). Magnet aus → Preis ist genau die Mausposition | ✅ |
| F1-8 | Esc beim Zeichnen | Nach dem ersten Klick Esc → Vorschau verschwindet, Werkzeug wieder „Auswählen“, nichts gezeichnet | ✅ |
| F1-9 | Auswählen und Löschen | Klick auf eine Zeichnung → dick hervorgehoben, „Löschen“ freigegeben; Entf-Taste (oder Knopf) → entfernt; Klick daneben → Auswahl aufgehoben | ✅ |
| F1-10 | Rückgängig | Strg+Z (oder Knopf) → letzte Änderung zurück (Zeichnung wieder da bzw. neue Zeichnung weg); mehrfach möglich | ✅ |
| F1-11 | Alle löschen | Erster Klick → Knopf fragt „Wirklich alle löschen?“; ohne zweiten Klick nach 4 s zurück; zweiter Klick → alle Zeichnungen weg; Strg+Z bringt sie zurück | ✅ |
| F1-12 | Fenstergröße und Zeitraum | Fenster schmaler/breiter → Zeichnungen bleiben an denselben Tagen und Preisen. Zeitraum ändern (z. B. von = 2026-09-01) → Zeichnungen mit Punkten außerhalb verschwinden, waagrechte Linien bleiben; Zeitraum leeren → alles wieder da | ✅ |
| F1-13 | Je Wertpapier, Neuladen | Zu AAPL wechseln → keine IBM-Zeichnungen; eigene AAPL-Zeichnung anlegen; zurück zu IBM → IBM-Zeichnungen wieder da; Seite neu laden (F5) → Zeichnungen beider Wertpapiere erhalten | ✅ |
| F1-14 | Eingabefelder | In das Feld „SMA“ klicken, Rücktaste drücken → Ziffer wird gelöscht, nicht die ausgewählte Zeichnung | ✅ |
| F1-15 | Ohne Browser-Speicher | Privates Fenster (InPrivate) öffnen: Zeichnen funktioniert; ist der Speicher gesperrt, erscheint der Hinweis „Browser-Speicher nicht verfügbar …“ statt eines Fehlers | ✅ |
| F1-16 | Dunkles Schema | Zeichnungen und Beschriftungen gut sichtbar | ✅ |

| Datum | Browser und Version | Prüfer | Ergebnis F1 |
|---|---|---|---|
| 2026-11-16 | Microsoft Edge (Version bitte eintragen) | Abdul Hamid Al Moghrabi | ✅ alle 16 Testfälle erfüllt |

## Automatisch geprüft (zur Ergänzung, nicht Teil des manuellen Tests)

- `Weboberflaeche.WirdVomServerAusgeliefert`: `/`, `/app.js`, `/zeichnen.js`, `/style.css` mit Status 200 und richtigem Inhaltstyp; `zeichnen.js` wird vor `app.js` eingebunden
- `Weboberflaeche.KeineExternenAdressen`: keine Datei in `web/` enthält `http://`, `https://` oder `"//` (außer der SVG-Namensraum-Kennung)
- Die Endpunkte, die die Seite benutzt, sind in E8/E9 getestet (Status, JSON, Fehlerfälle).

## Durchführung

| Datum | Browser und Version | Betriebssystem | Prüfer | Ergebnis gesamt |
|---|---|---|---|---|
| 2026-11-16 | Microsoft Edge (Version bitte eintragen) | Windows mit WSL, Server im Container `faup-dev` über `podman run -p 8080:8080` | Abdul Hamid Al Moghrabi | ✅ alle 20 Testfälle erfüllt |
