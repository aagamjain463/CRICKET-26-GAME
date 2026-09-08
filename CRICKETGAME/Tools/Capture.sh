#!/bin/bash
# Deterministic visual-acceptance capture. Writes the presentation beats to Artifacts/Shots.
# The Binaries/Mac/UnrealEditor wrapper launches the .app and returns immediately, so this
# script polls for the real process to exit before reporting.
# Usage: Tools/Capture.sh [label]
set -u
PROJ="/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor"
LABEL="${1:-shots}"
pkill -9 -f "C26Shots" 2>/dev/null
sleep 2
rm -f "$PROJ/Artifacts/Shots"/*.png
mkdir -p "$PROJ/Artifacts/Shots"
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver -game -C26Shots \
  -windowed -ResX=1600 -ResY=900 -nosplash -nosound \
  -abslog="$PROJ/Artifacts/$LABEL.log" >/dev/null 2>&1
sleep 5
DEADLINE=$((SECONDS+900))
while pgrep -f "C26Shots" >/dev/null && [ $SECONDS -lt $DEADLINE ]; do sleep 3; done
pkill -9 -f "C26Shots" 2>/dev/null
echo "captured: $(ls "$PROJ/Artifacts/Shots" | wc -l | tr -d ' ')"
grep -E "C26_SHOT_SKIPPED|C26_SHOTS_COMPLETE" "$PROJ/Artifacts/$LABEL.log" | tail -6
