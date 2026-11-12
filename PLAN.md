# Umsetzungsplan MarketLens

Grundlage: `PROJEKT.md` (E1–E10) und `AGENTS.md` (Regeln). Jeder Schritt endet mit grünem
Build und grünen Tests (`cmake -B build && cmake --build build && ctest --test-dir build`).
Zu jeder Funktionalität gehört ein Test; bestehende Tests werden nicht abgeschwächt.

## Ausgangslage

- Vorhanden: `src/kursdaten.{hpp,cpp}` (`Kerze`, `Kursreihe`, `Wertpapier`, noch nicht committet),
  `src/statistik.*`, `tests/test_kursdaten.cpp`, `tests/test_statistik.cpp`.
- E1 ist angefangen; es fehlt noch die Prüfung der Details (Fehlermeldung mit Datum, Wertpapier-Validierung).
- Offener Punkt: `PROJEKT.md` nennt "C++17-Backend", `AGENTS.md`/`CMakeLists.txt` verwenden C++20.
  Vorschlag: bei C++20 bleiben und `PROJEKT.md` anpassen (bitte bestätigen).
- Header-only-Bibliotheken (HTTP-Server z. B. cpp-httplib, JSON z. B. nlohmann/json) müssen ins Repository
  (`third_party/`). Das sind neue externe Abhängigkeiten → **vorher Rückfrage** (siehe AGENTS.md).

## Reihenfolge und Abhängigkeiten

```
E1 → E3 → E4 → E5            (reine Analyse, ohne Netzwerk)
E1 → E2 → E7 → E6            (Datenbeschaffung)
E3–E6 → E8 → E9 → E10       (Server, Parallelität, UI)
```

Begründung: Zuerst die reine Logik (leicht testbar, kein Netzwerk), dann die Datenquellen
hinter Interfaces, zuletzt Server und Oberfläche.

## Schritte

### Phase 1 – Datenmodell (E1)
1. `Kerze::validieren`: High ≥ Low, Open/Close in [Low, High], Volumen ≥ 0.
2. `Kursreihe`: streng aufsteigende Daten; Fehlermeldung nennt das betroffene Datum (ISO-Format).
3. `Wertpapier`: Symbol/Name/Land/Währung dürfen nicht leer sein.
4. Tests: gültige Reihe, jede Verletzung einzeln, Meldung enthält Datum, doppeltes Datum.

### Phase 2 – Indikatoren (E3, E4, E5)
Struktur: eigene Klassen/Funktionen je Gruppe, Rückgabe als `std::vector<std::optional<double>>`
(`nullopt` = nicht verfügbar in der Einschwingphase).
5. **E3** `Trendindikatoren`: SMA, EMA, MACD (Linie, Signal, Histogramm), RSI (Wilder). Perioden einstellbar;
   Fehler bei Periode ≤ 0, zu kurzer Reihe, MACD mit schnell ≥ langsam.
   Tests mit von Hand gerechneten Referenzwerten (Rechnung im Testkommentar festhalten).
6. **E4** `Risikokennzahlen`: Bollinger-Bänder, ATR, einfache/log. Renditen, annualisierte Volatilität (√252),
   maximaler Drawdown. Sonderfälle: konstante Kurse (Volatilität 0, Drawdown 0), eine Kerze (keine Renditen, definiert behandelt).
7. **E5** `SupportResistance`: lokale Hochs/Tiefs mit Fenster (Ränder ohne volles Fenster ignorieren, Plateaus:
   erstes Element zählt), Clustering mit relativer Toleranz + Berührungszahl, Einstufung relativ zum letzten Close,
   aufsteigend sortiert; klassische Pivot-Punkte P, R1, R2, S1, S2.

### Phase 3 – Netzwerk-Abstraktion und Datenquellen (E2, E6, E7)
8. Interfaces `IHttpClient` (Antwort: Status, Body; Fehler: Netzwerk/Timeout), `IPriceSource`, `IRateSource`;
   `FakeHttpClient` für Tests (liefert aufgezeichnete Antworten aus `tests/fixtures/`).
9. JSON-Einbindung (nach Rückfrage) und Fehlerhierarchie: `NetzwerkFehler`, `HttpStatusFehler` (401/404/429),
   `ParseFehler`, `KonfigurationsFehler` – jeweils mit verständlicher Meldung.
10. **E2** `PriceSource`-Implementierung: API-Key aus Umgebungsvariable (fehlt → Fehler vor der ersten Anfrage),
    JSON → validierte `Kerze`n. Katalog `katalog.json` (Symbol, Name, Land, Währung); fehlerhafte Einträge werden
    gemeldet und übersprungen, übrige werden ausgewertet. Tests für alle Fehlerfälle.
11. **E7** `Cache` hinter dem HTTP-Client (Decorator `CachingHttpClient`): Dateien auf Platte mit Ablaufzeit,
    Interface `IUhr` (+ `FakeUhr`), beschädigte Datei → ignorieren und neu abrufen, bei Netzfehler/429 abgelaufene
    Daten mit Hinweis `veraltet=true` ausliefern.
12. **E6** `RateSource` (Zinsentscheidungen: Zentralbank, Land, Datum, Leitzins, validiert; gleiche Fehlerbehandlung
    wie E2) und `Ereignisstudie`: Entscheidung auf Nicht-Handelstag → nächster Handelstag, außerhalb des Zeitraums
    überspringen, Land ohne Zentralbank → leere Auswertung.
13. Fixtures: aufgezeichnete Antworten einmal manuell abrufen, in `tests/fixtures/` ablegen (ohne API-Key im Repo).
14. CMake-Option `MARKETLENS_LIVE_HTTP` (Standard AUS) für echten HTTPS-Client; Standardbuild bleibt netzfrei.

### Phase 4 – Server (E8, E9)
15. **E8** `Anfragebehandlung` getrennt vom Socket: Funktion `Antwort behandle(const Anfrage&)` mit Endpunkten
    `/api/wertpapiere`, `/api/kurse`, `/api/indikatoren`, `/api/ereignisstudie`; Parameterprüfung →
    400/404 mit JSON-Fehler. Tests ohne Socket.
16. Dünne Netzwerkschicht (HTTP-Server-Bibliothek, nach Rückfrage) ruft nur `behandle` auf; statische Dateien
    aus `web/` ausliefern.
17. **E9** Thread-Pool + thread-sicherer Ergebnis-Cache (`std::shared_mutex`; identische Anfragen nur einmal
    berechnen, z. B. über `std::shared_future` pro Schlüssel). Tests: parallele Ergebnisse == sequentielle,
    Zähler der Berechnungen, sauberes Beenden (Join). Stresstest mit festem Seed und Zeitlimit.
    Zusätzlich Lauf mit `-DENABLE_SANITIZERS=ON` (ThreadSanitizer-Variante erwägen).

### Phase 5 – Weboberfläche (E10)
18. `web/index.html` + `web/app.js` + `web/style.css`, alle Skripte lokal (Diagramm selbst per Canvas/SVG
    zeichnen, keine CDN). Auswahl des Wertpapiers, Kurslinie, Indikatoren, S/R-Linien, Ereignisstudie, Fehleranzeige.
19. Manuelles Testprotokoll `docs/testprotokoll.md` (Schritte, erwartetes Ergebnis, Ergebnis/Datum) – auch für den Live-Zugriff.

### Phase 6 – Abschluss
20. `PROJEKT.md`: bei jeder Eigenschaft **Metrik (Stufe 2)** und **Test (Stufe 2)** ausfüllen (verweist auf konkrete Testnamen).
21. `AGENTS.md` (Aufbau) und `README.md` aktualisieren; Bauen im frischen Container prüfen.

## Dateistruktur (Ziel)

```
src/   kursdaten, trendindikatoren, risiko, supportresistance, http_client, quellen,
       cache, uhr, ereignisstudie, anfragebehandlung, threadpool, server, main
tests/ ein test_*.cpp je Modul, tests/fixtures/ für aufgezeichnete Antworten
web/   index.html, app.js, style.css
third_party/  Header-only-Bibliotheken (nach Rückfrage)
docs/  testprotokoll.md
```

## Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| Kostenloses API-Kontingent / Nutzungsbedingungen | Cache (E7), Fixtures, Anbieter vor Phase 3 festlegen |
| Zinsdaten-API schwer zu finden | Anbieter früh recherchieren; Fallback: Zinsdaten als versionierte JSON-Datei hinter `IRateSource` |
| Race Conditions | Sanitizer-Lauf, Stresstest mit festem Seed |
| Referenzwerte falsch von Hand gerechnet | Rechnung im Testkommentar, im Zweifel zweite unabhängige Rechnung |

## Offene Fragen an dich

1. C++17 (PROJEKT.md) oder C++20 (AGENTS.md/CMake)?
2. Freigabe für Header-only-Bibliotheken (vorgeschlagen: nlohmann/json, cpp-httplib)?
3. Welche Kursdaten-API (z. B. Alpha Vantage, Twelve Data) und Zinsdaten-Quelle?
