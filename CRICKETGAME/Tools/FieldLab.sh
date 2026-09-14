#!/bin/bash
# Repeatable check for the premium fielding sequence:
#   approach -> decelerate -> ground pickup -> rise/load -> overarm throw -> follow-through -> recovery
#
# Runs the Cricket26.Fielding.PremiumSequence automation test, which samples the pickup and throw
# solvers across the whole 1.03 s sequence and asserts the biomechanical landmarks (whole-body
# lowering, hand above turf, side-on load, high release, step-through recovery), exact continuity
# at the pickup -> throw boundary, and frame-to-frame continuity at 120 Hz so a mid-phase pop is
# caught as a number rather than by eye.
#
# It also covers the braking stride that carries a chase into the gather (C26Motion::SolveApproachBrake).
# That layer is driven by the match with Dt == 0 -- ActionTime is assigned and Animate(0) is called --
# so the test asserts it advances with no per-frame integration, starts exactly on the stride the
# chase ended on, decelerates step over step, never sinks a foot through the turf, and is a complete
# no-op for a fielder who walked in to the ball. C26_FIELD_LAYERED is the same 120 Hz sweep run
# through that layer stack, which is the only way a blend bug would show up as a number.
#
# Usage: bash Tools/FieldLab.sh [label]
#   Then, for a look at it in the real match instead of in numbers:
#     Tools/Capture.sh fielding      # walks the scripted beats, writes Artifacts/Captures/fielding
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
LABEL="${1:-fieldlab}"
[[ "$LABEL" =~ ^[A-Za-z0-9_-]+$ ]] || { echo "Use a simple label"; exit 2; }
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LOG="$PROJ/Artifacts/$LABEL.log"
mkdir -p "$PROJ/Artifacts"
"$UE" "$PROJ/CRICKETGAME.uproject" \
  -ExecCmds="Automation RunTests Cricket26.Fielding.PremiumSequence" \
  -TestExit="Automation Test Queue Empty" \
  -unattended -nullrhi -nosplash -nosound -NoLogTimes \
  -abslog="$LOG" >/dev/null 2>&1 || true
grep -aE 'C26_FIELD_CONTINUITY|C26_FIELD_LAYERED' "$LOG" || true
echo "---- failures ----"
grep -aE 'Test Failed|LogAutomationController.*Error' "$LOG" || echo "none"
grep -aq 'Result={Success} Name={PremiumSequence}' "$LOG"
