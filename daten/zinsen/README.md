# Offline-Zinsdaten (FRED)

Gespeicherte Antworten von FRED (`fred/series/observations`), eine Datei pro Zeitreihe aus
`config/katalog.json` (`fredSerie`): `<SERIE>.json`. Der Server lädt sie beim Start mit `DateiZinsquelle`
und leitet daraus die Zinsentscheidungen für die Ereignisstudie ab.

| Datei | Zentralbank | Zeitreihe |
|---|---|---|
| `DFEDTARU.json` | Fed | Federal Funds Target Range – Upper Limit, täglich (aufgenommen 2026-11-13) |
| `ECBDFR.json` | EZB | ECB Deposit Facility Rate, täglich (aufgenommen 2026-11-13) |

Aufnehmen (außerhalb des Containers, kostenloser Key von https://fredaccount.stlouisfed.org/apikeys):

    export FRED_API_KEY=...
    curl "https://api.stlouisfed.org/fred/series/observations?series_id=DFEDTARU&api_key=$FRED_API_KEY&file_type=json&observation_start=2015-01-01" -o daten/zinsen/DFEDTARU.json
    curl "https://api.stlouisfed.org/fred/series/observations?series_id=ECBDFR&api_key=$FRED_API_KEY&file_type=json&observation_start=2015-01-01" -o daten/zinsen/ECBDFR.json

Danach prüfen: `grep -ci api_key daten/zinsen/*.json` muss für beide Dateien `0` ergeben.

Hinweis: Das Datum ist der Tag, an dem der neue Zins **wirksam** wird, nicht der Tag der Sitzung.
Fehlt eine Datei, meldet der Server beim Start eine Warnung; die Ereignisstudie liefert dann für diese
Zentralbank keine Ereignisse.
