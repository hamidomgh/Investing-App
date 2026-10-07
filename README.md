# Projektvorlage FAUP (WiSe 2026/27)

Vorlage für euer Einzelprojekt: CMake-Grundgerüst mit GoogleTest, Dev-Container mit
Compiler, Testwerkzeugen und OpenCode, Pre-Commit-Hook sowie Vorlagen für `PROJEKT.md`
und `AGENTS.md`. Die Rahmenbedingungen für Projekt und Prüfung stehen im Moodle.

## 1. Eigenes Repository anlegen

1. Auf <https://gitlab.rz.htw-berlin.de> ein **neues, leeres, privates** Projekt anlegen
   (ohne README).
2. Vorlage klonen und mit eurem Projekt verbinden:

   ```
   git clone git@gitlab.rz.htw-berlin.de:kerdels/ce23_wise_2026.git meinprojekt
   cd meinprojekt
   git remote rename origin vorlage
   git remote add origin <URL eures Projekts>
   git push -u origin main
   ```

   Die Vorlage ist nur für Angemeldete sichtbar. Für `git@...`-Adressen braucht ihr einen
   SSH-Schlüssel in eurem GitLab-Profil (Einstellungen → SSH-Schlüssel). Alternativ per
   HTTPS (`https://gitlab.rz.htw-berlin.de/kerdels/ce23_wise_2026.git`) mit einem Personal
   Access Token (Bereich `read_repository`, `write_repository`) als Passwort.

   Die Vorlage bleibt als Remote `vorlage` erhalten. Falls sie im Semester aktualisiert
   wird, holt ihr die Änderungen mit `git pull --no-rebase vorlage main`.

## 2. Dev-Container

Voraussetzung ist **Podman** oder **Docker**:

- **Linux:** Podman aus den Paketquellen (empfohlen) oder Docker.
- **macOS:** Podman Desktop oder Docker Desktop.
- **Windows:** WSL2 mit einer Linux-Distribution (z.B. Ubuntu), darin Podman oder Docker
  Desktop mit WSL2-Backend. Das Projekt **innerhalb von WSL** klonen
  (z.B. `~/meinprojekt`), nicht unter `C:\`, und `./dev.sh` in der WSL-Shell ausführen.

Dann im Projektordner:

```
./dev.sh
```

Beim ersten Aufruf wird das Image gebaut (einige Minuten, ca. 1,3 GB). Danach seid ihr in
einer Shell im Container; euer Projekt liegt unter `/work`. Was ihr dort ändert, ändert
sich auch auf eurem Rechner, sodass ihr mit eurem gewohnten Editor arbeiten könnt.

| Befehl | Wirkung |
|---|---|
| `./dev.sh` | Shell im Container; ein zweiter Aufruf öffnet eine weitere Shell im selben Container |
| `./dev.sh <befehl>` | einzelnen Befehl im Container ausführen, z.B. `./dev.sh opencode` |
| `./dev.sh build` | Image neu bauen (z.B. nach einer Aktualisierung der Vorlage) |

Im Container enthalten: GCC 14, Clang 19, CMake, GoogleTest, gdb, gcovr, clang-format,
clang-tidy, OpenCode.

## 3. Bauen und testen

Im Container:

```
cmake -B build && cmake --build build && ctest --test-dir build
```

Genau mit diesen Befehlen wird euer Projekt nach dem Code-Freeze gebaut und getestet.

- Programmlogik gehört in die Bibliothek `projekt_lib` (`src/`), damit Programm und Tests
  denselben Code nutzen. `src/main.cpp` ist nur der Einstiegspunkt.
- Tests liegen in `tests/` und nutzen GoogleTest. `tests/test_statistik.cpp` zeigt den
  Aufbau.
- Neue `.cpp`-Dateien in `CMakeLists.txt` eintragen.
- Die Beispielklasse `Statistik` dürft ihr löschen, sobald es eigenen Code und eigene
  Tests gibt.

Mit Sanitizern: `cmake -B build-san -DENABLE_SANITIZERS=ON && cmake --build build-san && ctest --test-dir build-san`

## 4. Pre-Commit-Hook

`./dev.sh` richtet Git so ein, dass vor jedem Commit `.githooks/pre-commit` läuft: Das
Projekt wird gebaut und getestet. Schlägt etwas fehl, wird nicht committet. Das gilt auch,
wenn ihr außerhalb des Containers committet; der Hook startet dann selbst den Container.

Git braucht einmalig euren Namen, im Container:

```
git config --global user.name "Vorname Nachname"
git config --global user.email "s0123456@htw-berlin.de"
```

Pushen am besten außerhalb des Containers: Dort liegen eure SSH-Schlüssel bzw.
Zugangsdaten für GitLab, der Container sieht sie nicht.

## 5. OpenCode und Sprachmodell

`opencode` im Projektordner starten. OpenCode liest zu Beginn jeder Sitzung `AGENTS.md`.

Welches Modell ihr nutzt, ist euch überlassen (siehe Moodle). Eine Anleitung zur
Einrichtung folgt. Zwei Hinweise vorab:

- **API-Schlüssel** nicht ins Projekt legen. `./dev.sh` übernimmt Umgebungsvariablen aus
  `~/.config/faup/env` auf eurem Rechner (eine Zeile pro Variable, `NAME=wert`).
- **Lokales Modell auf eurem Rechner** (z.B. LM Studio): Im Container ist euer Rechner
  unter `host.docker.internal` erreichbar, z.B. `http://host.docker.internal:1234/v1`.

## 6. Dateien

| Datei | Zweck |
|---|---|
| `PROJEKT.md` | Projektziel, Eigenschaften, Definitions of Done, Schwierigkeit |
| `AGENTS.md` | Projektkontext und Regeln für den Agenten; mit dem Projekt aktuell halten |
| `CMakeLists.txt` | Build und Tests |
| `src/`, `tests/` | Code und Tests |
| `dev.sh`, `container/` | Dev-Container |
| `.githooks/pre-commit` | Bauen und testen vor jedem Commit |

## Probleme

- **Podman bricht ab mit `crun: open ... merged: Permission denied`:**
  `FAUP_RUN_ARGS="--group-add keep-groups" ./dev.sh`
- Weitere Fragen in den Präsenzübungen.
