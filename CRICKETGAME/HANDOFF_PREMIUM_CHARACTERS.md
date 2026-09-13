# CRICKET 26 premium character rebuild — handoff

## Read this first

**The requested premium match overhaul is NOT complete.** The user stopped this pass because only 8% usage remained and requested a handoff. Preserve their remaining budget. Do not repeat the forensic investigation or claim success from the new MetaHuman assets.

Workspace: `/Users/aagamjain/Desktop/CRICKET26-Codex/CRICKETGAME`
Branch: `feature/premium-character-rebuild`
Original clean checkpoint: `17f34c8`
Backup branch: `checkpoint/pre-premium-character-rebuild-20260912`

The current default match STILL uses the old presentation. This is deliberate: the user explicitly requires gates A–F before migration. No approved `DA_C26_DefaultPlayer` exists. Do not bypass the gate to make the new feature appear complete.

## User's non-negotiable objective

Replace the old broken full character presentation, preserving scoring, controls, simulation, fielding decisions, commentary and presentation contracts. Full anatomical bodies, correct batter/bowler/fielder/keeper/umpire equipment, real skeletal locomotion and distinct authored cricket actions. No procedural hero-shot invention, no prettier overlay on a broken rig, no proprietary commercial-game assets, no paid dependencies without reporting. Actual match visual acceptance is mandatory; all eleven fielding-side players and rematch must be checked. Mobile performance must be measured.

Required vertical slice: batter with drive/run and correct gear; bowler with runup/delivery; fielder run/pickup/throw; keeper crouch/collect and keeper gear; umpire idle/signal. If these fail, do not mass-migrate.

## Work completed

1. Saved Git checkpoint/feature branch before asset changes.
2. Fresh Blender inspection of twelve source FBXs. Evidence: `Artifacts/CharacterAudit/source-geometry.json`. Findings written BEFORE asset replacement in `Docs/PREMIUM_CHARACTER_ROOT_CAUSES.md`.
3. Built the original project and ran its real GoldenGate match. 33 before captures in `Artifacts/Captures/character_before/`; six existing camera-composition failures. Simulation contact/release and pickup/throw checks passed. Open `0_07_pickup.png`: visible trouser-to-shoe gaps and malformed arms.
4. Implemented replacement C++ presentation component + native skeletal animation graph + profile/role/equipment validation. The graph was tested evaluating an existing run fixture: **67 bones changed**. It uses real USkeletalMeshComponent, not UPoseableMeshComponent.
5. **12/12 Unreal automation tests passed** in `Artifacts/CharacterAudit/automation-retest.log`, including four new character tests. This was BEFORE the final replay/left-handed attachment edits. Final handoff build status is recorded below; those last changes still need regression testing.
6. Successfully assembled and SAVED the original MetaHuman body/face/material/physics assets (~406MB, 267 assets). The old build script only saved the SOURCE MetaHuman and lost generated packages on shutdown. Fixed by new wrapper `Tools/AssemblePremiumFoundation.py`, which saves only dirty packages under the project's MetaHuman directory.
7. Created explicit source and target IK Rigs and IK Retargeter with root/pelvis/spine/neck/head, arms/hands/legs/feet and finger chains. **Retarget reference pose has NOT been visually reviewed. No animation batch was retargeted.**
8. Exported assembled Body/Face FBX from the full Unreal editor; authored a NEW full-body candidate in Blender, preserving complete MetaHuman skin and individual finger weights, with original fitted garment geometry and the project's original modeled cricket shoes. This is a development candidate, NOT premium-approved.
9. Added replay capture/restoration for the replacement graph and VisualBat() adapter for existing contact validation. Final edits include handedness forwarding and left-handed equipment attachment support.

## Verified root causes (do not repeat stale historical claims)

- Actual match spawns 14 `AC26Athlete` actors: bowler, keeper, nine additional fielders, two batters, umpire. Scene root; no player capsule or CharacterMovement. Match owns transforms.
- Old body is a `UC26PoseMesh` / UPoseableMeshComponent; no AnimBP/AnimInstance, montage, IK retargeter or modular leader-pose hierarchy drives it.
- `Animate()` samples A_Run, THEN overwrites both legs AND arms with procedural limb solves. Gait is gated by Action==Running and manually supplied MoveSpeed, not actual displacement. Some receiver movements update location without running state. The optional static-hero branch returns before animating.
- Batting and bowling clips exist but their application is explicitly disabled near the END of `Animate()`. Actual sports poses come from `C26Motion.h` equations/manual rotations. No off-spin or leg-spin AnimSequences exist.
- Arm reach is reduced again by `.492` after `.48` reference scaling; batting finger roots are shrunk to `.24` to fit gloves. Not proper hand deformation.
- Current source kit Body.001 only contains upper-body skin; trousers supply leg geometry. Current Match FBX lacks actual calf/foot skin and shoes. Cuffs stop at .31m in the oversized import, while static shoes are independently positioned; gaps visible in baseline match.
- **Current ten scan FBXs DO contain leg/foot vertices.** Prior comments that they are all half-bodies are false for these versions. Nine have only 16 weighted bones, no forearm weights and arms-down geometry against a T-pose rig. Runtime rejects scans by bind-span mismatch and uses the old kit.
- The reported floating torso symptom was NOT reproduced in inspected current baseline frame. Do not invent a confirmed historical cause.
- Current fielders inspected do NOT show separate batting pads. Existing kit correctly gates them to Batter OR Keeper, but keeper uses the SAME batting pads/gloves/helmet: there are no keeper-specific assets. Baked scan gear cannot be hidden through separate component toggles. Historical normal-fielder pad leakage not reproduced.

## Exact new source files

`Source/CRICKETGAME/Characters/`
- `C26CharacterProfile.h/.cpp`: canonical Epic humanoid bone requirements; per-LOD imported vertex/weight inspection; required role slots; duplicate gear, missing sockets, wrong skeleton, missing/duplicate contact-notify, root-motion and approval gates; data-driven appearances and clips.
- `C26CricketerAnimInstance.h/.cpp`: native UE sequence-evaluator nodes with full-body transition blend. No runtime manual limb posing. Evaluates simulation-selected explicit clip time. Marker notifies do NOT change simulation.
- `C26CharacterPresentationComponent.h/.cpp`: same-actor replacement body, socket-mounted role equipment, measured displacement/speed/direction/acceleration/turn rate, teleport reset, state selection, conservative shot fallbacks, quality LOD policy, debug overlay, frozen-motion warning and replay snapshots.
- `C26CharacterTests.cpp`: role isolation, displacement/event timing, asset rejection and real native graph evaluation.

Modified integration:
- `Source/CRICKETGAME/CRICKETGAME.Build.cs`: AnimGraphRuntime and RenderCore dependencies.
- `Source/CRICKETGAME/SuperOver/C26Athlete.h/.cpp`: component ownership; approved activation before legacy Configure; replacement Animate/hand/receive/LOD dispatch; VisualBat adapter. Legacy is hidden and bypassed only AFTER validation/initialization.
- `Source/CRICKETGAME/SuperOver/C26MatchGameMode.cpp`: forwards shot label/handedness; simulation arithmetic unchanged.
- `Source/CRICKETGAME/SuperOver/C26CameraDirector.h/.cpp`: replay snapshots of displayed sequence/time/blend/stride, not only old procedural parameters.
- `Source/CRICKETGAME/SuperOver/Tests/C26GoldenGate.cpp`, `C26ProductionGate.cpp`: use VisualBat().

## Assets and authoring files

Assembled MetaHuman:
`/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/`
- `Body/SKM_MH_C26_Player_001_BodyMesh`
- `Face/SKM_MH_C26_Player_001_FaceMesh`
- `BP_MH_C26_Player_001`
Shared body skeleton:
`/Game/Cricket26/Characters/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel`
This is the ACTUAL generated asset path, not proof that the requested 182cm athletic body constraints applied. Body mesh extends ~153.6cm to neck and has 342 bones/4 LODs; face has 875 bones/4 LODs. Verify combined stature and silhouette. Do not confuse neck-height body bounds with total height.

IK:
- `/Game/Cricket26/Characters/IK/IK_C26_LegacySource`
- `/Game/Cricket26/Characters/IK/IK_C26_MetaHuman`
- `/Game/Cricket26/Characters/IK/RTG_C26_LegacyToMetaHuman`

DCC sources:
- `ArtSource/Premium/Foundation/Body.fbx`, `Face.fbx`, `manifest.json` (exact bones/material asset mappings)
- `ArtSource/Blender/Premium/build_full_body.py`
- `ArtSource/Premium/FullBody/C26_FullBody_Candidate.blend`
- `ArtSource/Premium/FullBody/SK_C26_FullBody_Candidate.fbx`
- `ArtSource/Premium/FullBody/build-report.json`

**New combined full-body candidate is NOT IMPORTED INTO UNREAL YET.** About 64,947 vertices / 120,348 triangles before optimization, six mesh parts, zero unweighted vertices reported. Face weights map extra facial joints to body neck/head ancestors; advanced facial motion is intentionally absent from this candidate. Original assembled facial rig is preserved. Garments are first-pass offline geometry derived from body topology and need visual fitting/art refinement; do NOT call them premium. Shoes are original existing assets fitted/weighted offline. No old broken player mesh is used in this candidate.

Tool scripts:
- `Tools/AuditCharacterSources.py`: read-only Blender FBX audit, tested.
- `Tools/AuditCharacterAssets.py`: Unreal audit, updated for 5.8 get_lod_count; latest version NOT re-run. Earlier `lod_info` access failed because property is not exposed in 5.8. Native profile validation handles geometry when a candidate is supplied.
- `Tools/AssemblePremiumFoundation.py`: tested wrapper saving generated MetaHuman packages.
- `Tools/PreparePremiumFoundation.py`: exports + rigs; tested in FULL EDITOR, not commandlet.

## Immediate next steps — do these, not another broad audit

1. Read final build log and rerun automation after final replay edits. Fix compile/test failures before more assets.
2. Open `C26_FullBody_Candidate.blend` in Blender, inspect FRONT/SIDE/BACK, hands, wrists, head/neck seam, cuffs/shoes and clothing. Render actual body; it has not been visually reviewed. Reject/fix obvious clipping or wrong proportions. Current face mapping may need neck seam corrections.
3. Import candidate to `/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate`. Import at natural cm scale; validate 150–210cm full bounds, canonical bone names, ALL LOD geometry and weights. Preserve source assets. If FBX transforms introduce a new reference skeleton, use its own skeleton for the canonical master and create target IK rig for it; do not force incompatible bone transforms onto existing skeleton.
4. Remap imported material SLOT NAMES to assembled MetaHuman materials using Foundation/manifest.json, and Jersey/Trousers to project cloth material instances. No arbitrary scan atlas on unrelated UVs.
5. Build appropriate mesh LODs, keeping all anatomical regions and correct kit. 120k triangles is not a mobile field-player budget.
6. Inspect retarget pose, then retarget ONLY A_Run and representative challenging sports clips for review. Source `A_Run` uses a different source skeleton from some kit clips: select correct source preview mesh/rig. Source in existing IK rig currently uses SK_Cricketer_Match; verify source compatibility before any batch. No blanket retarget or approval.
7. Obtain/refine original/legal AUTHORED cricket animations. Existing drive/bowling clips are explicitly rejected historical first passes, not production quality. Need distinct cover/straight/on drive, pull, sweep, glance (both handedness), pace/off-spin/leg-spin, role idles, transitions, fielding/keeper/umpire actions. See profile validator for exact required keys/events.
8. Create real keeper pads/gloves and fit batting gear to new skeleton; add sockets. Create umpire-specific skinned outfit. Do not satisfy missing assets with renamed batting equipment or placeholders.
9. Create `DA_C26_DefaultPlayer` at `/Game/Cricket26/Characters/Data/`, leave ApprovedForMatch=false until actual gates pass. Add source/license and visual evidence.
10. Test native component in dev map, then real match with `-C26CharacterSlice` (structural validation still required). That flag currently bypasses only approval, and selects representative squad numbers; see caveat below. Full migration only after gates A–F. Visually test all roles, handedness, contact/release, locomotion, replays, rematch and cinematics. Profile mobile on an actual target; no mobile claim from desktop tests.

## Known implementation caveats to resolve

- Full validation currently requires the whole base animation set even in slice mode. Consider explicit staged gates for body/run review WITHOUT weakening production validation.
- Slice selection uses squad number 7 across roles, so it may select an extra fielder with number 7. Tighten to role+number to select exactly five representatives.
- `SourceAndLicense` and visual approval records are required but not provided; no default profile exists.
- Epic canonical skeleton assumed by validator, whole-body skinned mesh required. Raw modular MetaHuman Body alone correctly FAILS head-geometry checks. New combined mesh must be validated after import.
- Add explicit validation for LeftHandedSocket and alternate bat grip transforms. Ensure left-handed flag propagates before Configure and on preference changes. Current final patch forwards it at delivery preparation; audit reset/configuration ordering.
- Bowling hand/style architecture supports explicit data, but there is no complete roster hookup for left-arm actions yet.
- No Control Rig, FBIK, ground trace/foot lock, hand IK, motion warping, Motion Matching database, face animation or certified cricket clip library implemented. Do not describe authored-pose metadata as IK.
- Native graph samples marker-aligned explicit times, not montages. No automatic notify-driven scoring/ball physics (by design). Shot pre-contact time is warped to simulation; review whether preparation duration needs presentation-only earlier scheduling.
- Replay adapter is freshly added and has not been playtested. Verify wrap-around run time and action transitions before cinematics approval.
- Required profile keys are the authoritative import contract in `C26CharacterProfile.cpp`. Fallback is conservative, logs missing clips; never substitutes pace for spin.
- New body candidate needs shader/material and visual QA; it is not yet a replacement visible in ordinary match.
- Legacy system has NOT been removed/quarantined because quality gates have not passed; only its bypass interface is ready.

## Tool pitfalls already diagnosed — save time

- Engine is **UE 5.8**, macOS; `/Applications/Blender.app`, `/Applications/Cascadeur.app` available. Cascadeur not used.
- Use ABSOLUTE `-script=` paths. Relative paths resolve under Engine/Binaries/Mac and fail.
- `UnrealEditor-Cmd -run=pythonscript` FBX skeletal export CRASHED at SkinnedMeshComponent.cpp MeshObject assertion. **Full editor `-ExecutePythonScript=` successfully exported and created rigs.** Do not retry the crashing commandlet export.
- Original MetaHuman build script reports DONE even without saving assembled outputs. Use new wrapper. First pass had texture synthesis errors; second persisted output successfully. Inspect actual skin materials visually.
- Never save engine/plugin CDO packages. Wrapper only saves dirty `/Game/Cricket26/Characters/MetaHumans/` packages.
- Do NOT run two Unreal editor processes against the same project concurrently: observed CachedAssetRegistry temporary-file collision. Serialize asset/editor/test runs.
- `UWorld::CreateWorld` already initializes world; a second InitializeNewWorld crashed the new graph test on duplicate WorldSettings. Fixed; subsequent suite passed.
- Standalone SequenceEvaluator bTeleportToExplicitTime is private: use SetTeleportToExplicitTime(). SkeletalMesh uses FindSocket(), not DoesSocketExist().
- Avoid relying on very stale Docs/CURRENT_TASK/AUTHORED_ANIMATION completion claims. Fresh logs above supersede them.

## Commands (from project root; one Unreal process at a time)

Build:
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" CRICKETGAMEEditor Mac Development -Project="$PWD/CRICKETGAME.uproject" -WaitMutex
```
Automation:
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CRICKETGAME.uproject" -nullrhi -unattended -nosplash -nosound '-ExecCmds=Automation RunTests Cricket26; Quit' '-TestExit=Automation Test Queue Empty' -abslog="$PWD/Artifacts/CharacterAudit/next-automation.log"
```
Baseline/current match (currently OLD presentation):
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/CRICKETGAME.uproject" /Game/Cricket26/Maps/L_SuperOver -game -windowed -ResX=1600 -ResY=900
```
Actual match capture gate:
```bash
bash Tools/GoldenGate.sh character_next -C26GateFPS=30
```
Blender rebuild candidate:
```bash
"/Applications/Blender.app/Contents/MacOS/Blender" --background --python "$PWD/ArtSource/Blender/Premium/build_full_body.py"
```
Full editor asset prep (already done):
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/CRICKETGAME.uproject" -ExecutePythonScript="$PWD/Tools/PreparePremiumFoundation.py" -unattended -nosplash -nosound
```
Debug CVars after valid profile activation: `c26.Character.Debug 1`, `c26.Character.Number 7`, `c26.Character.Role 0..5`, `-1` returns to match role.

## Resume prompt

“Read HANDOFF_PREMIUM_CHARACTERS.md and Docs/PREMIUM_CHARACTER_ROOT_CAUSES.md. Continue the existing feature/premium-character-rebuild branch. Do not redo the audit or claim the MetaHuman in Content is success. First verify the final source build, visually inspect/import the new full-body candidate, then meet authored-motion/role-equipment gates before real-match migration. Preserve simulation and keep token use low. Report asset and quality blockers honestly.”

## Final checkpoint verification

Final source build after replay/handedness changes: **SUCCEEDED**, `Artifacts/CharacterAudit/handoff-build.log`, 91.27 seconds. No Unreal editor/game processes from this task were active when the handoff was prepared. The last successful 12-test automation run predates those final changes; no new match/visual acceptance was claimed.
