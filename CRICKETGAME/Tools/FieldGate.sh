#!/bin/bash
# Repeatable IN-MATCH check for the premium fielding sequence's ball/hand contact.
#
# FieldLab.sh proves the pose solvers are continuous, but it cannot see the ball: at runtime the
# ball's position IS the posed palms (AC26MatchGameMode sets Simulation.Ball.Position =
# ReceivingPosition() from the gather onward). Only a real match can measure whether the animation
# actually arrived at the ball or whether the ball had to be snapped into the gloves to cover a
# gap. This gate drives one through -C26GateSuite, which runs
# AC26MatchGameMode::UpdateProductionGate and reports the two contracts that couple the animation
# to the ball:
#
#   C26_SUITE_PICKUP gap_cm   palms within 14 cm of where the ball was gathered
#   C26_SUITE_KEEPER gap_cm   gloves within 12 cm of the ball
#
# A bare gap cannot say WHY it is large, so the pickup line also prints the ball, the palms and
# the fielder's root. Read it as: if X and Y agree and only Z differs, the arm ran out of reach;
# if the root is far from the ball, the athlete never got there. Those need opposite fixes.
#
# Usage: bash Tools/FieldGate.sh [label]
#   bash Tools/FieldGate.sh gate_armfix
#
# Expect OTHER suite failures too (batting blade contact, keeper takes, natural outcomes). This
# gate is scoped to the fielding contracts; use the totals to tell a regression from a baseline.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-fieldgate}"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple label"; exit 2; }
DEST="$PROJ/Artifacts/Captures/$LABEL"
LOG="$PROJ/Artifacts/$LABEL.log"
mkdir -p "$DEST"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
# -game, not -nullrhi: the gate measures the posed palms and the rendered bat, so it needs a real
# renderer. Keep the screenshots -- the pickup frame is the only view of the gather itself.
# Both flags are required: -C26GoldenGate arms the gate, -C26GateSuite routes it to the suite
# contracts once it reaches stage 3.
"$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver \
  -game -C26GoldenGate -C26GateSuite -C26GateDir="$DEST" \
  -windowed -ResX=1600 -ResY=900 -nosplash -abslog="$LOG" >/dev/null 2>&1 || true
echo "---- fielding ball/hand contact ----"
grep -aE 'C26_SUITE_(PICKUP|KEEPER)' "$LOG" | sed 's/^.*LogC26: Display: //' || echo "none"
echo "---- suite failures ----"
grep -a 'C26_SUITE_CHECK FAIL' "$LOG" | sed 's/^.*LogC26: Display: //' || echo "none"
echo "---- totals ----"
printf 'PASS: %s\n' "$(grep -ac 'C26_SUITE_CHECK PASS' "$LOG" || true)"
printf 'FAIL: %s\n' "$(grep -ac 'C26_SUITE_CHECK FAIL' "$LOG" || true)"
echo "Captures: $DEST"
