# Testdaten für Twelve Data (F3)

Antworten im Format von `time_series` (`interval=1day`). Werden vom `FakeHttpClient` bzw. von `DateiQuelle`
geliefert, damit die Tests ohne Netzwerk und ohne API-Key laufen.

| Datei | Fall |
|---|---|
| `aufgezeichnet_IBM.json` | **echte** Antwort für IBM, aufgezeichnet am 2026-11-16: 100 Handelstage 2026-06-25 bis 2026-11-13, neuester zuerst. Kopie von `daten/twelvedata/IBM.json`, ohne API-Key (geprüft) |
| `aufgezeichnet_AAPL.json` | **echte** Antwort für Apple, ebenso |
| `gueltig.json` | 3 Tage, neuester zuerst (wird aufsteigend sortiert) |
| `fehler_401.json` | ungültiger API-Key (`code` 401) |
| `fehler_404_tarif.json` | Symbol nur im bezahlten Tarif (`code` 404); Text aus der echten Antwort für MBG auf XETR (2026-11-16) |
| `fehler_429.json` | Kontingent pro Minute erschöpft (`code` 429) |
| `fehler_400.json` | ungültige Anfrage (`code` 400) |
| `ohne_values.json` | Liste `values` fehlt |
| `fehlendes_feld.json` | `close` fehlt |
| `keine_zahl.json` | `close` ist `"n/a"` |
| `datum_mit_uhrzeit.json` | Datum mit Uhrzeit (kein Tageskurs) |
| `inkonsistente_kerze.json` | High unter Low (wird von E1 abgelehnt) |
| `doppelter_tag.json` | derselbe Tag zweimal |
| `abgeschnitten.json` | abgeschnittenes JSON |

Ändern sich die aufgezeichneten Dateien, müssen die Erwartungen in `TwelveDataAufgezeichnet.*` angepasst werden.
