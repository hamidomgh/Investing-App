# Entwurf: „Metrik (Stufe 2)“ und „Test (Stufe 2)“ für PROJEKT.md

Vorschläge zum Übernehmen in `PROJEKT.md`. Diese Datei ändert `PROJEKT.md` nicht. Bitte vor dem Kopieren
prüfen und bei Bedarf anpassen. Alle Testnamen und Zahlen beziehen sich auf den Stand vom 2026-11-13
(192 Tests, alle grün; Stand 2026-11-16). Jeder Block hat genau das Format der leeren Felder in `PROJEKT.md`.

Einzelne Eigenschaft prüfen: `ctest --test-dir build -R "<Muster>"` (das Muster steht jeweils unter „Test“).
Alles prüfen: `cmake -B build && cmake --build build && ctest --test-dir build`.

---

### E1: Datenmodell mit Validierung
**Metrik (Stufe 2):** Jede der 7 Regelverletzungen (High < Low, Open außerhalb Low..High, Close außerhalb Low..High, negatives Volumen, gleiches Datum, älteres Datum, leeres Feld eines Wertpapiers) wird abgelehnt: 100 % der Fehlerfälle werfen `std::invalid_argument`, und bei Kerzen und Kursreihen nennt die Meldung das betroffene Datum (`YYYY-MM-DD`). Gültige Eingaben werden zu 100 % angenommen.
**Test (Stufe 2):** 18 GoogleTest-Fälle in `tests/test_kursdaten.cpp` (`-R "^(Kerze|Kursreihe|Wertpapier)\."`), u. a. `Kerze.UngueltigeHighLow`, `Kerze.UngueltigeOpen`, `Kerze.UngueltigeClose`, `Kerze.FehlermeldungNenntDatum`, `Kursreihe.DoppeltesDatumImKonstruktorWirdAbgelehnt`, `Kursreihe.NichtAufsteigendImKonstruktorNenntDatum`, `Kursreihe.HinzufuegenLehntDoppeltesDatumAb`, `Kursreihe.HinzufuegenLehntAelteresDatumAb`, `Wertpapier.LeereFelderWerdenAbgelehnt`.

### E2: Kursdaten über eine Web-API
**Metrik (Stufe 2):** Alle 7 Fehlerarten (Netzwerk, Zeitüberschreitung, HTTP 401/404, Kontingent 429 bzw. Hinweis mit Status 200, ungültiges JSON, fehlendes Feld, fehlender API-Key) werden einer eigenen Fehlerklasse mit verständlicher Meldung zugeordnet. Bei fehlendem Key werden 0 Anfragen gestellt. Aufgezeichnete echte Antworten werden vollständig und korrekt in validierte Kerzen umgewandelt (IBM mit geprüften Einzelwerten); für alle 6 Wertpapiere aus 5 Ländern liegen echte Daten vor und lassen sich laden. Von 8 Katalogeinträgen mit 5 Fehlern werden die 3 gültigen übernommen, und von 6 Wertpapieren mit 4 Ladefehlern werden die 2 übrigen geladen (alle 6 werden angefragt).
**Test (Stufe 2):** 41 GoogleTest-Fälle offline mit `FakeHttpClient` und Testdaten in `tests/fixtures/alphavantage/` und `tests/fixtures/katalog/` (`-R "^(AlphaVantageTest|ApiSchluessel|PruefeStatus|Quellenfehler|FakeHttpClient|Katalog|Kursabruf|DateiQuelle)\."`), u. a. `AlphaVantageTest.FehlenderKeyVorDerErstenAnfrage`, `AlphaVantageTest.HttpFehlerstatus`, `AlphaVantageTest.KontingentHinweisMitStatus200`, `AlphaVantageTest.UngueltigeAntwortenWerdenAlsParseFehlerGemeldet`, `AlphaVantageTest.AufgezeichneteEchteAntwort`, `Katalog.FehlerhafteEintraegeVerhindernNichtDieUebrigen`, `Katalog.NeuesLandOhneCodeaenderung`, `Kursabruf.FehlerEinesWertpapiersVerhindertNichtDieUebrigen`, `OfflineDaten.JedesWertpapierDesKatalogsIstLadbar`. Der Live-Zugriff ist noch nicht eingebaut; die echte Antwort wurde von Hand aufgezeichnet (siehe `tests/fixtures/alphavantage/README.md`).

### E3: Trend- und Momentum-Indikatoren
**Metrik (Stufe 2):** Für SMA, EMA, MACD (Linie, Signal, Histogramm) und RSI weicht jeder geprüfte Wert höchstens 10⁻⁹ vom von Hand gerechneten Referenzwert ab (Rechenweg als Kommentar im Test). Werte vor Ende der Einschwingphase sind zu 100 % als „nicht verfügbar“ (`std::nullopt`) gekennzeichnet. Alle ungültigen Perioden (≤ 0, MACD schnell ≥ langsam) und zu kurzen Reihen werden abgelehnt; genau passende Mindestlängen werden angenommen.
**Test (Stufe 2):** 10 GoogleTest-Fälle in `tests/test_trendindikatoren.cpp` (`-R "^Trendindikatoren\."`): `SmaReferenzwerte`, `SmaPeriodeEinsIstDerKurs`, `SmaFehlerfaelle`, `EmaReferenzwerte`, `EmaFehlerfaelle`, `MacdReferenzwerte`, `MacdFehlerfaelle`, `RsiReferenzwerte`, `RsiSonderfaelle`, `RsiFehlerfaelle`.

### E4: Volatilität und Risikokennzahlen
**Metrik (Stufe 2):** Bollinger-Bänder, ATR, einfache und logarithmische Renditen, annualisierte Volatilität und maximaler Drawdown weichen höchstens 10⁻⁹ von den Handwerten ab. Für jede Kennzahl sind die Sonderfälle konstante Kurse, eine Kerze und (wo sinnvoll) leere Reihe getestet und liefern das dokumentierte Ergebnis (z. B. Volatilität 0 bzw. „nicht verfügbar“, Drawdown 0, Bänder gleich der Mitte). Kurse ≤ 0 werden bei allen 4 Kennzahlen, die durch Kurse teilen, abgelehnt.
**Test (Stufe 2):** 14 GoogleTest-Fälle in `tests/test_risikokennzahlen.cpp` (`-R "^Risikokennzahlen\."`), u. a. `BollingerReferenzwerte`, `BollingerKonstanteKurse`, `AtrReferenzwerte` (mit Kurslücke), `AtrEineKerze`, `RenditenReferenzwerte`, `RenditenEineKerze`, `VolatilitaetReferenzwert`, `VolatilitaetSonderfaelle`, `MaxDrawdownReferenzwert`, `MaxDrawdownSonderfaelle`, `RenditenLehnenNichtPositiveKurseAb`.

### E5: Support-/Resistance-Niveaus und Pivot-Punkte
**Metrik (Stufe 2):** Lokale Hochs und Tiefs werden für Fenster 1 und 2 exakt an den erwarteten Positionen erkannt. Ränder ohne volles Fenster liefern 0 Extrema, und ein Plateau gleicher Werte zählt genau einmal. Niveaus haben für Toleranz 0 %, 2 % und 10 % genau die von Hand bestimmten Preise und Berührungszahlen, sind zu 100 % aufsteigend sortiert und richtig als Support (≤ letzter Schlusskurs) bzw. Resistance eingestuft. P, R1, R2, S1, S2 weichen höchstens 10⁻⁹ von den Handwerten ab.
**Test (Stufe 2):** 16 GoogleTest-Fälle in `tests/test_supportresistance.cpp` (`-R "^SupportResistance\."`), u. a. `ExtremaFensterEins`, `ExtremaFensterZwei`, `ExtremaRaenderWerdenUebersprungen`, `ExtremaPlateauZaehltEinmal`, `ExtremaPlateauOhneAbfallIstKeinHoch`, `ExtremaPlateauBeiTiefs`, `NiveausReferenzwerte`, `NiveausGroessereToleranz`, `NiveausToleranzNull`, `NiveauAufLetztemKursIstSupport`, `PivotReferenzwerte`.

### E6: Zentralbank-Zinsentscheidungen und Ereignisstudie
**Metrik (Stufe 2):** Aus den aufgezeichneten echten FRED-Zeitreihen (2015–2026) werden für die Fed 32 und für die EZB 23 Zinsänderungen abgeleitet, in Übereinstimmung mit den Änderungstagen der Daten. Die Fehlerfälle wie in E2 (fehlender Key mit 0 Anfragen, Netzwerk, HTTP 400/404, 429, ungültiges JSON, fehlende Felder) werden zu 100 % unterschieden. In der Ereignisstudie stimmen die Kursveränderungen exakt mit Handwerten überein, z. B. echte Daten: Fed 4,00 % wirksam am Do 2026-09-17 → Reaktionstag Fr 2026-09-18, IBM 237,75 → 229,55 = −3,45 %; EZB 2,50 % wirksam am Mi 2026-09-16 → Do 2026-09-17, Mercedes-Benz (Deutschland) 45,365 → 46,195 = +1,83 %, genau ein Ereignis trotz 21 Euro-Ländern. Entscheidungen an Nicht-Handelstagen landen zu 100 % auf dem nächsten Handelstag, Entscheidungen außerhalb der Daten werden zu 100 % übersprungen, und Wertpapiere ohne passende Zentralbank liefern eine leere Liste (0 Fehler). Dokumentierte Abweichung: Das Datum ist der Tag der Wirksamkeit (FRED), nicht der Sitzungstag.
**Test (Stufe 2):** 31 GoogleTest-Fälle in `tests/test_ereignisstudie.cpp` und `tests/test_fred.cpp` (`-R "^(Ereignisstudie|Zinsentscheidung|Zentralbank|FredQuelle|FredTest|DateiZinsquelle|Zinsabruf|KatalogZentralbanken|FredBisEreignisstudie|FredAufgezeichnet)\."`), u. a. `Ereignisstudie.NichtHandelstagWirdAufNaechstenHandelstagAbgebildet`, `Ereignisstudie.AusserhalbDesZeitraumsWirdUebersprungen`, `Ereignisstudie.NurZentralbankDesEigenenLandes`, `FredQuelle.LeiteEntscheidungenAb`, `FredTest.Fehlerfaelle`, `FredTest.FehlenderKeyVorDerErstenAnfrage`, `Zinsabruf.FehlerEinerBankVerhindertNichtDieUebrigen`, `FredAufgezeichnet.FedUndEzb`, `FredAufgezeichnet.EreignisstudieMitEchtenDaten`, `FredAufgezeichnet.EzbUndDeutscheAktie`.

### E7: Zwischenspeicher für API-Antworten auf der Festplatte
**Metrik (Stufe 2):** Innerhalb der Gültigkeit (24 h) wird eine Antwort ohne erneute Anfrage geliefert, auch von einem neu erzeugten Cache-Objekt („nächster Start“). Ab genau 24 h wird neu abgerufen. Alle 6 getesteten Beschädigungsarten (leere Datei, falsche Kennung, abgeschnittener Kopf, abgeschnittener Inhalt, zusätzliche Bytes, veränderter Inhalt) sowie eine Datei einer anderen URL werden erkannt: kein Absturz, genau 1 neue Anfrage, Datei danach repariert. Bei Netzausfall, Zeitüberschreitung und erschöpftem Kontingent (HTTP 429 und Hinweis mit Status 200) werden zu 100 % die alten Daten mit `veraltet = true` und einem Hinweis mit Abrufzeitpunkt geliefert. Die Zeit wird ausschließlich über eine austauschbare Uhr (`IUhr`/`FakeUhr`) bestimmt, und 0 Cache-Dateien enthalten den API-Key oder die URL.
**Test (Stufe 2):** 18 GoogleTest-Fälle in `tests/test_http_cache.cpp` (`-R "^(CacheTest|AlphaVantageBewertung|StandardBewertung)\."`), u. a. `TrefferOhneNeueAnfrage`, `UeberlebtNeustart`, `AbgelaufenerEintragWirdNeuAbgerufen`, `BeschaedigteDateienWerdenIgnoriert`, `DateiEinerAnderenUrlWirdNichtVerwendet`, `NetzwerkfehlerLiefertAlteDatenMitHinweis`, `KontingentLiefertAlteDatenMitHinweis`, `AlphaVantageKontingentMitStatus200`, `KeinApiKeyAufDerPlatte`.

### E8: HTTP-JSON-Schnittstelle
**Metrik (Stufe 2):** Die 4 Endpunkte `/api/wertpapiere`, `/api/kurse`, `/api/indikatoren` und `/api/ereignisstudie` liefern Status 200 mit JSON, und ihre Werte sind identisch mit dem direkten Aufruf der Analyse-Klassen. Alle 13 getesteten ungültigen Parameter liefern 400 mit JSON-Fehlermeldung, die den Parameter nennt, und unbekannte Symbole sowie unbekannte Pfade liefern 404. Bei ungültigen Parametern und unbekannten Symbolen werden 0 Kursdaten geladen. Die gesamte Anfragebehandlung ist ohne Socket getestet; zusätzlich wurde der echte Server mit `curl` geprüft (siehe `docs/E8_Teil2_Server.md`, Abschnitt 7).
**Test (Stufe 2):** 15 GoogleTest-Fälle ohne Socket in `tests/test_anfragebehandlung.cpp` (`-R "^ApiTest\."`), u. a. `UnbekannterPfad`, `NurGetErlaubt`, `Wertpapierliste`, `KurseParameterfehler`, `UnbekanntesSymbol`, `IndikatorenMitStandardwerten`, `IndikatorenParameterfehler`, `IndikatorenZuWenigDaten`, `FehlerDerQuelle`, `Ereignisstudie`; dazu über den echten Server `HttpServer.StatischeDateienUndJsonFuerAnderePfade` (JSON-404 auch außerhalb von `/api`).

### E9: Gleichzeitige Anfragen mit thread-sicherem Cache
**Metrik (Stufe 2):** Im Stresstest mit festem Seed (20261113) werden 2000 zufällige Anfragen mit 1, 4 und 16 Threads in jeweils anderer Reihenfolge bearbeitet. Die Ergebnisse sind in 3 × 2000 Fällen byte-identisch mit der sequentiellen Berechnung, jede verschiedene Anfrage wird genau einmal berechnet, und die Laufzeit liegt bei etwa 0,25 s (Grenze im Test 30 s, CTest-Timeout 60 s). Über echte lokale Sockets erhalten 8 parallele Clients mit 320 Anfragen 0 Abweichungen bei genau 8 Berechnungen. 4 Anfragen sind nachweislich gleichzeitig in Bearbeitung. Nach dem Stoppen endet der Server innerhalb von 10 s, auch während einer laufenden Anfrage, die noch vollständig beantwortet wird. Alle Server- und Stresstests waren in 10 Wiederholungen grün.
**Test (Stufe 2):** 22 GoogleTest-Fälle in `tests/test_nebenlaeufigkeit.cpp` und `tests/test_http_server.cpp` (`-R "^(ThreadPool|ErgebnisCache|GecachteAnfragebehandlung|SynchronisierteQuelle|Stresstest|HttpServer)\."`, das Muster enthält zusätzlich den E8-Test `HttpServer.StatischeDateienUndJsonFuerAnderePfade`), u. a. `Stresstest.ParallelGleichSequentiellUndOhneDoppelteBerechnung`, `ErgebnisCache.GleichzeitigeAnfragenNurEinmalBerechnet`, `ThreadPool.ArbeitetParallel`, `ThreadPool.DestruktorBeendetSauber`, `HttpServer.ParalleleClientsBekommenGleicheErgebnisseWieDirekt`, `HttpServer.BearbeitetAnfragenParallel`, `HttpServer.StopptSauberMitLaufenderAnfrage`. Wiederholt: `./build/tests --gtest_filter='Stresstest.*:HttpServer.*' --gtest_repeat=10`. Hinweis: ThreadSanitizer war im Container nicht lauffähig (siehe `docs/E9_Teil1_Nebenlaeufigkeit.md`, Abschnitt 8).

### E10: Weboberfläche
**Metrik (Stufe 2):** Alle 20 Testfälle des manuellen Testprotokolls sind erfüllt: Auswahl, Kursdiagramm, Indikatoren, Support/Resistance, Ereignisstudie, Fehlermeldungen für 400, 422, 502 und „Server nicht erreichbar“, Fenstergröße und dunkles Schema. Im Netzwerk-Reiter des Browsers gehen 100 % der Anfragen an den eigenen Server, und 0 Dateien in `web/` enthalten eine externe Adresse (automatisch geprüft).
**Test (Stufe 2):** Manuelles Testprotokoll `docs/testprotokoll.md` (20 Fälle, durchgeführt am 2026-11-16, alle erfüllt). Ergänzend 2 automatische Tests (`-R "^Weboberflaeche\."`): `Weboberflaeche.WirdVomServerAusgeliefert` (Seite, Skript und Gestaltung mit richtigem Inhaltstyp) und `Weboberflaeche.KeineExternenAdressen`.

---

## Zuordnung aller 192 Tests

Geprüft mit `ctest --test-dir build -N -R "<Muster>"`: Die Muster oben ergeben zusammen genau 192 Tests.

| Eigenschaft | Anzahl Tests |
|---|---|
| E1 | 18 |
| E2 | 41 |
| E3 | 10 |
| E4 | 14 |
| E5 | 16 |
| E6 | 31 |
| E7 | 18 |
| E8 | 15 ohne Socket + 1 über den echten Server (`HttpServer.StatischeDateienUndJsonFuerAnderePfade`) |
| E9 | 22 |
| E10 | 2 + manuelles Testprotokoll |
| Vorlage (`Statistik`, nicht Teil von MarketLens) | 4 |
| **Summe** | **192** |

Das Muster von E9 (`^(…|HttpServer)\.`) liefert 23 Tests, weil es den einen `HttpServer`-Test von E8 mitfasst. Dieser Test
belegt außerdem einen Teil von E10 (Auslieferung statischer Dateien).

## Vor dem Übernehmen prüfen

- **E10:** Das Testprotokoll wurde am 2026-11-16 im Browser durchgeführt, alle 20 Testfälle erfüllt. Die Metrik kann so übernommen werden. (Fall 15 hat für spätere Durchläufe eine neue Anleitung, weil MBG.DEX inzwischen Daten hat.)
- **E2:** Der Live-Zugriff über HTTPS (CMake-Option) ist noch nicht eingebaut. Er wird im Text von `PROJEKT.md` erwähnt, ist aber nicht Teil der E2-DoD.
- **E1:** `Kerze.NegativesVolumenWirdAbgelehnt` prüft dasselbe wie `Kerze.UngueltigesVolumen` (doppelt, schadet nicht).
- Abweichungen vom Text von `PROJEKT.md`, die du kennen solltest: C++20 statt „C++17-Backend“, und E6 verwendet das Datum der Wirksamkeit statt des Sitzungstags (beides in `PLAN.md` bzw. `docs/E6_Teil2_Zinsquelle.md` dokumentiert).
