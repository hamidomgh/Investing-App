# Externe Header-only-Bibliotheken

Diese Bibliotheken liegen im Repository, damit der Standard-Build ohne Netzwerk und ohne
Paketinstallation funktioniert (siehe `PROJEKT.md`). Freigegeben am 2026-11-13 (siehe `PLAN.md`).

| Datei | Bibliothek | Version | Lizenz | Quelle | SHA-256 |
|---|---|---|---|---|---|
| `nlohmann/json.hpp` | JSON for Modern C++ (Niels Lohmann) | 3.11.3 | MIT | https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp | `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6` |
| `httplib.h` | cpp-httplib (Yuji Hirose) | 0.18.3 | MIT | https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.3/httplib.h | `a0a0c13dc086663863dbe6730a19716f7d3744904b6205496e8301750814dbd5` |

Prüfung der Prüfsummen: Für `nlohmann/json.hpp` stimmt der Wert mit der vom Projekt veröffentlichten
Prüfsumme überein. Für `httplib.h` veröffentlicht das Projekt keine Prüfsumme der Einzeldatei; der Wert oben
wurde beim Herunterladen vom Git-Tag `v0.18.3` selbst berechnet (2026-11-13) und dient zum späteren Vergleich.

Die Dateien werden nicht verändert. Prüfen: `sha256sum third_party/nlohmann/json.hpp`.
