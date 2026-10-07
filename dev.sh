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
# Einstellbar über Umgebungsvariablen:
#   ENGINE          podman oder docker (Standard: podman, falls vorhanden)
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

engine=${ENGINE:-}
if [[ -z "$engine" ]]; then
    if command -v podman >/dev/null 2>&1; then engine=podman
    elif command -v docker >/dev/null 2>&1; then engine=docker
    else die "weder podman noch docker gefunden"
    fi
fi

build() { "$engine" build -t "$image" "$root/container"; }

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
if [[ "$("$engine" container inspect -f '{{.State.Running}}' "$name" 2>/dev/null)" == true ]]; then
    [[ $# -gt 0 ]] || set -- bash
    exec "$engine" exec -i ${tty[@]+"${tty[@]}"} -w /work "$name" "$@"
fi

if ! "$engine" image inspect "$image" >/dev/null 2>&1; then
    echo "dev.sh: Image $image fehlt, wird gebaut (einmalig, dauert einige Minuten)" >&2
    build
fi

args=(run --rm -i ${tty[@]+"${tty[@]}"} --init
      --security-opt no-new-privileges
      -w /work)

# Nur die interaktive Shell bekommt einen Namen; einzelne Befehle (z.B. aus dem
# Pre-Commit-Hook) laufen in einem eigenen, namenlosen Container.
[[ $# -gt 0 ]] || args+=(--name "$name" --hostname faup-dev)

if [[ "$engine" == podman ]]; then
    # Benutzer dev (UID 1000) im Container entspricht dem eigenen Benutzer auf dem Host
    args+=(--userns=keep-id:uid=1000,gid=1000
           -v "$root:/work:Z" -v "$home_volume:/home/dev:Z")
    # host.docker.internal erreicht Dienste auf dem Host, auch wenn sie nur auf
    # localhost lauschen (z.B. LM Studio). Nur unter Linux ab Podman 5 (pasta); auf
    # macOS läuft Podman in einer VM, dort erledigt das podman machine.
    if [[ "$(uname -s)" == Linux && "$(podman version -f '{{.Client.Version}}' 2>/dev/null)" =~ ^[5-9]\. ]]; then
        args+=(--network pasta:--map-host-loopback,169.254.1.2
               --add-host=host.docker.internal:169.254.1.2)
    else
        args+=(--add-host=host.docker.internal:host-gateway)
    fi
else
    # Docker Desktop (macOS, Windows) kennt host.docker.internal; unter Linux muss der
    # Dienst auf dem Host dann auf 0.0.0.0 statt localhost lauschen.
    args+=(--add-host=host.docker.internal:host-gateway)
    args+=(--user "$(id -u):$(id -g)"
           -v "$root:/work" -v "$home_volume:/home/dev")
fi

if [[ -f "$env_file" ]]; then
    case "$(realpath "$env_file")" in
        "$root"/*) die "$env_file liegt im Projekt; Schlüssel gehören nicht ins Repository" ;;
    esac
    args+=(--env-file "$env_file")
fi

read -r -a extra <<< "${FAUP_RUN_ARGS:-}"
args+=(${extra[@]+"${extra[@]}"})

exec "$engine" "${args[@]}" "$image" "$@"
