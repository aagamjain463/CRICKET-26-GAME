#!/bin/bash
# Render the actual drive -> miss -> next delivery -> restart sequence.
# Usage: bash Tools/GoldenGate.sh label [-C26GateFPS=30] [-C26GateNoScreens]
# Fixed FPS tests correctness; use an unfixed, no-screenshot run for desktop frame timing.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-golden_gate}"
if [ "$#" -gt 0 ]; then shift; fi
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple capture label"; exit 2; }
DEST="$PROJ/Artifacts/Captures/$LABEL"
mkdir -p "$DEST"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
if ! "$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver \
    -game -C26GoldenGate -C26GateDir="$DEST" -windowed -ResX=1600 -ResY=900 \
    -nosplash -abslog="$PROJ/Artifacts/$LABEL.log" "$@" >/dev/null 2>&1; then
    grep -aE 'C26_GATE_.*(FAIL|TIMEOUT)|Error:' "$PROJ/Artifacts/$LABEL.log" || true
    exit 1
fi
# grep, not rg: ripgrep is not installed on every machine this runs on, and until this changed
# the script's own reporting step exited 127 under `set -e`, so the gate could never actually
# fail a caller. -a because the editor log is not guaranteed to be clean UTF-8.
grep -aE 'C26_GATE_(PASS|CONTACT|RELEASE|FRAME_TIME)' "$PROJ/Artifacts/$LABEL.log"
grep -aq 'C26_GATE_PASS failures=0' "$PROJ/Artifacts/$LABEL.log"
