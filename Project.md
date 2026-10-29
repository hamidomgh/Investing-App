# Projekt: MarketLens – globale Markt- und Zentralbankdaten-Analyse als Web-App

## Projektbeschreibung

MarketLens ist eine Web-Anwendung mit C++17-Backend, die historische Kursdaten von Wertpapieren und Indizes aus beliebig vielen Ländern sowie Zinsentscheidungen mehrerer Zentralbanken (z. B. Fed, EZB, Bank of England, Bank of Japan) analysiert. Die Anwendung berechnet gängige Kennzahlen der technischen Analyse und stellt diese über eine HTTP-JSON-Schnittstelle zur Verfügung. Die Ergebnisse werden in einer Weboberfläche mit Kursdiagramm, Indikatoren und Support-/Resistance-Linien dargestellt.

Das System ist datengetrieben aufgebaut: Jedes Wertpapier ist in einem Katalog (CSV) mit Symbol, Name, Land, Währung und Kursdatei beschrieben, jede Zentralbank mit Land und Währung. Neue Länder oder Wertpapiere können durch Hinzufügen von Dateien ergänzt werden, ohne den Code zu ändern. Die mitgelieferten Beispieldaten decken eine feste, kleine Auswahl ab (mindestens drei Länder bzw. Währungsräume). Eine Live-Anbindung an Datenanbieter ist nicht Teil des Projekts; Daten werden als CSV-Dateien importiert. Dadurch laufen alle Tests offline und deterministisch im Dev-Container.

Die Anwendung berechnet Trendindikatoren (SMA, EMA, MACD), Momentum (RSI), Volatilität und Risiko (Bollinger-Bänder, ATR, Renditen, Volatilität, maximaler Drawdown), Support- und Resistance-Niveaus mit klassischen Pivot-Punkten sowie eine Ereignisstudie zur Kursreaktion nach Zinsentscheidungen der jeweils passenden Zentralbank. Mit Wechselkursreihen lassen sich Kurse in eine gemeinsame Währung umrechnen, um Wertpapiere verschiedener Länder zu vergleichen.

Der Server beantwortet mehrere Anfragen gleichzeitig und hält bereits berechnete Ergebnisse in einem thread-sicheren Cache. Die Analyse-Logik ist von Netzwerk und Darstellung getrennt (Interfaces für Datenquellen), sodass sie ohne Server testbar ist. Der HTTP-Server nutzt eine Header-only-Bibliothek, die im Repository liegt; die Weboberfläche besteht aus statischem HTML/JavaScript, das vom Server ausgeliefert wird. Gebaut und getestet wird mit CMake und CTest.

## Eigenschaften und Definitions of Done

### E1: Datenmodell mit Validierung
**Definition of Done:** Eine Kursreihe besteht aus streng zeitlich aufsteigenden Kerzen (Datum, Open, High, Low, Close, Volumen). Inkonsistente Kerzen (High unter Low, Open oder Close außerhalb von Low..High) und nicht aufsteigende Daten werden abgelehnt, und der Fehler nennt das betroffene Datum. Jedes Wertpapier trägt Symbol, Name, Land und Währung.

### E2: Wertpapierkatalog und CSV-Import für mehrere Länder
**Definition of Done:** Ein Katalog (CSV) beschreibt beliebig viele Wertpapiere mit Land, Währung und Kursdatei; alle zugehörigen Kursdateien werden über das Interface `IPriceSource` geladen. Fehlerhafte Zeilen, fehlende Dateien und doppelte Symbole führen zu einem Fehler mit Zeilennummer bzw. Pfad, und eine fehlerhafte Datei verhindert nicht das Laden der übrigen Wertpapiere. Ein neues Land kann ohne Codeänderung ergänzt werden.

### E3: Trend- und Momentum-Indikatoren
**Definition of Done:** Für eine Kursreihe werden SMA, EMA, MACD (Linie, Signal, Histogramm) und RSI mit einstellbaren Perioden berechnet. Die Werte stimmen mit von Hand gerechneten Referenzwerten überein, Werte vor Ablauf der Einschwingphase sind als "nicht verfügbar" gekennzeichnet, und zu kurze Reihen oder ungültige Perioden werden mit Fehler abgelehnt.

### E4: Volatilität und Risikokennzahlen
**Definition of Done:** Berechnet werden Bollinger-Bänder, ATR, einfache und logarithmische Tagesrenditen, annualisierte Volatilität und maximaler Drawdown. Die Werte stimmen mit von Hand gerechneten Referenzwerten überein, und Sonderfälle (konstante Kurse, Reihe mit nur einer Kerze) sind definiert und behandelt.

### E5: Support-/Resistance-Niveaus und Pivot-Punkte
**Definition of Done:** Lokale Hochs und Tiefs werden mit einstellbarem Fenster erkannt (Ränder ohne volles Fenster und flache Plateaus korrekt behandelt), nahe beieinanderliegende Preise werden innerhalb einer relativen Toleranz zu Niveaus mit Berührungszahl zusammengefasst, und jedes Niveau wird relativ zum letzten Schlusskurs als Support oder Resistance eingestuft und aufsteigend sortiert. Klassische Pivot-Punkte (P, R1, R2, S1, S2) stimmen mit von Hand gerechneten Werten überein.

### E6: Zentralbank-Zinsentscheidungen und Ereignisstudie
**Definition of Done:** Zinsentscheidungen mehrerer Zentralbanken (Zentralbank, Land, Datum, Leitzins) werden über das Interface `IRateSource` geladen und validiert. Für jedes Wertpapier wird die Kursveränderung am Handelstag nach jeder Entscheidung der Zentralbank seines Landes berechnet; Entscheidungen auf Nicht-Handelstagen werden auf den nächsten Handelstag abgebildet, Entscheidungen außerhalb des Datenzeitraums werden übersprungen, und Wertpapiere ohne passende Zentralbank liefern eine leere Auswertung statt eines Fehlers.

### E7: Währungsumrechnung
**Definition of Done:** Kursreihen können mit einer Wechselkursreihe in eine Zielwährung umgerechnet werden. Fehlt für einen Handelstag ein Wechselkurs, wird der letzte bekannte Kurs davor verwendet, und liegt keiner vor, wird der Tag ausgelassen; ein fehlendes Währungspaar wird mit Fehler gemeldet. Die Umrechnung ist durch Hand-Rechnungen und die Rückumrechnung (Hin und zurück ergibt den Ausgangswert) abgesichert.

### E8: HTTP-JSON-Schnittstelle
**Definition of Done:** Der Server bietet mindestens Endpunkte für die Liste der Wertpapiere, die Kursdaten eines Wertpapiers, alle Indikatoren inklusive Support/Resistance und die Ereignisstudie. Parameter (Symbol, Zeitraum, Perioden, Zielwährung) werden geprüft; unbekannte Symbole, ungültige Parameter und unbekannte Pfade liefern passende HTTP-Statuscodes (404, 400) mit JSON-Fehlermeldung. Anfragen-Behandlung und Berechnung sind vom Netzwerk trennbar und ohne Socket testbar.

### E9: Gleichzeitige Anfragen mit thread-sicherem Cache
**Definition of Done:** Der Server bearbeitet mehrere Anfragen parallel in einem Thread-Pool. Berechnete Ergebnisse werden in einem Cache gehalten, auf den mehrere Threads zugreifen; Ergebnisse sind unabhängig von Anzahl und Reihenfolge der Anfragen identisch mit denen der sequentiellen Berechnung, identische Anfragen werden nicht mehrfach berechnet, und der Server beendet sich sauber ohne hängende Threads. Ein Stresstest mit festem Seed läuft in begrenzter Zeit.

### E10: Weboberfläche
**Definition of Done:** Eine vom Server ausgelieferte Seite erlaubt die Auswahl eines Wertpapiers und zeigt Kursverlauf, gewählte Indikatoren, Support-/Resistance-Linien und die Ereignisstudie an. Fehlermeldungen der Schnittstelle werden verständlich angezeigt, und die Seite funktioniert ohne externe Dienste (alle Skripte liegen im Repository). Die Prüfung erfolgt über ein manuelles Testprotokoll.

## Schwierigkeit (Selbsteinschätzung)

| | Dimension | Wert | Begründung |
|---|---|---|---|
| A | Algorithmik und Datenstrukturen (0–3) | 2 | Eigene Algorithmen: Pivot-Erkennung, Clustering, Indikatoren mit Einschwingphasen, Ereignisstudie, Wechselkurs-Zuordnung nach Datum. Nichttrivial, aber ohne Optimierung oder Engine. |
| B | Externe Schnittstellen (0–2) | 2 | Netzwerk: HTTP-Server mit JSON-API, dazu CSV-Formate und eine Header-only-HTTP-Bibliothek. |
| C | Nebenläufigkeit und Zeit (0–3) | 2 | Thread-Pool mit geteiltem Cache (geteilter Zustand zwischen Threads). |
| D | Plattform (0–3) | 0 | Desktop/Server. |
| E | Zustand und Persistenz (0–2) | 0 | Daten werden nur gelesen, der Cache liegt im Speicher. |
| F | Interaktion und Darstellung (0–2) | 2 | Weboberfläche mit interaktivem Diagramm. |

R = Summe + größter Einzelwert = 8 + 2 = **10**
S = min(1, R / 14) = **0,71**