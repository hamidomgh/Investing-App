# Offline-Daten

| Ordner | Inhalt | benutzt von |
|---|---|---|
| `alphavantage/` | Kurse von Alpha Vantage, alle 6 Wertpapiere | Server mit `--quelle alphavantage` (Standard) |
| `twelvedata/` | Kurse von Twelve Data, nur US-Wertpapiere (IBM, AAPL) | Server mit `--quelle twelvedata` (F3) |
| `zinsen/` | Leitzinsen von FRED (Fed, EZB) | **beide** Server (nur lesend) |

Jeder Anbieter hat seinen eigenen Ordner, damit sich die Daten zweier Server, die nebeneinander laufen, nicht stören.
