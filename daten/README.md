# Offline-Kursdaten

Gespeicherte Antworten von Alpha Vantage (`TIME_SERIES_DAILY`), eine Datei pro Wertpapier:
`<SYMBOL>.json`, z. B. `IBM.json`. Der Server liest sie mit `DateiQuelle`, solange es den echten
HTTPS-Zugriff (CMake-Option) noch nicht gibt.

| Datei | Inhalt |
|---|---|
| `IBM.json` | IBM, 100 Handelstage bis 2026-11-13 (Kopie von `tests/fixtures/alphavantage/aufgezeichnet_IBM.json`) |

Weitere Wertpapiere aus `config/katalog.json` hinzufügen (außerhalb des Containers, eigener Key):

    curl "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=MBG.DEX&outputsize=compact&apikey=$ALPHAVANTAGE_API_KEY" -o daten/MBG.DEX.json

Danach `grep -ci apikey daten/MBG.DEX.json` prüfen (muss `0` sein). Fehlt eine Datei, antwortet der
Server für dieses Wertpapier mit 502 und einer verständlichen Meldung; die anderen funktionieren weiter.
