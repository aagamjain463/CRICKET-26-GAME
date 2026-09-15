#!/bin/bash
# Round 7 reaction lab: a real AI delivery, then committed outcomes through the real Resolve().
# Usage: bash Tools/ReactLab.sh [label]
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-react_lab}"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple capture label"; exit 2; }
DEST="$PROJ/Artifacts/Captures/$LABEL"
mkdir -p "$DEST"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver -game -C26ReactLab -C26GateDir="$DEST" \
    -windowed -ResX=1600 -ResY=900 -nosplash -nosound -abslog="$PROJ/Artifacts/$LABEL.log" >/dev/null 2>&1 || true
grep -aE 'C26_REACT_(CHECK|NOTE)|C26_REACTLAB_|C26_REACTION_STAGE|C26_CHARACTER_MISSING_CLIP' "$PROJ/Artifacts/$LABEL.log"
grep -aq 'C26_REACTLAB_PASS failures=0' "$PROJ/Artifacts/$LABEL.log"
