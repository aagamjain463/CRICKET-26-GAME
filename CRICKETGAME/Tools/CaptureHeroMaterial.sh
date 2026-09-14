#!/bin/bash
# Hero skin/eye A/B capture: <stage=before|after> <view=daylight|side|closeup>
# Uses the existing C26ReviewCapture machinery on a transient inspection copy
# of the review level. Never touches match maps, cameras or game assets.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${1:-before}"
VIEW="${2:-daylight}"
EDITOR="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
pgrep -f UnrealEditor >/dev/null && { echo "Unreal already running; refusing concurrent run"; exit 1; }
export C26_MATERIAL_VIEW="$VIEW"
"$EDITOR" "$PROJ/CRICKETGAME.uproject" \
  -ExecutePythonScript="$PROJ/Tools/PrepareHeroMaterialReview.py" \
  -unattended -nosplash -nosound -abslog="$PROJ/Artifacts/HeroSkinEyes/prep_${VIEW}.log" >/dev/null 2>&1
"$EDITOR" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Characters/Debug/L_C26_CharacterReview \
  -game -C26ReviewCapture -C26ReviewAnimation=/Game/Cricket26/Characters/Animations/Review/Review_A_Idle \
  -C26ReviewLOD=-1 -unattended -windowed -ResX=1000 -ResY=900 -nosplash \
  -abslog="$PROJ/Artifacts/HeroSkinEyes/cap_${STAGE}_${VIEW}.log" >/dev/null 2>&1 || true
pkill -f "UnrealEditor.app" || true
DEST="$PROJ/Artifacts/HeroSkinEyes/$STAGE/$VIEW"
mkdir -p "$DEST"
mv -f "$PROJ/Artifacts/CharacterAudit/RunReview/Review_A_Idle_lod-1_"*.png "$DEST/" 2>/dev/null || true
ls "$DEST"
