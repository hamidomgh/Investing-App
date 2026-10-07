# Hinweise für KI-Agenten

Diese Datei liest der Agent (z.B. OpenCode) zu Beginn jeder Sitzung. Sie ist das
"Langzeitgedächtnis" des Projekts: Was hier steht, muss nicht in jeder Sitzung neu
erklärt werden. Passt sie an euer Projekt an und haltet sie aktuell.

## Projekt

<!-- 2–3 Sätze: Was macht das Programm? Details stehen in PROJEKT.md. -->

Ziel, Eigenschaften und Definitions of Done stehen in `PROJEKT.md`.

## Bauen und Testen

Alles läuft im Dev-Container (`./dev.sh`). Bauen und Testen ausschließlich mit:

```
cmake -B build && cmake --build build && ctest --test-dir build
```

- Neue `.cpp`-Dateien und Testdateien in `CMakeLists.txt` eintragen.
- Tests mit GoogleTest in `tests/`. Programmlogik liegt in der Bibliothek `projekt_lib`,
  damit sie ohne `main` testbar ist.
- Ein Pre-Commit-Hook baut und testet vor jedem Commit.

## Regeln

- C++20, objektorientiert. Keine neuen externen Abhängigkeiten ohne Rückfrage.
- Kleine Schritte: Jede Änderung endet mit grünem Build und grünen Tests.
- **Bestehende Tests nicht abschwächen, löschen oder deaktivieren**, um sie grün zu
  bekommen. Schlägt ein Test fehl, liegt der Fehler zuerst im Code. Wenn ein Test
  wirklich falsch ist: anhalten, begründen und auf Bestätigung warten.
- Zu jeder neuen Funktionalität gehört ein Test.
- Keine Commits ohne Aufforderung.
- Änderungen kurz erklären: Was wurde geändert, und warum? Ich muss den Code in der
  Prüfung selbst erklären können.

## Aufbau

<!-- Wichtige Klassen und Dateien, je eine Zeile. Mit dem Projekt mitwachsen lassen. -->

- `src/main.cpp`: Einstiegspunkt
- `tests/`: GoogleTest-Tests
