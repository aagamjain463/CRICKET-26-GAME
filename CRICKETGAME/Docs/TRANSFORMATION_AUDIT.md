# Transformation audit — 2026-09-09

Baseline: `322e23c`, UE 5.8.2, Apple M5 / 16 GB, Mac Metal SM6.
Fresh editor build passed; Cricket26 automation 3/3 passed; 17/17 rendered
beats completed a full Super Over. Evidence: `Artifacts/Captures/audit_baseline_0909`
and `Artifacts/automation_baseline_0909.log`. These are functional checks, not
acceptance of premium animation or phone performance.

## KEEP

- `Core/C26Rules.h`: sole score authority, delivery IDs/epochs, innings, chase,
  ties, free hits and restart protection. Native `AC26MatchGameMode` coordinates.
- Existing HUD, controller touch/mouse input, settings, menus and original identity.
- `FC26Simulation`: analytic flight/bounce, 240 Hz swept stump/rope checks.
- Existing `AC26CameraDirector`, replay capture/restore, primary/backup fielding.
- Regulation geometry: wickets ±1006 cm; popping creases ±884 cm; 305 cm strip;
  71.1 cm stumps; ball physics radius 3.6 cm; boundary radii 65.5 / 72 m.

## IMPROVE

- Contact synchronization: simulation-only test never measures the rendered bat.
  Tick advances the batter again after contact and bowler again after release.
- Batting framing crops striker feet; contact tracking changes aim abruptly.
- Equipment follows approximate joint offsets; helmet floats when head bends.
  Procedural trousers expose original shorts/skin around bent knees.
- Existing field forecast uses constant-speed arrival despite accelerating movement;
  pickups currently stop the ball before the hands arrive. Not accepted as premium.
- Audio cues exist (14 clips); impact variation exists. Footsteps reuse gather audio.
- Rendering: mobile deferred configured; Mac testing is desktop SM6. Conventional
  shadows; Nanite/Lumen/ray tracing disabled. Character pose/cloth updated every frame.
- Android ARM64 Vulkan/ES configured; iOS landscape Metal. No device package or profile
  verified. PSO precache enabled, actual coverage unmeasured.

## REPLACE

- Cricket action source: all batting/bowling is procedural `UPoseableMeshComponent`
  posing. Only A_Idle/A_Run/A_Catch/A_FielderThrow exist; none drives current poses.
  **ASSET BLOCKER: PREMIUM STRAIGHT DRIVE**.
  **ASSET BLOCKER: PREMIUM FAST BOWLING ACTION**.
- Hero equipment/kit silhouette needs authored cleanup; no claim that cheap sphere
  gloves/pads or current base character satisfy the final character target.

## DEFER

Full stadium rebuild, crowds expansion, shot libraries, new modes, commentary,
multiplayer, UI redesign and advanced animation systems without suitable motion.

## Exact ownership / asset inventory

See `GAME_ARCHITECTURE.md`. Startup map `/Game/Cricket26/Maps/L_SuperOver` uses
native match mode and stadium. Default engine GameInstance, no gameplay pawn or
custom scoring GameState. Cricket contains no ABP/IK rig/retargeter/physics asset;
template mannequin assets remain separate. Runtime effects are pooled procedural
billboards, HUD is native Canvas (not a cricket UMG Blueprint). Data is native tuning
structs, not an imported data-asset layer. Source assets resolve to
`/Users/aagamjain/Desktop/CRICKET-26/Assets/_Project`.

Plugins: Python/EditorScripting/ModelingTools (editor), ProceduralMeshComponent,
StateTree/GameplayStateTree. Tools include content/material/effect builders,
`AuditContent.py`, pixel sampler, and capture script. Target mobile/scalable;
packaging cooks L_SuperOver and /Game/Cricket26 using IoStore/compression.
Reference YouTube fetch was unavailable (timeout/throttled); not claimed as viewed.
