# Projekt: MarketLens – globale Markt- und Zentralbankdaten-Analyse als Web-App

**Name:** Abdul Hamid Al Moghrabi
**Matrikelnummer:** 605719

## Projektziel

MarketLens ist eine Web-Anwendung mit C++17-Backend. Sie ruft historische Kursdaten von Wertpapieren und Indizes aus beliebig vielen Ländern sowie Zinsentscheidungen mehrerer Zentralbanken (z. B. Fed, EZB, Bank of England, Bank of Japan) über Web-APIs ab und berechnet daraus gängige Kennzahlen der technischen Analyse. Die Ergebnisse werden über eine eigene HTTP-JSON-Schnittstelle bereitgestellt und in einer Weboberfläche mit Kursdiagramm, Indikatoren und Support-/Resistance-Linien dargestellt.

"Alle Länder" wird datengetrieben umgesetzt: Jedes Wertpapier ist in einer Konfigurationsdatei mit Symbol, Name, Land und Währung beschrieben, jede Zentralbank mit Land und Währung. Ein neues Land oder Wertpapier wird durch einen neuen Eintrag ergänzt, ohne den Code zu ändern. Kurse werden in der Währung des Wertpapiers verarbeitet; eine Währungsumrechnung ist nicht Teil des Projekts. Welche Anbieter genutzt werden, richtet sich nach deren Bedingungen und kostenlosen Kontingenten; der Zugangsschlüssel (API-Key) wird über eine Umgebungsvariable gelesen und nie im Repository abgelegt.

Der Netzwerkzugriff liegt hinter Schnittstellen (`IHttpClient`, `IPriceSource`, `IRateSource`). Die Tests verwenden aufgezeichnete Antworten der Anbieter und eine Fake-Implementierung des HTTP-Clients, sodass alle Tests offline und deterministisch im Dev-Container laufen. Der echte HTTPS-Zugriff ist eine zuschaltbare CMake-Option, damit der Standardbefehl `cmake -B build && cmake --build build && ctest --test-dir build` ohne Netzwerk und ohne zusätzliche Bibliotheken funktioniert; der Live-Zugriff wird über ein manuelles Testprotokoll geprüft. Da kostenlose Kontingente begrenzt sind, werden Antworten auf der Festplatte zwischengespeichert.

Berechnet werden: Trendindikatoren (SMA, EMA, MACD), Momentum (RSI), Volatilität und Risiko (Bollinger-Bänder, ATR, Renditen, Volatilität, maximaler Drawdown), Support- und Resistance-Niveaus mit klassischen Pivot-Punkten sowie eine Ereignisstudie zur Kursreaktion nach Zinsentscheidungen der jeweils passenden Zentralbank.

Der Server beantwortet mehrere Anfragen gleichzeitig und hält berechnete Ergebnisse in einem thread-sicheren Cache. Die Analyse-Logik ist von Netzwerk und Darstellung getrennt und ohne Server testbar. Der HTTP-Server und das JSON-Parsing nutzen Header-only-Bibliotheken, die im Repository liegen; die Weboberfläche besteht aus statischem HTML/JavaScript, das vom Server ausgeliefert wird. Gebaut und getestet wird mit CMake und CTest.

## Eigenschaften und Definitions of Done

### E1: Datenmodell mit Validierung
**Definition of Done:** Eine Kursreihe besteht aus streng zeitlich aufsteigenden Kerzen (Datum, Open, High, Low, Close, Volumen). Inkonsistente Kerzen (High unter Low, Open oder Close außerhalb von Low..High) und nicht aufsteigende Daten werden abgelehnt, und der Fehler nennt das betroffene Datum. Jedes Wertpapier trägt Symbol, Name, Land und Währung.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E2: Kursdaten über eine Web-API
**Definition of Done:** Kursdaten eines Wertpapiers werden über das Interface `IPriceSource` von einer Web-API abgerufen; der Netzwerkzugriff läuft über das Interface `IHttpClient`. Die JSON-Antwort wird in validierte Kerzen (siehe E1) umgewandelt. Fehlerfälle werden unterschieden und mit verständlicher Meldung behandelt: Netzwerkfehler und Zeitüberschreitung, HTTP-Fehlerstatus (z. B. 401 bei ungültigem Schlüssel, 404 bei unbekanntem Symbol), Überschreiten des Anfragekontingents (z. B. 429), ungültiges JSON und fehlende Felder. Ein fehlender API-Key wird vor der ersten Anfrage gemeldet. Ein Katalog (Konfigurationsdatei) beschreibt Symbol, Land und Währung beliebig vieler Wertpapiere; ein neues Land kann ohne Codeänderung ergänzt werden, und ein fehlerhaftes Wertpapier verhindert nicht die Auswertung der übrigen.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E3: Trend- und Momentum-Indikatoren
**Definition of Done:** Für eine Kursreihe werden SMA, EMA, MACD (Linie, Signal, Histogramm) und RSI mit einstellbaren Perioden berechnet. Die Werte stimmen mit von Hand gerechneten Referenzwerten überein, Werte vor Ablauf der Einschwingphase sind als "nicht verfügbar" gekennzeichnet, und zu kurze Reihen oder ungültige Perioden werden mit Fehler abgelehnt.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E4: Volatilität und Risikokennzahlen
**Definition of Done:** Berechnet werden Bollinger-Bänder, ATR, einfache und logarithmische Tagesrenditen, annualisierte Volatilität und maximaler Drawdown. Die Werte stimmen mit von Hand gerechneten Referenzwerten überein, und Sonderfälle (konstante Kurse, Reihe mit nur einer Kerze) sind definiert und behandelt.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E5: Support-/Resistance-Niveaus und Pivot-Punkte
**Definition of Done:** Lokale Hochs und Tiefs werden mit einstellbarem Fenster erkannt (Ränder ohne volles Fenster und flache Plateaus korrekt behandelt), nahe beieinanderliegende Preise werden innerhalb einer relativen Toleranz zu Niveaus mit Berührungszahl zusammengefasst, und jedes Niveau wird relativ zum letzten Schlusskurs als Support oder Resistance eingestuft und aufsteigend sortiert. Klassische Pivot-Punkte (P, R1, R2, S1, S2) stimmen mit von Hand gerechneten Werten überein.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E6: Zentralbank-Zinsentscheidungen und Ereignisstudie
**Definition of Done:** Zinsentscheidungen mehrerer Zentralbanken (Zentralbank, Land, Datum, Leitzins) werden über das Interface `IRateSource` und `IHttpClient` von einer Web-API geladen und validiert; Fehlerfälle werden wie in E2 behandelt. Für jedes Wertpapier wird die Kursveränderung am Handelstag nach jeder Entscheidung der Zentralbank seines Landes berechnet. Entscheidungen auf Nicht-Handelstagen werden auf den nächsten Handelstag abgebildet, Entscheidungen außerhalb des Datenzeitraums werden übersprungen, und Wertpapiere ohne passende Zentralbank liefern eine leere Auswertung statt eines Fehlers.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E7: Zwischenspeicher für API-Antworten auf der Festplatte
**Definition of Done:** Antworten der Web-APIs werden lokal gespeichert und beim nächsten Start wiederverwendet, solange sie nicht abgelaufen sind; abgelaufene Einträge werden neu abgerufen. Eine beschädigte oder unvollständige Cache-Datei wird erkannt und ignoriert (Neuabruf statt Absturz), und der Zugriff auf Datum und Ablaufzeit ist über eine austauschbare Uhr testbar. Bei erschöpftem Kontingent oder Netzausfall werden vorhandene, auch abgelaufene, Daten mit entsprechendem Hinweis ausgeliefert.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E8: HTTP-JSON-Schnittstelle
**Definition of Done:** Der Server bietet mindestens Endpunkte für die Liste der Wertpapiere, die Kursdaten eines Wertpapiers, alle Indikatoren inklusive Support/Resistance und die Ereignisstudie. Parameter (Symbol, Zeitraum, Perioden) werden geprüft; unbekannte Symbole, ungültige Parameter und unbekannte Pfade liefern passende HTTP-Statuscodes (404, 400) mit JSON-Fehlermeldung. Anfragen-Behandlung und Berechnung sind vom Netzwerk trennbar und ohne Socket testbar.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E9: Gleichzeitige Anfragen mit thread-sicherem Cache
**Definition of Done:** Der Server bearbeitet mehrere Anfragen parallel in einem Thread-Pool. Berechnete Ergebnisse werden in einem Cache gehalten, auf den mehrere Threads zugreifen; Ergebnisse sind unabhängig von Anzahl und Reihenfolge der Anfragen identisch mit denen der sequentiellen Berechnung, identische Anfragen werden nicht mehrfach berechnet, und der Server beendet sich sauber ohne hängende Threads. Ein Stresstest mit festem Seed läuft in begrenzter Zeit.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E10: Weboberfläche
**Definition of Done:** Eine vom Server ausgelieferte Seite erlaubt die Auswahl eines Wertpapiers und zeigt Kursverlauf, gewählte Indikatoren, Support-/Resistance-Linien und die Ereignisstudie an. Fehlermeldungen der Schnittstelle werden verständlich angezeigt, und die Seite funktioniert ohne externe Dienste (alle Skripte liegen im Repository). Die Prüfung erfolgt über ein manuelles Testprotokoll.
**Metrik (Stufe 2):**
**Test (Stufe 2):**

## Schwierigkeit (Selbsteinschätzung)

| | Dimension | Wert | Begründung |
|---|---|---|---|
| A | Algorithmik und Datenstrukturen (0–3) | 2 | Eigene Algorithmen: Pivot-Erkennung, Clustering, Indikatoren mit Einschwingphasen, Ereignisstudie mit Datumsabbildung. Nichttrivial, aber ohne Optimierung oder Engine. |
| B | Externe Schnittstellen (0–2) | 2 | Netzwerk: Web-APIs als Datenquellen (HTTP-Client, JSON) und eigener HTTP-Server mit JSON-API. |
| C | Nebenläufigkeit und Zeit (0–3) | 2 | Thread-Pool mit geteiltem Cache (geteilter Zustand zwischen Threads). |
| D | Plattform (0–3) | 0 | Desktop/Server. |
| E | Zustand und Persistenz (0–2) | 1 | Antworten werden auf der Festplatte gespeichert und geladen (einfaches Speichern und Laden mit Ablaufzeit). |
| F | Interaktion und Darstellung (0–2) | 2 | Weboberfläche mit interaktivem Diagramm. |

R = Summe + größter Einzelwert = 9 + 2 = **11**
S = min(1, R / 14) = **0,79**

## Antrag auf Ausnahme von der OOP-Anforderung

Nicht erforderlich. Das Projekt ist objektorientiert aufgebaut (Datenmodell, Quellen-Interfaces, Indikator- und Analyse-Klassen, Server).
