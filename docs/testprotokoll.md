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

Offline-Daten bei der Durchführung am 2026-11-16: Kurse nur für **IBM** (2026-06-25 bis 2026-11-13), Zinsen für Fed und EZB.
Seit dem 2026-11-16 liegen Kurse für alle 6 Wertpapiere in `daten/alphavantage/` und für IBM/AAPL in `daten/twelvedata/` (F3).

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

## F2 – Erklärungen „?“ (Feature, siehe `Features/F2_Erklaerungen.md`)

Vorbereitung wie oben, Wertpapier IBM, kein Zeitraum, Standardperioden.

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| F2-1 | Maus über das „?“ neben „SMA“ | Fenster mit Titel, Absätzen „was / wie / lesen“, fett „Aktuell: SMA 20 am letzten Tag = …“, darunter kursiv „Übliche Deutung, keine Anlageberatung.“ Maus weg → Fenster schließt nach kurzer Zeit | ✅ |
| F2-2 | Maus vom „?“ in das Fenster bewegen | Fenster bleibt offen, kein Flackern; Maus aus dem Fenster → schließt | ✅ |
| F2-3 | SMA auf 5, „Anzeigen“, dann „?“ bei SMA | Text nennt „SMA 5“ und „Für die ersten 4 Tage …“; der aktuelle Wert entspricht der SMA-Linie am letzten Tag (Infokasten) | ✅ |
| F2-4 | Alle 8 „?“ in den Einstellungen | SMA, EMA (mit α = 0,0952 bei 20), Bollinger, Support/Resistance (Fenster 5, Toleranz 2 %), Pivot-Punkte, Zinsentscheidungen, RSI, MACD zeigen jeweils die passende Erklärung | ✅ |
| F2-5 | Alle „?“ bei den Kennzahlen | Zeitraum, Handelstage, Veränderung, Volatilität, Maximaler Drawdown, Pivot (dreimal derselbe Text), Niveaus, Währung; jede „Aktuell“-Zeile stimmt mit der angezeigten Kennzahl überein (z. B. Drawdown, P/R1/R2/S1/S2) | ✅ |
| F2-6 | TSCO.LON wählen, „?“ bei „Währung“ | „Aktuell: GBX (Pence)“, Erklärung 100 GBX = 1 GBP | ✅ |
| F2-7 | „?“ oben rechts in Kurs-, RSI- und MACD-Diagramm | Erklärung der Kerzen und Markierungen bzw. von RSI und MACD | ✅ |
| F2-8 | „?“ in der Werkzeugleiste | Erklärung aller Zeichenwerkzeuge, Magnet und Fibonacci-Stufen | ✅ |
| F2-9 | „?“ bei „Ereignisstudie“ und bei allen 7 Spaltenköpfen | Passende Erklärung; „Wirksam ab“ erklärt, dass es nicht der Sitzungstag ist | ✅ |
| F2-10 | „?“ bei der letzten Spalte „Veränderung“ (rechter Rand) | Fenster bleibt vollständig im sichtbaren Bereich | ✅ |
| F2-11 | Tastatur: mit Tab zu einem „?“ | Fenster öffnet beim Fokus; Esc schließt | ✅ |
| F2-12 | Klick auf das „?“ neben „RSI“ | Fenster öffnet; das Kästchen „RSI“ wird **nicht** umgeschaltet | ✅ |
| F2-13 | Schmales Fenster bzw. Touch (F12 → Gerätesymbolleiste) | Antippen eines „?“ öffnet, Antippen außerhalb schließt; Fenster passt in die Breite | ✅ |
| F2-14 | Fehlerfall: von = 2026-11-01, „Anzeigen“, dann „?“ bei „RSI“ | Erklärung erscheint ohne „Aktuell“-Satz; nirgends „undefined“ oder leere Platzhalter | ✅ |
| F2-15 | Dunkles Schema | „?“ und Fenster gut lesbar | ✅ |
| F2-17 | **Erklärungen unten auf der Seite** (nach Korrektur 2026-11-16) | Ganz nach unten scrollen; „?“ bei „Ereignisstudie“ und allen Spalten der Tabelle überfahren: Fenster erscheint vollständig (bei wenig Platz darunter über dem „?“). Fenster des Browsers sehr niedrig machen: Fenster bleibt im Bild, langer Text lässt sich im Fenster scrollen, ohne dass es schließt | ✅ |
| F2-16 | Zeichnen nach F2 | Zeichenwerkzeuge (F1) funktionieren weiter; das „?“ oben rechts im Kursdiagramm stört nicht | ✅ |

| Datum | Browser und Version | Prüfer | Ergebnis F2 |
|---|---|---|---|
| 2026-11-16 | Microsoft Edge (Version bitte eintragen) | Abdul Hamid Al Moghrabi | ✅ alle 17 Testfälle erfüllt (Erklärungen unten auf der Seite wurden zunächst abgeschnitten; behoben und mit F2-17 erneut geprüft) |

## F3 – Zweiter Anbieter Twelve Data, zwei Server nebeneinander (Feature, siehe `Features/F3_Zweiter_Anbieter.md`)

Vorbereitung: zwei WSL-Terminals außerhalb des Containers, im Projektordner, Programm gebaut.

    # Terminal 1: Alpha Vantage, alle Wertpapiere
    podman run --rm -it -p 8080:8080 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev ./build/projekt --quelle alphavantage --port 8080 --host 0.0.0.0
    # Terminal 2: Twelve Data, nur USA
    podman run --rm -it -p 8081:8081 -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev ./build/projekt --quelle twelvedata --port 8081 --host 0.0.0.0

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| F3-1 | Startmeldungen beider Terminals | 1: „MarketLens: 6 Wertpapiere (Alpha Vantage), Kursdaten aus 'daten/alphavantage/' …“; 2: „MarketLens: 2 Wertpapiere (Twelve Data), Kursdaten aus 'daten/twelvedata/' …“; beide ohne Warnung zu den Zinsdaten | ✅ |
| F3-2 | Tab 1 `http://localhost:8080/`, Tab 2 `http://localhost:8081/` | Kopfzeile „Kurse: Alpha Vantage“ bzw. „Kurse: Twelve Data“; Titel der Browser-Tabs „MarketLens – Alpha Vantage“ bzw. „MarketLens – Twelve Data“ | ✅ |
| F3-3 | Auswahlliste | Tab 1: 6 Wertpapiere; Tab 2: nur IBM und AAPL | ✅ |
| F3-4 | IBM in beiden Tabs | Titel 247,14 USD (Tab 1) bzw. 247,13 USD (Tab 2); Kerzen, SMA, RSI und Support/Resistance sehen gleich aus (Abweichung nur durch Rundung) | ✅ |
| F3-5 | Ereignisstudie IBM in beiden Tabs | jeweils das Fed-Ereignis 17.09.26 → 18.09.26, 237,75 → 229,55, −3,45 % (an diesen beiden Tagen sind die Schlusskurse bei beiden Anbietern gleich) | ✅ |
| F3-6 | „?“ neben „Kurse: …“ | Erklärung des Anbieters mit „Aktuell: Alpha Vantage“ bzw. „Aktuell: Twelve Data“ | ✅ |
| F3-7 | Zeichnungen getrennt | In Tab 1 bei IBM eine Trendlinie zeichnen → in Tab 2 bei IBM nicht vorhanden (und umgekehrt) | ✅ |
| F3-8 | Unabhängig | Terminal 2 mit Strg+C beenden („MarketLens beendet.“) → Tab 1 funktioniert weiter (Anzeigen klicken); Tab 2 zeigt „Keine Verbindung zum Server“ | ✅ |
| F3-9 | Port belegt | Während Terminal 2 läuft, in einem dritten WSL-Terminal denselben `podman run … -p 8081:8081 …`-Befehl starten → podman meldet, dass Port 8081 schon belegt ist („address already in use“), der zweite Twelve-Data-Server startet nicht. (Die eigene Prüfung des Programms auf belegte Ports ist in E9 automatisch getestet.) | ✅ |
| F3-10 | Unbekannter Anbieter | In der Shell des Entwicklungs-Containers (`./dev.sh`): `./build/projekt --quelle finnhub` → „Fehler: unbekannter Anbieter: finnhub (erlaubt: alphavantage, twelvedata)“ | ✅ |

| Datum | Browser und Version | Prüfer | Ergebnis F3 |
|---|---|---|---|
| 2026-11-16 | Microsoft Edge (Version bitte eintragen) | Abdul Hamid Al Moghrabi | ✅ alle 10 Testfälle erfüllt |

## F5 – Live-Zugriff auf beide Anbieter (Feature, siehe `Features/F5_Live_Zugriff.md`)

Vorbereitung: Image mit `libssl-dev` neu gebaut (`./dev.sh build`), Live-Build im Container (ohne nacktes `-j`):

    cmake -B build-live -DMARKETLENS_LIVE_HTTP=ON && cmake --build build-live && ctest --test-dir build-live

Dann zwei WSL-Terminals außerhalb des Containers, im Projektordner. `--env-file` übergibt die API-Keys
(`ALPHAVANTAGE_API_KEY`, `TWELVEDATA_API_KEY`) wie `./dev.sh`:

    # Terminal 1: Alpha Vantage live
    podman run --rm -it -p 8080:8080 --env-file ~/.config/faup/env -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev ./build-live/projekt --live --quelle alphavantage --port 8080 --host 0.0.0.0
    # Terminal 2: Twelve Data live
    podman run --rm -it -p 8081:8081 --env-file ~/.config/faup/env -v "$PWD:/work:Z" -w /work --userns=keep-id:uid=1000,gid=1000 faup-dev ./build-live/projekt --live --quelle twelvedata --port 8081 --host 0.0.0.0

Vor dem ersten Durchlauf `cache/` löschen, damit F5-2 wirklich übers Netz geht.
Achtung Kontingent: Alpha Vantage erlaubt nur ca. 25 Abrufe pro Tag; der ganze Test braucht höchstens 7.

| Nr. | Schritt | Erwartetes Ergebnis | Ergebnis |
|---|---|---|---|
| F5-1 | Startmeldungen | 1: „… (Alpha Vantage), Kursdaten live, Cache 'cache/alphavantage/' (12 h) …“; 2: „… (Twelve Data), Kursdaten live, Cache 'cache/twelvedata/' (1 h) …“ | |
| F5-2 | Erster Abruf | http://localhost:8080/ und http://localhost:8081/, je IBM → Kerzen bis zum letzten Handelstag (neuer als in `daten/`); kein Hinweis auf veraltete Daten; danach liegt je eine Datei in `cache/alphavantage/` und `cache/twelvedata/` | |
| F5-3 | Beide Anbieter gleich | Schlusskurs von IBM am letzten Tag in beiden Tabs gleich (höchstens 0,01 Unterschied) | |
| F5-4 | Kein Key in den Dateien | `grep -r apikey cache/` und `grep -r "<dein Key>" cache/` finden nichts | |
| F5-5 | Neustart ohne neuen Abruf | Beide Server mit Strg+C beenden und neu starten, IBM öffnen → gleiche Daten sofort; Änderungszeit der Dateien in `cache/` (`ls -l --time-style=full-iso cache/*`) unverändert | |
| F5-6 | Netz weg: alte Daten | Terminal 2 neu mit `--gueltigkeit 1` und zusätzlich `--network none` vor `faup-dev`, frühestens eine Stunde nach F5-2 (der Abrufzeitpunkt steht in der Datei, `touch` hilft nicht) IBM öffnen → Kurse werden angezeigt, dazu der Hinweis auf veraltete Daten (Datum des Abrufs, Netzwerkfehler) | |
| F5-7 | Netz weg, kein Cache | Wie F5-6, aber AAPL (noch nie live geladen) → Fehlermeldung „Die Datenquelle liefert für dieses Wertpapier keine Daten“ o. ä. (HTTP 502), Seite bleibt bedienbar | |
| F5-8 | Fehlender Key | `podman run` **ohne** `--env-file` → Programm endet sofort mit „Fehler: …TWELVEDATA_API_KEY…“ (bzw. ALPHAVANTAGE_API_KEY) | |
| F5-9 | `--live` im normalen Build | Im Container `./build/projekt --live` → „Fehler: --live braucht HTTPS; bauen mit: cmake -B build-live -DMARKETLENS_LIVE_HTTP=ON“ | ✅ (2026-11-19) |
| F5-10 | Unpassende Optionen | `./build/projekt --cache x` → „--cache und --gueltigkeit gelten nur zusammen mit --live“; `--live --daten daten` → „--daten und --live schließen sich aus“; `--live --gueltigkeit 0` → „ungültige Gültigkeit (Stunden 1-168)“ | ✅ (2026-11-19) |

| Datum | Browser und Version | Prüfer | Ergebnis F5 |
|---|---|---|---|
| | | | |

## Automatisch geprüft (zur Ergänzung, nicht Teil des manuellen Tests)

- `Weboberflaeche.WirdVomServerAusgeliefert`: `/`, `/app.js`, `/zeichnen.js`, `/erklaerungen.js`, `/style.css` mit Status 200 und richtigem Inhaltstyp; `zeichnen.js` und `erklaerungen.js` werden vor `app.js` eingebunden
- `Weboberflaeche.ErklaerungenVollstaendig` (F2): jeder benutzte Schlüssel hat einen Text, jeder Text wird benutzt, jeder Platzhalter wird geliefert
- `Weboberflaeche.KeineExternenAdressen`: keine Datei in `web/` enthält `http://`, `https://` oder `"//` (außer der SVG-Namensraum-Kennung)
- Die Endpunkte, die die Seite benutzt, sind in E8/E9 getestet (Status, JSON, Fehlerfälle).

## Durchführung

| Datum | Browser und Version | Betriebssystem | Prüfer | Ergebnis gesamt |
|---|---|---|---|---|
| 2026-11-16 | Microsoft Edge (Version bitte eintragen) | Windows mit WSL, Server im Container `faup-dev` über `podman run -p 8080:8080` | Abdul Hamid Al Moghrabi | ✅ alle 20 Testfälle erfüllt |
