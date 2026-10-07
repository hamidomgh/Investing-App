# Projekt: <Titel>

Name: <Vorname Nachname>
Matrikelnummer: <...>

<!--
Diese Datei entsteht in zwei Stufen (siehe Moodle, "Projekt und Prüfung: Rahmenbedingungen"):
  Stufe 1 bis M0 (29.10.2026): Projektziel, 10 Eigenschaften mit Definition of Done,
                               Schwierigkeit, ggf. Antrag auf Ausnahme von OOP.
  Stufe 2 bis 07.01.2027:      zu jeder Definition of Done eine Metrik und ein Test
                               (oder ein manuelles Testprotokoll).
Nach der Freigabe bei M0 sind Eigenschaften und Definitions of Done festgeschrieben.
Kommentare wie dieser dürfen stehen bleiben oder gelöscht werden.
-->

## Projektziel

<!-- ½ bis 1 Seite: Was soll das Programm können? Für wen? Was ist ausdrücklich
     nicht Teil des Projekts? -->


## Eigenschaften und Definitions of Done

<!--
Eine Eigenschaft beschreibt, WAS das Projekt kann. Die Definition of Done beschreibt,
WANN diese Eigenschaft fertig ist, so konkret, dass eine andere Person es nachprüfen kann.

Beispiel (bitte ersetzen):

### E1: Messwerte aus CSV-Dateien lesen
**Definition of Done:** Das Programm liest eine CSV-Datei mit Kopfzeile und beliebig vielen
Datenzeilen. Fehlerhafte Zeilen werden mit Zeilennummer gemeldet und übersprungen; die
übrigen Zeilen werden verarbeitet. Eine leere oder fehlende Datei führt zu einer
verständlichen Fehlermeldung, nicht zu einem Absturz.
**Metrik (Stufe 2):** Für die 6 Testdateien in tests/daten/ stimmen eingelesene Werte und
gemeldete Zeilennummern mit der erwarteten Ausgabe überein.
**Test (Stufe 2):** tests/test_csv_leser.cpp (CsvLeser.*)

Weniger gut: "Das Programm kann CSV-Dateien lesen." (Welche? Was passiert bei Fehlern?)
-->

### E1: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E2: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E3: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E4: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E5: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E6: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E7: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E8: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E9: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**

### E10: <Eigenschaft>
**Definition of Done:**
**Metrik (Stufe 2):**
**Test (Stufe 2):**


## Schwierigkeit (Selbsteinschätzung)

<!-- Werte nach der Tabelle in Moodle. Begründung je Dimension in einem Satz:
     Woran im Projekt macht ihr den Wert fest? -->

| | Dimension | Wert | Begründung |
|---|---|---|---|
| A | Algorithmik und Datenstrukturen (0–3) | | |
| B | Externe Schnittstellen (0–2) | | |
| C | Nebenläufigkeit und Zeit (0–3) | | |
| D | Plattform (0–3) | | |
| E | Zustand und Persistenz (0–2) | | |
| F | Interaktion und Darstellung (0–2) | | |

R = Summe + größter Einzelwert =
S = min(1, R / 14) =


## Antrag auf Ausnahme von der OOP-Anforderung

<!-- Nur falls nötig: Warum ist OOP für dieses Projekt wenig sinnvoll? Wie wird der Code
     stattdessen strukturiert? Sonst diesen Abschnitt löschen. -->
