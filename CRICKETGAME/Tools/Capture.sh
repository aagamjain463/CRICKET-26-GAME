#!/bin/bash
# Deterministic visual-acceptance capture. Writes the presentation beats to Artifacts/Shots.
# The Binaries/Mac/UnrealEditor wrapper launches the .app and returns immediately, so this
# script polls for the real process to exit before reporting.
# Usage: Tools/Capture.sh [label]
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LABEL="${1:-shots}"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple capture label"; exit 2; }
DEST="$PROJ/Artifacts/Captures/$LABEL"
mkdir -p "$DEST"
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver -game -C26Shots \
  -C26ShotsDir="$DEST" \
  -windowed -ResX=1600 -ResY=900 -nosplash -nosound \
  -abslog="$PROJ/Artifacts/$LABEL.log" >/dev/null 2>&1
echo "Captures: $DEST"
grep -E 'C26_SHOT_SKIPPED|C26_SHOTS_COMPLETE' "$PROJ/Artifacts/$LABEL.log" || true
! grep -q 'C26_SHOT_SKIPPED' "$PROJ/Artifacts/$LABEL.log"
