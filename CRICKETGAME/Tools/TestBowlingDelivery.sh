#!/bin/bash
# CRICKET 26 - Fast-Bowling Sequence Evaluation (Round 3 Task 2)
#
# Runs the golden gate, which drives ONE complete delivery through the real match code --
# idle at the mark -> run-up -> gather -> bound -> back-foot contact -> front-foot plant ->
# arm circle -> release -> follow-through -> recovery -- and captures the sequence from a
# camera parked on the bowler.
#
# Views:
#   side      square of the crease, level with the bowler: the arm circle and the delivery stride
#   front_34  down the pitch toward him: the approach, the gather and the front-foot brace
#   rear_34   over his trailing shoulder: the follow-through and the recovery strides
#
# Usage:
#   Tools/TestBowlingDelivery.sh [side|front_34|rear_34|all]
#
# Output: Artifacts/Captures/Bowling_<view>/*.png  and  Artifacts/Bowling_<view>.log
# The frames that matter, in sequence order:
#   0_01_ready  0_02_runup  0_02b_gather  0_02c_plant
#   1_03_release  1_03b_followthrough  1_03c_recovery

set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"

run_view() {
    local VIEW="$1"
    local DEST="$PROJ/Artifacts/Captures/Bowling_${VIEW}"
    local LOG="$PROJ/Artifacts/Bowling_${VIEW}.log"
    rm -rf "$DEST"; mkdir -p "$DEST" "$PROJ/Artifacts"
    echo "============================================================"
    echo "Fast-bowling sequence - ${VIEW} view"
    echo "============================================================"

    local OK=1
    "$UE" "$PROJ/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver \
        -game -C26GoldenGate -C26GateDir="$DEST" -C26WorldView="${VIEW}" \
        -windowed -ResX=1600 -ResY=900 -nosplash -nosound \
        -abslog="${LOG}" >/dev/null 2>&1 || OK=0

    # -a: the editor log is not guaranteed to be clean UTF-8.
    echo "-- release check (the ball must still be in the hand on the rendered release frame) --"
    grep -aE 'C26_GATE_RELEASE' "${LOG}" || echo "  (no release line -- the run did not reach release)"
    echo "-- sequence frames captured --"
    grep -aE 'C26_GATE_FRAME (0_0[12]|1_03)' "${LOG}" || echo "  (none)"
    echo "-- gameplay gate --"
    grep -aE 'C26_GATE_(PASS|CHECK .* FAIL|TIMEOUT)' "${LOG}" || true
    echo "-- images in ${DEST} --"
    ls -1 "$DEST" 2>/dev/null | grep -E '_(01_ready|02_runup|02b_gather|02c_plant|03_release|03b_followthrough|03c_recovery)\.png' || echo "  (none)"
    echo ""
    [ "$OK" = 1 ] || { echo "RUN FAILED for ${VIEW} (see ${LOG})"; return 1; }
}

case "${1:-all}" in
    side|front_34|rear_34) run_view "$1" ;;
    all) run_view side; run_view front_34; run_view rear_34 ;;
    *) echo "Unknown view: $1. Use side, front_34, rear_34, or all."; exit 2 ;;
esac

echo "Done. Compare 02b_gather -> 02c_plant -> 03_release -> 03b_followthrough across the views."
