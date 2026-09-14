# Premium right-handed batting library (Round 4)

## Strokes
Source: `ArtSource/Blender/Premium/c26_actions.py` (`SHOTS`). Each stroke is authored as technique in
the batter's frame: foot targets, where the middle of the bat meets the ball (`_bat_at`), bat shaft
and face. Every stroke has the same phase clock (stance, trigger, backlift, stride/load, plant,
downswing, contact, extension, finish, hold, recovery, stance) but its own footwork, contact geometry
and finish.

| Clip | Family | Gameplay labels (`C26Controls::ShotFamily`) |
|---|---|---|
| STRAIGHTDRIVE | front foot | STRAIGHT DRIVE, DUG-OUT DRIVE |
| COVERDRIVE | front foot | COVER DRIVE, EXTRA-COVER DRIVE |
| ONDRIVE | front foot | ON DRIVE |
| FRONTFOOTDEFENCE | front foot | DEFENSIVE PUSH, YORKER BLOCK, edge fallback |
| LOFTEDDRIVE | front foot | LOFTED COVER DRIVE |
| BACKFOOTDEFENCE | back foot | BACK-FOOT DEFENCE |
| BACKFOOTPUNCH | back foot | BACK-FOOT PUNCH |
| SQUARECUT | back foot | SQUARE CUT, LATE CUT, UPPER CUT |
| PULL | back foot | PULL |
| HOOK | back foot | HOOK |
| SWEEP | spin | SWEEP (new label) |
| SLOGSWEEP | spin/aggressive | SLOG SWEEP (new label) |
| LOFTEDSTRAIGHT | aggressive | LOFTED STRAIGHT DRIVE, LEG-SIDE PICKUP |
| LEGGLANCE | wristy | LEG GLANCE, FLICK |

SWEEP and SLOG SWEEP are chosen when the player commits a press well past the ideal stride
(`Stride > IdealStrideForLength + 0.1`) to a leg-side ball below the hip. Auto footwork never sweeps.

## Right-handed correctness: how it is enforced
1. **Authoring.** The LEFT hand target is solved from the bat (`_bat_at`/`_bat_held`), and the RIGHT
   hand is derived 8.5cm down the same handle. `c26_rig.apply` orients hand_l so that
   `hand_l @ BAT_OFFSET_L` is the authored bat. Left-handed clips are the sagittal mirror.
2. **Dense bake.** `c26_rig.spec_at` interpolates technique and re-solves every frame. Blending bone
   rotations used to pull the bottom hand off the handle and flip the bat up to 130 deg/frame
   between keys.
3. **Blender gate.** `batting_lab.py` checks every frame of every stroke:
   - left shoulder and left foot lead, the left wrist is higher on the handle
   - both palms stay on the handle and no wrist is dislocated
   - the bat clears the torso capsule, the head (13cm) and the thighs
   - no knee or foot goes through the floor, no planted foot skates
   - no pelvis, chest or bat snap in one frame
   - the face at contact points along the stroke line, and each stroke differs from every other
   The gate exits non-zero on any failure.
4. **Engine gate.** `-C26ShotReview` in `L_C26_CharacterReview` plays every `_R` clip on the real
   batter profile (body, skeleton, sockets, fitted equipment). It repeats the top-hand, on-handle,
   torso and head checks against the rendered bat and captures side and bowler-3/4 frames.
5. **Socket fit.** `Tools/FitBatterSockets.py` solves the equipment offsets in UE bone frames from
   the same stance measured in Blender and in the engine. The Blender-to-UE map is a Y reflection
   with 0.00cm residual. The previous offsets were Blender bone-space numbers: in game the bat
   stuck out sideways from the hands and the pads floated off the shins.

## Pipeline bugs fixed on the way
- `clear()` never reset bone scale. Scale leaked from every `pb.matrix` write, so limb lengths
  drifted over a long authoring session.
- Poses are now solved with no action bound, and every translated bone is keyed. Before, the
  previous clip leaked into the next one.
- The `batting_lab` head check measured a degenerate segment. Engine measurement exposed it.

## Rebuild / verify
```bash
B=/Applications/Blender.app/Contents/MacOS/Blender
$B --background --python ArtSource/Blender/Premium/batting_lab.py -- --render      # C26_BATTING_LAB PASS
$B --background --python ArtSource/Blender/Premium/author_cricket_actions.py -- --only STRAIGHTDRIVE_R,...
# editor: Tools/ImportCricketActions.py, Tools/BuildBatterReview.py (keeps fitted offsets)
$B --background --python ArtSource/Blender/Premium/fit_batter_offsets.py          # stance frames
UnrealEditor CRICKETGAME.uproject /Game/Cricket26/Characters/Debug/L_C26_CharacterReview -game -C26ShotReview -C26StanceOnly
UnrealEditor CRICKETGAME.uproject -ExecutePythonScript=Tools/FitBatterSockets.py
UnrealEditor CRICKETGAME.uproject /Game/Cricket26/Characters/Debug/L_C26_CharacterReview -game -C26ShotReview
```

## Validation (2026-09-14)
- Blender lab: 14/14 PASS. No pair of strokes is too similar; the closest pair differs by 11cm mean.
- Engine shot review: 14/14 PASS (top hand, on handle, torso, head), 140 captures in
  `Artifacts/CharacterAudit/ShotReview`.
- BatLab with the premium batter slice: `C26_LAB_PASS deliveries=27 failures=0`. Gameplay labels
  resolved to COVERDRIVE/SQUARECUT/LEGGLANCE/FRONTFOOTDEFENCE/PULL/STRAIGHTDRIVE clips with no
  missing-clip warnings.
- Automation: every Cricket26 test passes except the pre-existing legacy
  `Cricket26.Anim.AuthoredClips` (old `A_C26_BattingDrive` asset).
- GoldenGate with the batter slice: only the 2 pre-existing `ball intersects rendered blade
  triangles` failures remain. The contact gap improved from 60.4cm to 9.9cm (stage 0).

## Limitations
- The review body is still unapproved (`ApprovedForMatch=false`). The premium batter runs with
  `-C26CharacterSlice`; the default match keeps the old batter.
- No runtime IK or foot locking: contact is matched by clip timing, not warped to the ball line.
  Gameplay compresses the pre-contact part of a clip into `BatContactPoseTime` (0.24s), so the
  backlift is fast in play.
- Left-handed equipment reuses the right-handed offsets. LH strokes are mirrored and not
  engine-verified.
- Stance to stroke uses the existing clip blend, with no dedicated transition clips per shot.
- The torso and legs are kinematically authored, not motion-captured. Drives lean further forward
  than an elite batter would.
