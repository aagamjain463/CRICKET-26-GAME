# Character audit — 12 September 2026

Checkpoint: `17f34c8`, branch `checkpoint/pre-premium-character-rebuild-20260912`.
Implementation branch: `feature/premium-character-rebuild`.
Written before replacement assets are modified. Machine evidence is in
`Artifacts/CharacterAudit/source-geometry.json` and the baseline match captures in
`Artifacts/Captures/character_before/`.

## What actually runs

`AC26MatchGameMode::BuildMatchActors` spawns fourteen `AC26Athlete` actors:
bowler, keeper, nine other fielders, two batters, one umpire. The root is a scene
component. There is no player collision capsule, CharacterMovement or root-motion
movement in this match architecture. Custom simulation moves actors with
SetActorLocation/AddActorWorldOffset; this need not change to replace presentation.
`UC26PoseMesh` is a UPoseableMeshComponent. No AnimInstance, AnimBP, montage,
leader-pose body, Copy Pose, IK Rig or retargeter drives it. Equipment is repositioned
in mesh space every pose update. The separate MetaHuman actor is never spawned here.

## Missing lower body: separate asset defects, stale claims

The fresh Blender audit contradicts old C++ comments claiming every hero FBX is a
half body: all ten current exports contain lower-leg and foot-weighted vertices.
Do not repeat that assertion as a verified diagnosis. Nine scans have only sixteen
weighted bones, with no forearm weights, despite a 67-bone T-pose skeleton. Their
geometry is in an arms-down stance and does not fit the rest skeleton. The current
runtime rejects those scans by bind-span ratio. The default match uses
`SK_Cricketer_Match`, not the scans and not the MetaHuman.

The original kit's `Body.001` has geometry only from z=1.77 to 3.74 metres in the
oversized source. Its clothing supplied the omitted anatomy. `Bottoms.001` stops
at z=.89m; the newer `C26_Trousers` reaches .31m but the match FBX exports NO shoes
and NO lower-leg/foot skin. Separately placed static shoes cover the feet. The
baseline match capture visibly shows gaps between trouser cuffs and detached shoes.
The runtime applies .48 scale inside its pose transforms to reconcile a 374cm
import with the ground. This is not a complete anatomical master body.

The current baseline capture DOES show both trouser legs on the inspected fielder.
The reported floating-torso symptom was not reproduced in that capture; its exact
historical runtime cause cannot be certified from these source files. All LODs
need geometry checks, not inference from bounds or bone names. No modular legs,
leader-pose relationship or camera change can repair absent anatomy in a mesh.

## Wrong equipment

`ApplyVisualRole` says baked scans include pads/helmet/bat; changing component
visibility cannot remove baked equipment. The old static-hero path disables all
separate gear. In the currently selected kit path `ApplyDetail` gates pads to
Batter OR Keeper, but BOTH roles share the same batting pads, gloves and helmet.
There are no keeper-specific pads/gloves. Current baseline fielders inspected so
far do not have separate batting pads; historical pad leakage is not reproduced
and must not be blamed on a nonexistent current Fielder=true switch.

## Gliding / broken limbs

`Animate` computes procedural targets, samples `A_Run` in ApplyRecordedMotion,
then unconditionally solves BOTH legs and BOTH arms again. The sampled run's
limbs are overwritten. `Action==Running`, not measured displacement, gates gait
advancement. Direct-transform movements such as receiving a return can move an
actor without setting Running or MoveSpeed. The static HeroMesh branch returns
before any skeleton update. Distant solve skipping adds another timing path.
The arm-reach limiter multiplies measured ArmSpan by .492 AGAIN after the
reference was already scaled .48. Batting finger roots are scaled to .24 to hide
them inside gloves. These cause malformed reach/hands rather than premium anatomy.

## Awkward batting / bowling

Both imported action clips are loaded and their driven bones measured, but their
application is explicitly disabled near the end of Animate. Actual cricket strokes
come from C26Motion target equations plus manual twists/IK, not authored full-body
animation. A single drive clip cannot cover the requested shot library. Comments
record prior incorrect authored release poses and unsafe whole-pose overwrites;
these clips require visual review, not automatic promotion. No off-spin/leg-spin
AnimSequence exists. Run-up translates 17.05m over one fixed duration for every
style, with only .31s reserved for the delivery gather. Release/contact are exact
simulation-clock evaluations, so replacing them with unconstrained montage clocks
would break the existing gameplay contract.

## Baseline quality gate

The project compiles. The real GoldenGate match captured 33 frames and failed six
camera-composition assertions (repeated across stages); release, contact and
pickup/throw checks passed. A passing simulation check is NOT a character-quality
pass. Existing reports claiming completion are stale.
