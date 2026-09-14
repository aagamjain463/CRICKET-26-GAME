#!/bin/bash
# Round 3 batting review: the striker's pre-delivery movement and the straight drive.
#
#   bash Tools/BatterReview.sh [clip]
#
# Three parts, because the sequence has three ways to be wrong.
#
#  1. THE NUMBERS. Tools/correct_authored_anim.py re-verifies the corrected clip that the
#     engine imports. Its Round 3 gates are the ones that cannot be seen in a still:
#     no foot skate on either foot across the whole clip, the stride and the recovery are
#     STEPS (the foot is off the ground while it relocates), the recovery lands back on the
#     stance frame, the hips lead the chest through the downswing, the head holds its height
#     through contact, and the weight actually moves onto the front foot.
#
#  2. THE PICTURES. Tools/AnimContactSheet.py draws the skeleton straight out of that same
#     corrected FBX -- not a re-simulation -- from three camera positions and at two speeds,
#     with every authored key frame named, so Idle -> Trigger -> Straight Drive -> Recovery
#     can be read off one image. Writes to Artifacts/AnimSheets/.
#
#  3. THE IMPORT. Both of the above read the FBX. Neither can tell you that the asset the
#     game plays is the same thing. Tools/VerifyImportedClip.py loads the imported
#     UAnimSequence and re-asserts the geometry in engine centimetres. Skipped, not failed,
#     when UnrealEditor-Cmd is not installed.
#
# Slow motion is `--step 1` (every frame); the normal read is `--step 2`. Front / three / side
# are named for where the CAMERA is, relative to a right-handed batter: front is from the
# bowler's end, side is square of the wicket (the classic technique camera), three is between.
#
# The in-engine half of the same test is Tools/Capture.sh: the beat 14_runup_batting is the
# unloaded stance and 14b_trigger_batting is the same striker mid-trigger, so those two
# screenshots are the before/after of the pre-delivery movement as the game renders it.
set -euo pipefail
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
CLIP="${1:-A_C26_BattingDrive}"
OUT="$PROJ/Artifacts/AnimSheets"

# The contact sheet needs Pillow; the numeric gate does not. Prefer a python3 that has it so
# both halves run from one command, and say so plainly if neither does.
PY=""
for CAND in python3 /Library/Frameworks/Python.framework/Versions/3.14/bin/python3 \
            /usr/local/bin/python3 /opt/homebrew/bin/python3; do
    if command -v "$CAND" >/dev/null 2>&1 && "$CAND" -c 'import PIL' >/dev/null 2>&1; then
        PY="$CAND"; break
    fi
done
if [ -z "$PY" ]; then
    echo "No python3 with Pillow found -- cannot draw the contact sheets." >&2
    echo "Install it (python3 -m pip install pillow) or pass one via PATH." >&2
    exit 2
fi

echo "== 1. corrected clip, Round 3 gates =="
"$PY" "$PROJ/Tools/correct_authored_anim.py" "Solved/$CLIP.fbx" --verify-only

echo
echo "== 2. contact sheets -> $OUT =="
"$PY" "$PROJ/Tools/AnimContactSheet.py" "$CLIP" --view all --step all --out "$OUT"

# 3. THE IMPORT. Stages 1 and 2 both read the corrected FBX; neither can tell you
#    whether the asset the game actually plays is the same thing. This one loads
#    the imported UAnimSequence out of the content browser and re-asserts the
#    geometry in engine centimetres. Skipped (not failed) if the editor is absent,
#    so the offline half still works on a machine without UE.
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
if [ -x "$UE" ]; then
    echo
    echo "== 3. imported asset, in engine =="
    "$UE" "$PROJ/CRICKETGAME.uproject" \
        -run=pythonscript -script="$PROJ/Tools/VerifyImportedClip.py" \
        -unattended -nosplash -nullrhi -stdout >/dev/null 2>&1 || true
    cat "$PROJ/Artifacts/VerifyImportedClip.log" 2>/dev/null || echo "  no log written"
else
    echo
    echo "== 3. imported asset, in engine: SKIPPED (no UnrealEditor-Cmd at $UE) =="
fi

echo
echo "Look at: $OUT/${CLIP}_side_step1.png  (side, every frame -- the technique read)"
echo "         $OUT/${CLIP}_front_step2.png (front, normal speed -- the whole sequence at a glance)"
echo "In-engine before/after: Tools/Capture.sh then compare Artifacts/Captures/<label>/14_runup_batting.png"
echo "                        against 14b_trigger_batting.png"
