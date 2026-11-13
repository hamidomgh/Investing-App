# Testdaten für Alpha Vantage

Handgeschriebene Antworten im Format von `TIME_SERIES_DAILY`. Sie werden vom `FakeHttpClient`
geliefert, damit die Tests ohne Netzwerk und ohne API-Key laufen.

| Datei | Fall |
|---|---|
| `daily_ok.json` | gültige Antwort mit 3 Tagen (neuester zuerst, wie bei Alpha Vantage) |
| `error_message.json` | unbekanntes Symbol (`"Error Message"`, HTTP 200) |
| `note_kontingent.json` | Kontingent erschöpft, ältere Form (`"Note"`) |
| `information_kontingent.json` | Kontingent erschöpft, neuere Form (`"Information"` mit "rate limit") |
| `information_sonstiges.json` | sonstiger Hinweis (Premium-Funktion) |
| `ohne_zeitreihe.json` | Feld `"Time Series (Daily)"` fehlt |
| `ungueltiges_json.json` | abgeschnittenes JSON |
| `fehlendes_feld.json` | `"4. close"` fehlt |
| `keine_zahl.json` | `"4. close"` ist `"n/a"` |
| `ungueltiges_datum.json` | Datum 2024-02-30 gibt es nicht |
| `inkonsistente_kerze.json` | High unter Low (wird von E1 abgelehnt) |

Zusätzlich eine **aufgezeichnete echte Antwort**:

| Datei | Fall |
|---|---|
| `aufgezeichnet_IBM.json` | echte Antwort von Alpha Vantage für IBM, `compact`, aufgezeichnet am 2026-11-13: 100 Handelstage (2026-06-25 bis 2026-11-13). Enthält keinen API-Key (geprüft). |

Neu aufnehmen (außerhalb des Containers, eigener Key nur in der Umgebungsvariable):

    curl "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=IBM&outputsize=compact&apikey=$ALPHAVANTAGE_API_KEY" -o aufgezeichnet_IBM.json

Danach prüfen: `grep -ci apikey aufgezeichnet_IBM.json` muss `0` ergeben. Ändern sich die Daten,
müssen die erwarteten Werte im Test `AlphaVantageTest.AufgezeichneteEchteAntwort` angepasst werden.
