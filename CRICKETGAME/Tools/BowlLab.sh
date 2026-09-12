#!/bin/bash
# Scripted bowling playtest: 39 real deliveries driven through the actual
# AC26PlayerController pointer routing (press -> drag -> release) covering the
# delivery library, the exact pitch target, the movement dial and its direction,
# the pace slider, the release bands including no-ball, handedness mirroring,
# the crease, and deliveries the player assembles by hand.
#
# Usage: bash Tools/BowlLab.sh [label] [shots]
#
#   -C26BowlLabFPS=60 fixes the simulation cadence so a release lands on the
#   same meter value on every machine; the harness is a correctness check, not a
#   frame-timing benchmark.
#   -C26ReverseAlways makes reverse swing reachable from ball one, which a
#   six-ball Super Over could otherwise never reach.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-bowllab}"
SHOTS=""
[ "${2:-}" = "shots" ] && SHOTS="-C26BowlLabShots"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple label"; exit 2; }
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver \
  -game -C26BowlLab -C26BowlLabFPS=60 -C26ReverseAlways "${SHOTS:-}" \
  -windowed -ResX=1600 -ResY=900 -nosplash -nosound \
  -abslog="$PROJ/Artifacts/$LABEL.log" >/dev/null 2>&1 || true
grep -aE 'C26_BOWL_(RELEASE|PASS|FAIL|RESULT)' "$PROJ/Artifacts/$LABEL.log" || true
echo "---- failures ----"
grep -a 'C26_BOWL_CHECK.*FAIL' "$PROJ/Artifacts/$LABEL.log" || echo "none"
grep -aq 'C26_BOWL_PASS' "$PROJ/Artifacts/$LABEL.log"
