# Features

Beschreibungen neuer Funktionen, **bevor** sie gebaut werden. Jede Datei beschreibt Ziel, Umfang, Verhalten,
betroffene Dateien, offene Entscheidungen und die Prüfung. Erst wenn eine Beschreibung bestätigt ist, wird sie
umgesetzt; danach wird der Status hier aktualisiert.

Diese Funktionen gehen über die Definitions of Done in `PROJEKT.md` hinaus. Sie dürfen keine bestehende
Eigenschaft (E1–E10) und keinen bestehenden Test verschlechtern.

| Nr. | Feature | Bereich | Status |
|---|---|---|---|
| F1 | [Zeichenwerkzeuge im Kursdiagramm](F1_Zeichenwerkzeuge.md) | nur Weboberfläche (`web/`) | **umgesetzt** (2026-11-16), Erklärung `docs/F1_Zeichenwerkzeuge.md`; Browsertest F1-1…F1-16 am 2026-11-16 bestanden |
| F2 | [Erklärungen („?“) zu allen Zahlen](F2_Erklaerungen.md) | nur Weboberfläche (`web/`) | **umgesetzt** (2026-11-16), Erklärung `docs/F2_Erklaerungen.md`; Browsertest F2-1…F2-17 am 2026-11-16 bestanden |
| F3 | [Zweiter Anbieter Twelve Data, zwei Server nebeneinander](F3_Zweiter_Anbieter.md) | Server und Weboberfläche | **offline umgesetzt** (2026-11-16, Zweig `zweiter-anbieter`), Erklärung `docs/F3_ZweiterAnbieter.md`; Browsertest F3-1…F3-10 am 2026-11-16 bestanden; Live-Client folgt |
| F5 | [Live-Zugriff auf beide Anbieter](F5_Live_Zugriff.md) | Server (`main`, Netzwerkschicht) | **umgesetzt, Live-Test offen** (2026-11-19, Zweig `live-zugriff`): Teil 1 `LiveHttpClient` (`docs/F5_Teil1_LiveClient.md`), Teil 2 Verdrahtung in `main` (`docs/F5_Teil2_Verdrahtung.md`); Testfälle F5-1…F5-8 brauchen das neue Image mit `libssl-dev` |

## Vorlage für neue Features

1. Ziel (ein, zwei Sätze, aus Sicht des Benutzers)
2. Umfang: was dazugehört und was ausdrücklich nicht
3. Verhalten im Detail
4. Betroffene Dateien (und ob Server/C++ betroffen sind)
5. Offene Entscheidungen mit Empfehlung
6. Prüfung: automatische Tests und/oder neue Fälle im Testprotokoll
