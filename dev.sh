#!/usr/bin/env bash
# dev.sh – startet den Dev-Container für dieses Projekt.
#
#   ./dev.sh                 Shell im Container (ein zweiter Aufruf öffnet eine weitere Shell
#                            im laufenden Container)
#   ./dev.sh <befehl> ...    einzelnen Befehl im Container ausführen, z.B. ./dev.sh opencode
#   ./dev.sh build           Image (neu) bauen
#
# Eingebunden wird nur:
#   /work       dieses Projekt
#   /home/dev   ein Volume (kein Host-Verzeichnis) für Einstellungen von OpenCode, Git und
#               Shell; es überlebt Neustarts des Containers.
# API-Schlüssel kommen als Umgebungsvariablen aus einer Datei außerhalb des Projekts
# (Standard: ~/.config/faup/env, Format: NAME=wert je Zeile).
#
# Voraussetzung: Podman (unter Linux aus den Paketquellen, unter macOS Podman Desktop).
#
# Einstellbar über Umgebungsvariablen:
#   FAUP_ENV_FILE   Datei mit API-Schlüsseln
#   FAUP_RUN_ARGS   zusätzliche Optionen für "run", durch Leerzeichen getrennt; z.B.
#                   "--group-add keep-groups", falls Podman mit
#                   "crun: open ... merged: Permission denied" abbricht
set -euo pipefail

image=faup-dev
home_volume=faup-home
env_file=${FAUP_ENV_FILE:-$HOME/.config/faup/env}
root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
name="faup-$(basename "$root" | tr -c 'a-zA-Z0-9_.\n-' '_')"

die() { echo "dev.sh: $*" >&2; exit 1; }

command -v podman >/dev/null 2>&1 || die "podman nicht gefunden (Installation: siehe README.md)"

build() { podman build -t "$image" "$root/container"; }

if [[ "${1:-}" == build ]]; then
    build
    exit 0
fi

# Git soll die Hooks aus dem Projekt verwenden (.githooks/pre-commit)
if command -v git >/dev/null 2>&1 && [[ -e "$root/.git" ]]; then
    git -C "$root" config core.hooksPath .githooks
fi

tty=()
[[ -t 0 && -t 1 ]] && tty=(-t)

# Läuft der Container schon, dort eine weitere Shell bzw. den Befehl starten
if [[ "$(podman container inspect -f '{{.State.Running}}' "$name" 2>/dev/null)" == true ]]; then
    [[ $# -gt 0 ]] || set -- bash
    exec podman exec -i ${tty[@]+"${tty[@]}"} -w /work "$name" "$@"
fi

if ! podman image inspect "$image" >/dev/null 2>&1; then
    echo "dev.sh: Image $image fehlt, wird gebaut (einmalig, dauert einige Minuten)" >&2
    build
fi

args=(run --rm -i ${tty[@]+"${tty[@]}"} --init
      --security-opt no-new-privileges
      -w /work)

# Nur die interaktive Shell bekommt einen Namen; einzelne Befehle (z.B. aus dem
# Pre-Commit-Hook) laufen in einem eigenen, namenlosen Container.
[[ $# -gt 0 ]] || args+=(--name "$name" --hostname faup-dev)

# Benutzer dev (UID 1000) im Container entspricht dem eigenen Benutzer auf dem Host
args+=(--userns=keep-id:uid=1000,gid=1000
       -v "$root:/work:Z" -v "$home_volume:/home/dev:Z")

# Der Host ist im Container als host.containers.internal erreichbar, auch für Dienste,
# die nur auf localhost lauschen (z.B. LM Studio). Unter Linux je nach Netzwerk-Backend:
# neueres pasta (Podman 5 und später) oder slirp4netns (z.B. Podman 4.9 unter Ubuntu
# 24.04). Auf macOS läuft Podman in einer VM, dort erledigt das podman machine.
if [[ "$(uname -s)" == Linux ]]; then
    if pasta --help 2>&1 | grep -q -- --map-host-loopback; then
        args+=(--network pasta:--map-host-loopback,169.254.1.2
               --add-host=host.containers.internal:169.254.1.2)
    elif command -v slirp4netns >/dev/null 2>&1; then
        args+=(--network slirp4netns:allow_host_loopback=true
               --add-host=host.containers.internal:10.0.2.2)
    fi
fi

if [[ -f "$env_file" ]]; then
    case "$(realpath "$env_file")" in
        "$root"/*) die "$env_file liegt im Projekt; Schlüssel gehören nicht ins Repository" ;;
    esac
    args+=(--env-file "$env_file")
fi

read -r -a extra <<< "${FAUP_RUN_ARGS:-}"
args+=(${extra[@]+"${extra[@]}"})

exec podman "${args[@]}" "$image" "$@"
