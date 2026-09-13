#!/bin/bash
# Capture one unapproved candidate in the development review map. Never migrates match actors.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LOD="${2:-0}"
[[ "$LOD" =~ ^[0-3]$ ]] || exit 2
case "${1:-Run}" in
Run) CLIP=/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run ;;
Idle) CLIP=/Game/Cricket26/Characters/Animations/Review/Review_A_Idle ;;
StraightDrive) CLIP=/Game/Cricket26/Characters/Animations/Review/Review_A_C26_BattingDrive ;;
FastBowl) CLIP=/Game/Cricket26/Characters/Animations/Review/Review_A_C26_BowlingPace ;;
*) echo 'Use Run, Idle, StraightDrive or FastBowl, followed by LOD 0..3'; exit 2 ;;
esac
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
 "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Characters/Debug/L_C26_CharacterReview \
 -game -C26ReviewCapture -C26ReviewAnimation="$CLIP" -C26ReviewLOD="$LOD" \
 -windowed -ResX=1000 -ResY=900 -nosplash -abslog="$PROJ/Artifacts/CharacterAudit/review_${1:-Run}_lod${LOD}.log" >/dev/null 2>&1
