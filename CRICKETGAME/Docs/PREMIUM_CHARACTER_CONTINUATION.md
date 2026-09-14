# Premium character continuation — 2026-09-13 (feature/premium-character-continuation)

Status: OUTFIELD VERTICAL SLICE IN REAL MATCH. Not full migration. No approval flags set.

## 1. Resume point
Branch `feature/premium-character-continuation` cut from merged `main` (prior GPT-6/Astra + Codex work).
On resume the build was BROKEN: `C26CameraDirector.cpp` used undeclared `Striker` (merge fallout).
Fixed by using the existing `CameraStrikerMark` anchor (replay celebration + boundary tracking).
Build now succeeds (18.6s incremental, 12.8s after slice change).

## 2. Real defects found and fixed (not re-audited)
- **Mirrored batting poses dropped a wrist.** `c26_rig._resolve` ran a fixed left-then-right
  pass; after `mirror()` the left hand depends on the right, so the left arm never solved.
  Fixed with a two-pass resolve + hard error on unsolved wrists.
  Regression: `ArtSource/Blender/Premium/verify_hand_dependencies.py` → `C26_HAND_DEPENDENCIES_PASS 6`.
- **Bent knees tilted shoes into the ground.** Ankle orientation inherited the calf's pitch.
  Fixed: foot resets to the canonical ground frame, then applies only the authored ankle roll.
  Fielding clips (FielderReady, Walk, Start, Stop, Turns, Pickup, Throw, Catch, Celebrate,
  Disappointed) were re-authored with grounded feet.
- **Jersey neckline/shoulder holes (visible in review captures).** The bridge from the wide
  torso top edge to the neck ring was too wide, wound inward (invisible from the front),
  and torso skin was kept underneath. Fixed: tighter collar (rx 0.056/ry 0.052, +0.022 up),
  outward-facing bridge winding check, torso+clavicle+neck_02 skin occluded inside the
  garment volume. Shoulder holes are closed; collar reads as a crew neck.
- **Slice selected one fielder, so the gate ball went to an old actor.** Slice now allows
  all athletes of the requested role when no number is given, so all 9 fielders (then the
  bowler too) use the review profile and whoever fields is a new body.

## 3. What is validated in a REAL match (L_SuperOver GoldenGate)
Profile `DA_C26_FielderReview` (unapproved, `ApprovedForMatch=false`): body
`SK_C26_Athlete_Review`, canonical `SK_C26_FullBody_Candidate_Skeleton`, clips
FielderReady/BowlerReady/Walk/Run(C26_A_Run fixture)/Start/Stop/TurnLeft/TurnRight/
Pickup/Throw/Catch/Celebrate/Disappointed + FastBowl/OffSpin/LegSpin L/R with
BallRelease markers. `InspectRole(FIELDER)` and `InspectRole(BOWLER)` pass;
`InspectRole(BATTER)` and `InspectRole(KEEPER)` correctly FAIL (no faked gear).
- `character_outfield` gate: **10/10 new bodies active** (bowler + 9 fielders),
  `drive completed pickup and hand throw` PASS, `return leaves rendered throwing hand` PASS.
- Screenshots: `0_02_runup` (new bowler running in), `0_07_pickup` (new fielder chasing
  at full stride, full legs/arms, no pads), `0_08a_throw_release` (new fielder mid-throw).
- Automation: all 5 `Cricket26.Characters.*` tests pass; full suite green except the
  pre-existing legacy `Cricket26.Anim.AuthoredClips` failure on old `/Game/Cricket26/Animations`
  assets (unchanged by this slice).
- The 2 gate FAILs are the pre-existing `ball intersects rendered blade triangles`
  check on the OLD batter/bat (gap ~86–90cm), not the new outfield.

## 4. Known visual limitations (honest, from the captures)
- Jersey–trouser waistband gaps open at full stride (skin flash at the midriff).
  Fix: lengthen jersey hem below the trouser waist.
- Jersey chest has smooth-shading bands (normal bunching from the abdomen flatten).
- Bodies are bald with neutral faces: correct for distant fielders, not hero close-ups.
- No keeper pads/gloves, umpire outfit, batting pads/gloves/bat on the new skeleton yet:
  batter/keeper/umpire remain OLD visuals by design (gates not passed).
- No Control Rig/FBIK, foot lock, hand IK, motion warping, motion matching, or mobile
  device profiling. Min-spec LODs exist (4 LODs, all regions weighted) but no perf numbers.

## 5. Files created
- `ArtSource/Blender/Premium/refine_review_body.py` (garment/occlusion refinement)
- `ArtSource/Blender/Premium/verify_hand_dependencies.py` (handedness regression)
- `ArtSource/Premium/FullBody/C26_Athlete_Review.blend` + `SK_C26_Athlete_Review.fbx`
- `Tools/BuildFielderMatchReview.py` (body import, sockets, outfield review profile)
- `Content/Cricket26/Characters/Bodies/SK_C26_Athlete_Review.uasset`
- `Content/Cricket26/Characters/Data/DA_C26_FielderReview.uasset`
- `Docs/PREMIUM_CHARACTER_CONTINUATION.md` (this file)
- Captures: `Artifacts/Captures/character_outfield/`, review shots
  `Artifacts/CharacterAudit/RunReview/A_C26_FielderReady_lod0_*`

## 6. Files modified
- `Source/.../SuperOver/C26CameraDirector.cpp` (Striker → CameraStrikerMark fix)
- `Source/.../Characters/C26CharacterProfile.{h,cpp}` (InspectRole, SetEquipmentSocket)
- `Source/.../Characters/C26CharacterPresentationComponent.{h,cpp}` (role-scoped slice,
  role re-validation, teleport reset, match-review capture tick)
- `Source/.../Characters/C26CharacterReviewMode.cpp` (C26ReviewBody override)
- `ArtSource/Blender/Premium/c26_rig.py` (two-pass hands, grounded feet)
- `ArtSource/Blender/Premium/author_cricket_actions.py` (--only filter)
- `Tools/ImportCricketActions.py` (hash-keyed reimport, safe notify repair)
- `Tools/ReviewPremiumCharacter.sh` (arbitrary clip + body override)

## 7. How to test right now
```bash
cd ~/Desktop/CRICKET-26-GAME/CRICKETGAME
# close-up review (new body, chosen clip, LOD 0..3):
bash Tools/ReviewPremiumCharacter.sh FielderReady 0 /Game/Cricket26/Characters/Bodies/SK_C26_Athlete_Review
# real match, new bowler + all 9 new fielders (old batter/keeper/umpire):
bash Tools/GoldenGate.sh character_outfield -C26GateFPS=30 -unattended \
  -C26CharacterSlice -C26CharacterProfile=/Game/Cricket26/Characters/Data/DA_C26_FielderReview
# automation:
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CRICKETGAME.uproject" \
  -nullrhi -unattended -nosplash -nosound '-ExecCmds=Automation RunTests Cricket26; Quit' \
  '-TestExit=Automation Test Queue Empty' -abslog="$PWD/Artifacts/CharacterAudit/continuation-automation.log"
```
Look for: `C26_CHARACTER_ACTIVE` ×10, blue full-body bowler running in, blue fielder
chase/pickup/throw, no pads on fielders, no T-pose. The old red batter/keeper/umpire
are expected until their gates are built.

## 8. Batter-stance investigation (same day, later)
- Softened `BATTER_READY_R/TAP` (pelvis drop -12 to -9/-10, feet narrowed): stance now
  reads athletic instead of a squat. Verified in `RunReview/A_C26_BatterReady_R_lod0_0`
  (17:33): side-on, head square, knees bent, collar closed, no tear.
- Fixed a real pipeline bug in `c26_rig.bake`: loop-closing test used `is` (identity),
  so every mirrored (_L) loop clip gained one spurious 30-frame extension
  (BatterReady_L was 94 frames vs 64 for _R). Now `!=`; `BatterReady_L` re-authored
  to 0-64 and reimported. Other _L loops carry the same extension until rebuilt.
- Garment churn post-mortem: five successive "fixes" for the batting-yaw chest hole
  (occlusion variants, abdomen-flatten removal, face-mesh join, rigid trapezius plug,
  raised neckline) EACH regressed something, including breaking the previously clean
  fielder. All reverted; the match-verified garment script is restored untouched.
  Lesson: iterate garments against BOTH fielder and batter captures every round, and
  treat the deficit-loop stance depth as a deformation input, not just a pose value.
- Fielder + batter-stance both verified clean on the current review body (17:33
  captures). Match gate `character_outfield2` re-run on it: 10/10 new bodies,
  pickup/throw PASS, only the pre-existing old-batter blade FAILs remain.
- `Tools/BuildBatterReview.py` drafted (sockets BatGrip/Glove/Helmet/PadMount,
  v1 identity offsets, shot library with BatContact checks) but NOT yet run: no
  batter equipment exists in-engine yet. Running it is the next step, then
  offset iteration via review captures.
- Open: jersey hem still .975 (the .90 extension was part of the reverted churn;
  waistband skin flash at full stride remains), keeper kit/crouch, umpire outfit,
  hair/face LODs, foot-lock/IK, mobile profiling on hardware.

## 9. Batter slice: equipment fit, honest contact metric, guard fix
- `Tools/BuildBatterReview.py` creates `DA_C26_BatterReview` (28 clips, 6 gear
  items, `InspectRole(BATTER)` passes; keeper/batter cross-checks still reject).
  Sockets BatGrip/Glove/Helmet/PadMount live on the canonical skeleton.
- Offsets are MEASURED, not guessed: `fit_batter_offsets.py` ports the legacy
  PlaceKit formulas onto the posed review rig (round-trip 0.0cm), applied by
  `ApplyBatterOffsets.py`. First in-match capture: bat blade down, gloves on
  hands, pads on shins, helmet on head. Equipment materials dress per-slot
  (`DressEquipment`, same keys/tones as legacy); only a nonexistent Crown key
  misses (logged). Helmet shell's carbon-check look is its detail texture.
- **Broken metric found and fixed.** The blade check sampled the ball a frame
  AFTER contact (already travelled ~1m), so it could never pass for ANY system
  (it never did, old included). It now measures blade vs `LastContact.ContactPoint`.
- **Warp episode, honestly:** built per-ball visual root warping for the broken
  metric, then measured it lurching/clamping and ripped it all out. The honest
  `C26_CHARACTER_CONTACT` log (hands vs sim contact at the contact frame) is the
  remaining instrumentation. `ContactDelta` stays as data for future use.
- Facing verified correct at yaw -90 (clavicle-derived chest vector = proper
  right-hander's stance). A +90 experiment faced the keeper; a Rotator-order
  mishap laid the batter prone mid-experiment (fixed via named-field edits).
- **Guard fix:** striker stood leg-stump guard (-38) while clips assume middle;
  miss was almost entirely lateral. Guard moved to middle (+2) at all 4
  hardcoded sites (spawn, per-over reset, controller ×2; footwork ±35 preserved).
  Hands miss 105.6cm -> 81.0cm, true blade gap 100.4cm -> 73.8cm (stage 2: 60.4cm),
  blade (83cm) now within reach. Gate failures back to baseline count of 2.
  Batter on middle guard looks natural in broadcast framing.
- Open: true gap ~60-74cm vs 3.6cm threshold. Needs line-varied stride variants
  (straight-to-off-stump needs lateral stride the base clip lacks) and
  depth-matched shot selection for low full balls. No per-ball visual warping.

## 10. Next steps (not done)
Line/depth-varied batting clips + selection, keeper kit + crouch, umpire outfit
+ signals, hem extension with dual-pose verification, hair/face LODs,
foot-lock/IK pass, mobile profiling on hardware. Do not mass-migrate until
those gates pass.
