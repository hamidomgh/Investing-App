# Testdaten für FRED (Zinsentscheidungen, E6)

Handgeschriebene Antworten im Format von `fred/series/observations` (`file_type=json`). Die Werte in
`fed_ok.json` sind den Zinssenkungen der Fed 2024 nachempfunden (Obergrenze des Zielbands): 5,50 % →
5,00 % wirksam ab 2024-09-19, → 4,75 % wirksam ab 2024-11-08. Es ist **keine** aufgezeichnete Antwort.

| Datei | Fall |
|---|---|
| `fed_ok.json` | gültig; enthält einen Tag ohne Wert (`"."`) und zwei vertauschte Tage (wird sortiert) |
| `fehler_api_key.json` | Inhalt einer 400-Antwort bei ungültigem API-Key |
| `serie_unbekannt.json` | Inhalt einer 400-Antwort bei unbekannter Zeitreihe |
| `ohne_observations.json` | Feld `observations` fehlt |
| `keine_zahl.json` | Wert `"abc"` |
| `ungueltiges_datum.json` | 2024-02-30 |
| `doppeltes_datum.json` | derselbe Tag zweimal |
| `feld_fehlt.json` | Beobachtung ohne `value` |
| `abgeschnitten.json` | abgeschnittenes JSON |

Zusätzlich **aufgezeichnete echte Antworten** (2026-11-13, `observation_start=2015-01-01`, ohne API-Key, geprüft):

| Datei | Inhalt |
|---|---|
| `aufgezeichnet_DFEDTARU.json` | Fed, 4335 Tage, 32 Änderungen, letzte: 2026-09-17 auf 4,00 % |
| `aufgezeichnet_ECBDFR.json` | EZB, 4335 Tage, 23 Änderungen, erster Wert −0,20 %, letzte: 2026-09-16 auf 2,50 % |

Es sind Kopien von `daten/zinsen/`. Getrennt abgelegt, damit die Tests gleich bleiben, wenn `daten/zinsen/`
neu aufgenommen wird. Ändern sich die Dateien hier, müssen die Erwartungen in `FredAufgezeichnet.*` angepasst werden.
