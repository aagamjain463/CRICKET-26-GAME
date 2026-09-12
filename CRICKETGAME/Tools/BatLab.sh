#!/bin/bash
# Scripted batting playtest: 27 real deliveries driven through the actual
# AC26PlayerController pointer routing (press -> drag -> release), covering the
# direction, magnitude, timing, handedness, multi-touch and marker-lifetime
# cases. Usage: bash Tools/BatLab.sh [label]
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-batlab}"
SHOTS=""
[ "${2:-}" = "shots" ] && SHOTS="-C26BatLabShots"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple label"; exit 2; }
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver \
  -game -C26BatLab "${SHOTS:-}" -windowed -ResX=1600 -ResY=900 -nosplash -nosound \
  -abslog="$PROJ/Artifacts/$LABEL.log" >/dev/null 2>&1 || true
grep -aE 'C26_LAB_(RELEASE|PASS|FAIL)' "$PROJ/Artifacts/$LABEL.log" || true
echo "---- failures ----"
grep -a 'C26_LAB_CHECK.*FAIL' "$PROJ/Artifacts/$LABEL.log" || echo "none"
grep -aq 'C26_LAB_PASS' "$PROJ/Artifacts/$LABEL.log"
