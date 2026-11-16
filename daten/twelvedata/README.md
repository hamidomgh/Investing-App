# Offline-Kursdaten von Twelve Data (F3)

Gespeicherte Antworten von Twelve Data (`time_series`, `interval=1day`, `outputsize=100`), eine Datei pro
Wertpapier: `<SYMBOL>.json`. Der Server mit `--quelle twelvedata` liest sie mit `DateiQuelle` und
`TwelveDataQuelle::parse`. Im kostenlosen Tarif liefert Twelve Data nur US-Wertpapiere; der Katalog führt deshalb nur
IBM und AAPL mit einem Eintrag `"twelvedata"` in `symbole`.

| Datei | Inhalt |
|---|---|
| `IBM.json` | IBM, 100 Handelstage 2026-06-25 bis 2026-11-13, aufgenommen 2026-11-16 (auch als `tests/fixtures/twelvedata/aufgezeichnet_IBM.json`) |
| `AAPL.json` | Apple, ebenso |

Beide Dateien enthalten keinen API-Key (geprüft mit `grep -ci apikey`). Die Schlusskurse weichen von denen in
`daten/alphavantage/` höchstens um 0,01 ab (Test `TwelveDataAufgezeichnet.GleicheKurseWieAlphaVantage`).

Neu aufnehmen (außerhalb des Containers, eigener Key, höchstens 8 Anfragen pro Minute):

    curl -s "https://api.twelvedata.com/time_series?symbol=IBM&interval=1day&outputsize=100&apikey=$TWELVEDATA_API_KEY" -o daten/twelvedata/IBM.json
