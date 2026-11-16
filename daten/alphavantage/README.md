# Offline-Kursdaten von Alpha Vantage

Gespeicherte Antworten von Alpha Vantage (`TIME_SERIES_DAILY`), eine Datei pro Wertpapier:
`<SYMBOL>.json`, z. B. `IBM.json`. Seit F3 liegen sie in `daten/alphavantage/` (Twelve Data: `daten/twelvedata/`).
Der Server mit `--quelle alphavantage` (Standard) liest sie mit `DateiQuelle`, solange es den echten
HTTPS-Zugriff (CMake-Option) noch nicht gibt.

| Datei | Inhalt |
|---|---|
| `IBM.json` | IBM, 100 Handelstage bis 2026-11-13, aufgenommen 2026-11-13 (auch als `tests/fixtures/alphavantage/aufgezeichnet_IBM.json`) |
| `AAPL.json` | Apple, 100 Handelstage 2026-06-25 bis 2026-11-13, aufgenommen 2026-11-16 |
| `MBG.DEX.json` | Mercedes-Benz (Xetra), 100 Handelstage 2026-06-29 bis 2026-11-13, aufgenommen 2026-11-16 (auch als `tests/fixtures/alphavantage/aufgezeichnet_MBG.DEX.json`) |
| `TSCO.LON.json` | Tesco (London), 100 Handelstage 2026-06-26 bis 2026-11-13, aufgenommen 2026-11-16. **Kurse in Pence**, deshalb Währung `GBX` im Katalog |
| `SHOP.TRT.json` | Shopify (Toronto), 100 Handelstage 2026-06-23 bis 2026-11-13, aufgenommen 2026-11-16 |
| `RELIANCE.BSE.json` | Reliance Industries (Bombay), 100 Handelstage 2026-06-22 bis 2026-11-13, aufgenommen 2026-11-16 |

Alle Dateien enthalten keinen API-Key (geprüft mit `grep -ci apikey`). Der Test
`OfflineDaten.JedesWertpapierDesKatalogsIstLadbar` prüft, dass sich jedes Wertpapier des Katalogs laden lässt.

Weitere Wertpapiere hinzufügen oder neu aufnehmen (außerhalb des Containers, eigener Key):

    curl "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=MBG.DEX&outputsize=compact&apikey=$ALPHAVANTAGE_API_KEY" -o daten/alphavantage/MBG.DEX.json

Danach `grep -ci apikey daten/alphavantage/MBG.DEX.json` prüfen (muss `0` sein). Fehlt eine Datei, antwortet der
Server für dieses Wertpapier mit 502 und einer verständlichen Meldung; die anderen funktionieren weiter.
