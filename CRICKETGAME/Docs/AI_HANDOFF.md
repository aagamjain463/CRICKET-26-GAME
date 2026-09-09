# CRICKET 26 — AI HANDOFF

## Claude (Opus 5) continuation — 2026-09-09, kit and drive pose. Supersedes the sections below.

Branch `work/golden-delivery-contact`. Commits `4f5ab04` (rendered golden gate) and the kit/pose
commit that follows it. Build **succeeds**, automation **3/3 PASS**
(`Cricket26.Rules.SuperOver`, `Cricket26.Simulation.GoldenDelivery`,
`Cricket26.Simulation.Trajectories`), `C26_GATE_PASS failures=0 frames=25` on every run,
and `-C26Smoke` completed **10/10 autonomous matches with 0 errors**
(`C26_SMOKE_PASS matches=10 boundaries=32 wickets=18 replays=50 extras=2 actors=14`,
`Artifacts/smoke_kit.log`). The smoke ran on the binary containing the pose, pad, glove and
helmet work; the sleeves landed after it and are an additive mesh section with no gameplay
coupling, verified by a further gate run (`gate_sleeve2`).

### The gate is now the tool to use

`bash Tools/GoldenGate.sh <label>` renders drive -> miss -> restart -> drive in about 90 s and
asserts against the **rendered** frame rather than the simulation's intent. It writes
`Artifacts/Captures/<label>/*.png` and `Artifacts/<label>.log`. It measures:

- `C26_GATE_RELEASE hand_gap_cm` — the ball's distance from the bowler's hand on the release frame.
- `C26_GATE_CONTACT gap_cm` and `ball_local` — the ball's distance from the **generated bat blade
  triangles**, and where down the blade it landed. The blade runs local Z 0 to -83; -58 is the
  widest point of the willow, and is where a middled drive belongs.
- One score commit per delivery, a clean `Rules.Epoch` after restart, replay and time dilation
  cleared, and frame times over the run-up/delivery/in-play window (desktop only, not a device claim).

Current numbers: `hand_gap_cm=0.000`, contact `gap_cm=0.523` at `Z=-57.9`, `failures=0`,
mean 20.4 ms / p95 22.3 ms with screenshots on, in-editor, on this Mac.

### What this session changed

All in `Source/CRICKETGAME/SuperOver/`. Rules, scoring, innings and Super Over logic untouched.

- **`Tests/C26GoldenGate.cpp` + `Tools/GoldenGate.sh`** (from the previous agent's working tree,
  verified and committed here) — the harness described above.
- **Contact on the meat of the bat.** The contact solver in `C26Athlete::Animate` now pulls the
  grip to `Contact + Dir*61.5` as well as pivoting the blade through the ball. Measured contact
  moved -75.7 cm (the toe) -> -69.9 -> **-57.9 cm** as the pose work below let the arms reach.
- **`BuildGloves()`** — batting gloves are real geometry: flared cuff, closed fist, four separate
  finger rolls arcing over the knuckles, thumb up the side. They were scaled engine spheres, which
  read as one white lump where the batter's hands should be. `GloveL/GloveR` are now
  `UProceduralMeshComponent`.
- **`BuildPads()`** — batting pads are real geometry: three bolsters, a knee roll, and wings that
  carry past the widest part of the leg. The old scaled sphere was 20.5 cm wide and pushed 4.2 cm
  forward, so from the gameplay camera — which watches the striker **from behind** — it vanished
  entirely behind his own calf and the batter appeared to be batting without pads. `PadL/PadR` are
  now `UProceduralMeshComponent`.
- **Helmet ear and nape coverage** — `Shell` section 1 is a skirt whose drop is driven by azimuth,
  so the opening sweeps up over the face instead of being cut as a hole. A bare crown read as a cap
  from every side angle. A fielder's cap hides section 1 and keeps the squashed crown.
- **The drive is a drive, not a squat.** Batting `Crouch` was a fixed -23 cm hip drop for the whole
  stroke. It is now -13 cm in the stance, dipping to -23 at contact and recovering through the
  follow-through, and a new `Shift` moves the **pelvis forward** onto the striding foot (17 cm on a
  straight drive). `LeanForward` at contact went 28 -> 38 degrees. This is what moved the contact
  point onto the middle of the blade: the arms are only 51.8 cm long (`C26_RIG armspan`), so the
  body has to travel or the bat can only reach the ball with its toe.
- **Lifted feet roll onto the toe.** Every foot was pinned to its reference rotation, so any foot
  off the ground hung flat in the air — the back heel of a drive and every stride of the bowler's
  run-up. Lift now drives up to 36 degrees of toe-down.
- **`Tools/CropFrame.py`** — crops and nearest-neighbour magnifies a region of a capture PNG,
  stdlib only. `SamplePixels.py` answers "what colour is this"; this answers "what shape is this".
  Every kit judgement in this session came from it, e.g.
  `python3 Tools/CropFrame.py Artifacts/Captures/<label>/0_10_replay_contact.png /tmp/a.png 480,230,620,480 2`

### Second pass in the same session — bowler approach and gate portability

- **`Tools/GoldenGate.sh` could never fail a caller.** Its reporting step used `rg`, which is not
  installed here, so under `set -e` the script exited 127 after a perfectly good run. It uses
  `grep -aE` now. If you are reading a handoff that claims the gate passed, check that the claim
  came from a run after this fix.
- **Fast bowler approach.** `Role==Bowler && Action==Running` gets a longer stride (50 cm), a high
  knee drive (33 cm, was 24), arms pumping with the elbows tucked and the leading hand rising, and
  18 degrees of lean. Fielders and running between the wickets keep the neutral run untouched.
- **Cap versus sun hat.** The peak is narrower and forward-only for players; only the umpire keeps
  the full brim.
- **Latent normal-frame guard.** Limb tubes crossed against `RigForward`, which degenerates when a
  limb points along it. There is now a blended reference that rolls toward vertical. Be clear about
  what this did: it is a guard, **not** a fix for anything visible. The run-up's bright leading
  thigh measures luma 129 against 74 for the rest of the same kit both before and after the change,
  so that contrast is genuine key light on a raised thigh beside a self-shadowed trailing leg.

### Known issues, honestly

- The shirt is one smooth volume: the near arm merges into the chest in replay close-ups. This is
  the next visible bottleneck and it is in `SK_Cricketer_KitBase`, not in the procedural kit.
- The head under the helmet is an undetailed dark mass at close range.
- A thin dark wedge sits where the front pad's top meets the trouser. It reads as thigh shadow on
  the pad, not as a geometry break, but it has not been proven to be shadow.
- Cricket motion is still **procedural and refined, not authored or captured**. The premium-motion
  asset gate is OPEN. Do not describe the stroke as mocap.
- No on-device measurement exists. The frame times above are desktop editor numbers.

### Next exact task

Torso and sleeve silhouette on `SK_Cricketer_KitBase` so the arms separate from the chest, then
re-run `Tools/GoldenGate.sh` and inspect `0_10_replay_contact.png` with `CropFrame.py`.

---

## Active continuation — superseded by the section above

Branch: `work/golden-delivery-contact`. Last tested checkpoint: `322e23c`.
Milestone: Golden Delivery repair; premium motion gate remains OPEN.
Fresh baseline build succeeds, automation 3/3 passes, rendered capture 17/17
completes a Super Over (`audit_baseline_0909`). See `TRANSFORMATION_AUDIT.md` and
`CURRENT_TASK.md`. Changed so far: seven memory/audit documents; no imported assets.
Relevant assets: L_SuperOver, SK_Cricketer, four generic animation clips.
Known issues: procedural cricket motion, cropped striker feet, floating helmet,
cloth skin exposure, unmeasured rendered contact. Next exact task: deterministic
runtime contact/release measurement and drive/miss/reset capture before repairs.
No on-device/mobile performance claim. Historical "golden" claims below are not
the user's premium quality acceptance.

## Golden Delivery session — 2026-09-09 (Muse Spark, supersedes nothing, extends the Claude section)

Task: first AAA quality transformation of ONE delivery. The base was already strong (scale,
broadcast lens, hand release, contact-anchored cameras all verified in captures), so this session
made targeted upgrades and verified each one in rendered frames. Build **succeeds**, automation
**3/3 PASS** (incl. new `Cricket26.Simulation.GoldenDelivery`), captures **17/17** twice
(`Artifacts/Captures/golden_01/`, `golden_02/`), smoke **6 full matches clean, 0 errors**
(killed by operator timeout at match 7, not by a failure — see `Artifacts/smoke_golden.log`).

### What changed (all in `Source/CRICKETGAME/SuperOver/`, rules/scoring untouched)

- `C26MatchGameMode.cpp` — ball render 1.0x → **1.6x** real size (physics stays 3.6 cm; a
  true-size ball is ~3 px on a phone; old dead 3.5x line removed). Distance-based run-up
  **footsteps** (`fielder_gather` @ 0.10 vol every ~95 cm from `LastStepY`, init at the mark in
  `StartDelivery`). **Crowd swell** (`crowd_anticipation`, was shipped but never loaded/cued)
  under the bat sound, scaled by contact quality. `-C26Debug` trail (RunUp/Delivery/InPlay) +
  green release sphere + gold contact sphere via `DrawDebugHelpers`; dev only, off in captures.
- `C26Audio.cpp` — manifest adds `crowd_anticipation`; `fielder_*` joins the pitch/gain-varied
  impact group so footsteps never machine-gun.
- `C26CameraDirector.cpp` — batting lens punched in: (310,2300,400) @ **36°** (was
  (322,2455,432) @ 37°); contact-hold shot matched. Lofted tracking **tightens FOV with height**
  (tower + chase) instead of zooming out into a pixel-ball. Running/square camera aims at the
  ball's **ground line** (Z clamped 320) so skiers no longer tilt up into the stands.
  Replay close-up is **loft-aware** (aim hands to the ball .62/.55 when aerial vs .42/.20).
- `C26Athlete.cpp` — straight-drive emphasis for |angle|≤15° non-loft: front stride 26→34 cm
  amplitude, +4° forward lean, squarer chest. Trouser knee/calf radii 7.1/6.4 → 7.8/7.0 to stop
  skin peeking through in replay close-ups.
- `Tests/C26Automation.cpp` — new `Cricket26.Simulation.GoldenDelivery`: hand release origin,
  readable timescale, +Y incoming, pre-contact bounce, contact at striker's end, Perfect/Good
  `STRAIGHT DRIVE` with quality >.5, contact point == ball (no teleport), -Y redirect with
  |Vx|<|Vy|, grounded launch, finite 2 s rollout.
- `C26Stadium.cpp` — unity-build `-Wshadow` fix: `MakeSign` param `Ink` → `Glyph` (baseline did
  not compile on receipt; `Teal` clash from STATUS was already gone).

### Verified in captures (golden_02 vs fx_01 baseline)

- `08_delivery_batting`: bowler's arm at release, **ball clearly visible mid-pitch** (was ~3 px).
- `09_in_play`: PERFECT contact, batter follow-through with visible bat, ball readable in flight.
- `10_fielding`: was a frame of empty stands; now a proper square running-camera frame (both
  batters, bowler follow-through, umpire, fielders).
- `12_replay`: was ball stranded near the rope while the bat swung at air; now ball and bat in
  one slow-mo frame (incl. a bowled at 0.28x with the ball inches from the bat).

### Known visual limits (genuine, not excuses)

- Gloves still read as smooth blobs at replay distance (needs knuckle/cuff silhouette).
- Tiny skin peek can survive above the front pad in extreme close-ups (thigh vs pad top).
- Helmet reads as a cap from pure side angles; grille only resolves from the front.
- No dedicated footstep sample yet — footsteps reuse `fielder_gather` quietly.

### Next, in order

1. Crowd/anticipation mix pass on device speakers; consider one synthesized soft footstep.
2. Glove silhouette + helmet side profile (cheap geometry, big replay payoff).
3. `stat unit` profile of a representative delivery; record in SUPER_OVER_STATUS.
4. HUD safe areas for 19.5:9/20:9; mobile quality tiers → `CrowdQuality`.
5. Continue brief priorities: backup-fielder polish, running-between-wickets polish.

---## Claude (Opus 5) continuation — 2026-09-09, later than the Codex section below

**This section supersedes the Codex section that follows it.** BUG C (blue ground) is fixed and
the two root causes Codex identified were both applied and verified in rendered captures.

> **Concurrency warning.** During this session a second agent was editing the same files live
> (it added `AC26Athlete::Shade` contact shadows, `M_Shade`, `M_Beam` and
> `Tools/BuildPresentationAssets.py`). Both sets of changes coexisted and compiled, but if two
> agents run at once, re-read a file immediately before editing it and prefer anchored,
> assert-on-miss replacements over whole-file writes.

### Verified state

- Editor build **succeeds**. `Tools/Capture.sh` completes **17/17** beats (`C26_SHOTS_COMPLETE`).
- Latest good captures: `Artifacts/Captures/fx_01/` (current), `rig_fix_01/` (previous),
  `transformation_01/` (Codex baseline, still shows the bugs listed below as fixed).
- Rig metrics logged at startup as `C26_RIG`: shoulder 143.9, hip 100.4, ankle 11.8,
  **armspan 51.8 cm**. The arm span number matters — see the bat fix below.
- Five milestone commits landed on `main` (`8db079a` venue, `5517c34` athlete, `cdab64a` camera,
  `7aef99c` fx, `aac61b3` match wiring). Before this session **nothing had ever been committed**.

### Bugs found and fixed this session

1. **The bat was completely detached from the hands.** `PlaceKit` drew the blade at the grip the
   shot *asked for*, while the hands were placed by two-bone IK that **clamps silently at full
   extension**. The follow-through asked for ~53 cm of reach from an athlete with a 51.8 cm arm
   span, so the hands stopped short and the bat carried on without them. The blade now spans the
   two *posed* palms (`Palm()` reads wrist→`HandMiddle1`), so separation is structurally
   impossible, and grip targets are clamped to `ArmSpan*.95` so the pose stays inside the body.
   **Never place equipment from an IK target; place it from the joint that actually resolved.**
2. **In-play camera was a near-vertical plan view of empty grass.** The main tower sat 17.8 m
   behind the striker at 11.8 m high — a 33° downward look. Now 33 m back at 7.8 m (13°).
   `Look()` also gained a `MaxDrop` angle limit that holds framing by **standing the camera
   further off rather than tilting it down**, so no gameplay shot can regress to a game board.
3. **Pads and trousers interpenetrated.** Pads took a fixed mesh-space forward offset, so they
   slid sideways the moment the batter turned side-on; and trouser radii were roughly twice life
   size, extruding cloth out through the pads. Pads now follow the pelvis facing; radii are real.
4. **Athletes were the dimmest things on screen.** Measured luma: turf 105–120, pitch 153, crowd
   38–46, but bowler 69 and umpire 53. A steep key light rakes the horizontal ground and barely
   touches a vertical torso. `CrossLight` moved from −51° to −33° pitch at 1.95 intensity, and kit
   albedo was raised. **Re-measure this after any lighting change** (see the sampler in §5 below).

### Added this session

- `C26Effects` — pooled camera-facing billboards on one fixed-capacity mesh section (one draw
  call, no Niagara). Pitch dust, turf scuff, bowler foot plant, stump burst. Driven from the
  simulation's existing `BounceEvent` / `StumpEvent`.
- `M_Particle` + `Tools/BuildEffectAssets.py`. **Pin-name trap:** a `TextureSample` calls its
  colour output `'RGB'` but a `VertexColor` calls the same thing `''`. The builder treats only
  those two as interchangeable — a single-channel request like `'A'` must never fall back to the
  full colour output, or it wires the wrong data and still looks like success.
- Hit-stop on a clean strike (30–48% dilation for 55–75 ms, measured on **real** seconds so the
  dilation cannot extend its own duration). Released in `PrepareDelivery`, `StartMatch` and
  `Menu`, because a match left in slow motion is exactly the stale state that breaks Play Again.
- Impact audio now varies pitch and gain for bat/ball/stump/keeper cues; crowd and UI stay
  unvaried. Bat transient volume scales with contact quality.

### Exact next tasks, in order

1. Re-enable the four pylon spot floods at quality 3 and calibrate `FloodCandelas` (start 26000,
   **one capture per change** — an earlier 2,400,000 cd was ~9 stops hot and washed to white).
2. HUD safe areas for 19.5:9 and 20:9, and mobile quality tiers wired to `CrowdQuality`.
3. Profile a representative delivery (`stat unit`) and record numbers in SUPER_OVER_STATUS.
4. Gloves still read as smooth blobs at replay distance; give them a knuckle/cuff silhouette.


## Codex continuation — active, 2026-09-09

Baseline UE 5.8.2 editor build **succeeded** on receipt of this handoff. Existing dirty source
changes are the previous agent's work and are being preserved/extended. The untracked Barlow
font asset remains user-owned and excluded from milestone commits. Read the prior audit below;
its "complete" claims in SUPER_OVER_STATUS are historical and not current acceptance.

Current task: diagnose why procedural venue surfaces lose their authored appearance, then
validate athlete skin scale/equipment and delivery synchronization in rendered captures. Native
match/rules, input, camera, replay, AI, materials, asset builder and configs have been inspected.
No cricket gameplay Blueprint owns authority; templates remain out of scope. Baseline screenshot
06_ready_batting confirms the unresolved blue surface and oversized hair/buried equipment.
Do not rebuild scoring or restart the project. Next checkpoint: short repeatable visual probes,
correct playing surface, proportionate skinned athlete and visible cricket equipment.

### Confirmed root causes (supersedes BUG C hypotheses below)

- `Bowl` pointed at `TRASH_ProceduralMeshComponent_0`, proven by runtime logging. The previous
  session renamed the native default subobject from `ContinuousStadiumBowl` to `EclipseOvalBowl`.
  The saved map retained its old pointer. All 19 generated sections were written into the orphan;
  the registered component contained zero sections. Blue foreground was fog over empty space.
  Restore the original default subobject name; never rename serialized native subobjects casually.
- Athlete joint positions were multiplied by 0.48 but component-space skin scales were not.
  This compressed bone lengths while leaving skin offsets/head/hair at the original import size.
  Scale both translation and scale in every reference component-space transform, keeping equipment
  in real centimetres. Merely enlarging the helmet was treating the symptom.
- Added read-only `Tools/AuditContent.py` and fast `-C26Probe=<label>` ready-screen captures to
  `Artifacts/<label>.png`. `probe_baseline` and `probe_nofog` preserve before evidence.



**Last updated: 2026-09-09. This section supersedes everything below it.**
Read this file top-to-bottom before touching anything. The historical log at the bottom is kept for
context but parts of it are now known to be wrong (see "Corrections to earlier sessions").

---

## 0. HOW TO WORK ON THIS PROJECT (read first)

- Project: `/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/CRICKETGAME.uproject`
- Git repo root is the **parent** dir `/Users/aagamjain/Desktop/CRICKET-26-GAME`, branch `main`,
  base commit `6d67944`. **Nothing from this session has been committed yet.**
- Engine: `/Users/Shared/Epic Games/UE_5.8` (installed 5.8.2). Do not upgrade.
- **Build command** (always run this after any C++ edit; `-Werror` is on, including `-Wshadow`):
  ```
  cd /Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME
  "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
    CRICKETGAMEEditor Mac Development "$PWD/CRICKETGAME.uproject" \
    -WaitMutex -NoHotReloadFromIDE 2>&1 | grep -E "error|Error:|Result:"
  ```
  A one-file change rebuilds in ~15-60 s.
- **Visual iteration loop** (this is the single most valuable tool in the repo):
  ```
  Tools/Capture.sh <label>        # ~4-5 min, writes Artifacts/Shots/*.png + Artifacts/<label>.log
  ```
  It launches the game headless-ish with `-C26Shots`, walks 17 scripted presentation beats
  (menu → intro → batting → delivery → in-play → reaction → replay → interval → bowling → result)
  and screenshots each one. **Always look at the PNGs after a change.** Do not trust reasoning
  about rendering — measure it.
- **Measure pixels, don't eyeball.** There is a pure-python PNG reader snippet used this session
  (see §5). Sampling actual RGB values is how the material bugs below were found.
- Other runs: `-C26Smoke` plays 10 autonomous matches and logs `C26_SMOKE_PASS`.
  Automation tests: `Cricket26.Rules.SuperOver`, `Cricket26.Simulation.Trajectories`.
- Untracked file to preserve, never delete, never commit as ours:
  `Content/Cricket26/UI/Fonts/BarlowCondensed-SemiBold_Font.uasset`
- Baseline screenshots for before/after comparison are in `Artifacts/Shots_baseline/`
  (and `Artifacts/Shots_prev/`). Current output is `Artifacts/Shots/`.

---

## 1. THE THREE ROOT-CAUSE BUGS FOUND THIS SESSION (most important knowledge here)

These explain almost every "it looks like a prototype" complaint in the brief. Two are fixed, one
is still open.

### BUG A — Outfield was unlit because its triangles were wound backwards. **FIXED**
`C26Stadium.cpp` used to build the stand rings with one triangle winding and the outfield strips
with the opposite winding. Every generated material has `two_sided=True`, so the renderer flipped
the ground's shading normal to point **down**. The whole field was lit only by the sky light's
lower hemisphere — that is the "flat dark teal outfield" in the old screenshots, and by contrast it
made the pitch look blown out.
Fix: all venue geometry now goes through `FC26Surface::Quad(A,B,D,E,Want,...)` which computes the
face normal and **emits whichever winding makes the shading normal face `Want`**. Never hand-write
triangle indices for this venue again; use `Quad`, `Band`, `Plate`, `Sweep`, `Box`.
UE's procedural-mesh face normal for triangle (V0,V1,V2) is `(V1-V2) x (V0-V2)`.

### BUG B — `M_Grass`, `M_Pitch`, `M_Willow`, `M_Crowd` silently render as DEFAULT GREY. **WORKED AROUND**
These are the materials `Tools/BuildContent.py` creates with `textured=True`, i.e. with a
`WorldPosition -> Multiply -> Noise` chain built through the Python material API. That chain does
not resolve on this renderer, so anything drawn with them falls back to the engine default grey
material. This is why the "pitch" was a giant white slab and the outfield never had grass colour:
**those were never the authored colours at all.**
`Tools/FixMaterials.py` header already documents the same class of failure for usage flags.
Current workaround: **everything is drawn with `M_Surface`-derived `UMaterialInstanceDynamic`s**
(`Tint`, `Roughness`, `Glow` params). `M_Surface` is the only generated material with
`used_with_skeletal_mesh` set, and it is verified to render on procedural meshes, HISMs and the
skinned cricketer. Variation now comes from **geometry** (36 mowing bands in 6 tones, watered
collar, 5 prepared strips, worn landing areas) instead of a noise node.
**TODO for the next agent:** optionally repair `M_Grass`/`M_Pitch`/`M_Willow`/`M_Crowd` with a
Python pass (pattern: `Tools/FixCrowd.py`) using node types that are known to work, then switch the
turf/pitch/bat back to them for real fine detail. Verify with a capture, not by assumption.

### BUG C — **STILL OPEN: the ground renders blue.** ← START HERE
This is the top-priority unresolved problem. Measured pixels from
`Artifacts/Shots/06_ready_batting.png` (capture label `venue3`):

| sample | RGB |
|---|---|
| ground near (250,700) | (62, 87, 123) |
| ground mid (300,520) | (63, 88, 124) |
| ground far (700,380) | (108,149,190) |
| **crease mark (1180,428)** | **(63, 88, 124) — identical to the ground** |
| sightscreen (1000,200) | (153,159,166) |
| crowd (200,90) | (39, 51, 67) |
| shirt (535,660) | (89, 217, 226) |
| sky top (1000,10) | (102,143,184) |
| stand rows (1450,200) | (101,141,183) |

Key deductions already made:
- The venue log says `Eclipse Oval built: 19 mesh sections, 28 batches, 43134 instances`, so the
  6 turf sections **are** being created and `Emit()` skips empty surfaces — the geometry exists.
- The turf and the crease paint are **both** `M_Surface` MIDs on the **same** procedural mesh
  component, differing only in `Tint` (green vs near-white). They sample to the *same* pixel value.
  So the difference between them is being lost — i.e. neither is showing its albedo.
- Ground (62,87,123) ≈ sky (102,143,184) in hue. Stands and sightscreen, which are also `M_Surface`
  MIDs, *do* show their own values. So it is specifically the near-horizontal ground plane.

**Untested hypotheses, in the order worth trying (each is one capture cycle):**
1. The crease-mark sample point may simply have missed the thin line — re-sample with a small
   window scan, or temporarily paint the turf magenta (`Tint = (1,0,1)`) and capture. If the ground
   turns magenta, the material path is fine and the problem is lighting/fog. If it stays blue, the
   ground sections are being drawn *over* by something.
2. Something is drawn on top of the ground. Suspects in `AC26Stadium::BuildVenue()`:
   the `Ap` (apron) sweep at Z=2.5 and the `Ring` collar at Z=2.0 use `Oval(...)` radii that may
   overlap the whole field; and the `Sky` dome (`Sky` component, 6 sections, radius 34000,
   elevation `-0.06 .. PI/2`, emissive `Glow=5.5`, two-sided) — verify it really is a dome above
   the horizon and not a closed sphere. Try `Sky->SetVisibility(false)` for one capture.
3. Exponential height fog: `Haze` density `.000042`, falloff `.09`, max opacity `.62`, start
   distance 2600, inscattering `(.055,.086,.145)` — a blue. Try `Haze->SetVisibility(false)` for
   one capture. The fact that ground-far is *lighter* and bluer than ground-near is fog-shaped
   behaviour, but the near ground should be unfogged at 26 m start distance.
4. The directional key is only 2.35 lux with the sky light at 0.42; if the ground is somehow only
   receiving sky light (blue, `(.40,.50,.66)`) it would go blue. Check `KeyLight` shadowing —
   `DynamicShadowDistanceMovableLight=9000`, 3 cascades. Try `KeyLight->SetCastShadows(false)`
   for one capture; if the ground turns green, something large is casting a shadow across it.
   (`Bowl`, `Sky` and all HISM batches are already `SetCastShadow(false)`.)

Isolate one variable per capture. The magenta test in (1) is the cheapest first move.

---

## 2. WHAT WAS CHANGED THIS SESSION (all uncommitted, all compiling)

Build status: **Succeeds.** Runtime: capture pass completes 17/17 beats (one warning:
`13_interval` skipped, pre-existing timing issue). No crashes.

### `Source/CRICKETGAME/SuperOver/C26Stadium.h` / `.cpp` — venue fully rewritten
- New `FC26Surface` geometry builder with guaranteed-facing `Quad`, plus `Band` (ellipse-clipped
  mowing strip), `Plate`, `Sweep` (ring between two ellipse profiles), `Box`.
- Geometry is now **batched by material into ~19 sections** instead of ~130 one-ring-per-section.
- Ground: 36 mowing bands in 6 tones; darker watered collar inside the rope; dry apron between rope
  and hoardings; square laid **flat into** the turf (no floating slab edge); 5 prepared strips with
  the centre one the real 3.05 m x 20.12 m pitch; worn bowler landing areas and crease wear;
  flat crease paint (popping 3.66 m, bowling 2.64 m, return creases).
- Architecture: perimeter wall, 22-row lower bowl, glazed hospitality ribbon, 18-row upper bowl,
  cantilevered roof with dark underside + illuminated fascia band, roof trusses.
  **Deliberately removed** the ring of 36 ground-to-roof columns — they read as a forest of poles
  through the seating gaps from every camera. Do not add them back.
- Six floodlight pylons (masts only above the roof line) with 5x10 emissive lamp arrays, plus a
  continuous roof-edge lighting rig.
- Sight screens behind both bowler's arms; boundary rope; LED hoarding ring in two tints;
  30-yard circle dashes; original signage and two broadcast screens.
- Crowd: 8 shirt groups, deliberately dark (luma 0.04-0.13) so the field stays the brightest thing;
  seats in a two-tone banded pattern; aisles; empty seats; per-spectator lean/scale jitter.
  Density and heads scale with `CrowdQuality` (`Pitch2` spacing 116/132/168/230 cm, `RowStep`).
- Lighting: `KeyLight` 2.35 lux shadowed + `CrossLight` 1.05 lux unshadowed, with explicit
  `ForwardShadingPriority` 1/0 — **this silences the on-screen "Multiple directional lights are
  competing…" error that was burned into every old screenshot.**
- Four `USpotLightComponent` pylon floods exist but are **currently `SetVisibility(false)` and only
  enabled at quality 3**. `FloodCandelas = 26000`. **Warning: an earlier value of 2,400,000 cd was
  ~9 stops too hot and washed the entire scene to white.** Re-enable carefully, one capture at a
  time.
- Post grade: manual exposure `AutoExposureBias = 1.15`, bloom `BM_SOG` 0.30 @ threshold 1.55,
  no grain, no chromatic aberration, motion blur 0, mild AO, vignette 0.22.
- `UpdateAtmosphere` is now fixed-step (frame-rate independent celebration decay) and drives LED,
  lamp and crowd glow.

### `Source/CRICKETGAME/SuperOver/C26Athlete.h` / `.cpp` — posing rewritten
**The big find:** the Mixamo rig is a T-pose facing **mesh +Y** with **mesh +X out to the
character's LEFT** (verified from the `C26_RIG` log: `LeftHand X=+70.3`, `RightHand X=-70.0`,
`LeftToeBase Y=+9.1`). The old pose code assumed X=forward, Y=right — i.e. **every athlete in the
game was standing 90 degrees off**, which is why the batter read as a hunched blob.
- `Mesh->SetRelativeRotation(FRotator(0,-90,0))` so the actor's +X is the athlete's forward. All
  existing yaw maths in the game mode (fielders facing the middle, striker at -90, bowler at +90)
  is now correct **without changing the game mode**.
- All equipment re-parented from `RootComponent` to `Mesh` so it shares one frame with the pose.
- `static FVector Rig(Forward,Right,Up) { return FVector(-Right,Forward,Up); }` is now the **only**
  way pose targets are expressed. Keep it that way.
- New `Twist(bone, TurnRight, LeanForward, LeanRight)` rotates a bone about mesh axes pivoting on
  itself; used to open the batter **side-on** (46 deg through hips/spine chain) without moving any
  IK target.
- `AimHead()` rewritten: it finds which head-local axis currently points out of the face (via the
  reference rotation) and rotates *that* onto the target, clamped to 68 deg for the head and 26 deg
  for the neck, with 40% neck follow. The old version aimed the neck→head bone vector, which tipped
  the skull sideways.
- Knee bend vector corrected to `RigForward` (was mesh +X = sideways).
- Bat rebuilt: flat hitting face on local +X, swelled back, 83 cm handle-to-toe, separate dark
  rubber grip section. Placed with `MakeFromZX(Dir, FaceDir)` so **the bat face aims along the
  shot**. `BatLength=83`, `MiddleDrop=62` (ball meets the middle 62 cm below the hands) are shared
  constants so bat, hands and contact point cannot drift apart.
- Helmet was a **40 cm sphere centred 18 cm above the head bone** — that was the "hood" look. Now
  23.5 cm, centred +7, with a peak and a 5-bar grille built in the head's own forward frame.
- Pads placed knee→ankle with forward offset; gloves placed at the hand bones; keeper gets bigger
  gloves and a deeper crouch.
- Kit split into **separate shirt and trouser materials** (trousers cooler/flatter) so the body no
  longer reads as one moulded block of colour. All athlete materials derive from `M_Surface`.
- Shot animation reworked in the correct frame: backlift over the off shoulder, down through the
  ball, follow-through wrapping in the direction the ball was actually hit; separate vertical-bat
  and horizontal-bat (`Cross`) families; defensive push; celebrate/disappointed poses.
- Bowling action reworked: gather → braced front foot → arm through the vertical → follow-through,
  with the release notch timed so the hand is at the top of the circle when `ReleaseBall()` fires.
- `MoveSpeed` (cm/s) added and driven by the game mode so **stride frequency matches distance
  covered** (`Cadence = clamp(MoveSpeed/48, 8, 17)`) — no more foot skating.

### `Source/CRICKETGAME/SuperOver/C26CameraDirector.h` / `.cpp` — broadcast direction rewritten
- **Found dead code:** `MarkContact`, `MarkOutcome` and `SetFieldingTarget` existed but were
  **never called by anything**. So contact-anchored replays, the wicket camera and fielder-aware
  tracking had never once run. All three are now wired from `C26MatchGameMode`.
- `MarkContact` now takes the contact position; `SetFieldingTarget` takes a `RunnersActive` flag.
- Batting lens: 4.3 m high, ~15.5 m behind the striker, 37 deg with a slow push-in through the
  run-up and a small extra push at delivery.
- In-play is a real broadcast cut sequence: hold on the striker through contact (0.34 s, with a
  decaying impulse/shake) → pan from a fixed main tower → then either a square running camera, a
  boundary camera outside the ball's radial line, or a ball-local chase (low for driven, lifted for
  lofted).
- Replay is a **three-shot cut**: tight side-on on the stroke → behind the bowler's arm (or a low
  stump-height angle for a wicket) → wide of the result. Slow-motion ramps around contact
  (0.28-0.38x) and recovers to 0.9x. `ReplaySpeed()` is exposed and the HUD badge now prints the
  real rate instead of a hard-coded "0.65x".
- `Look()` gained a per-shot `TrackRate`, a single decaying shake (not continuous noise), and a
  camera-safety envelope that only clamps to the enclosure below roof height so establishing shots
  can sit outside the bowl.
- New intro: aerial arrival over the roof → pylon look-up tilting down to the square → pitch beauty
  pass → bowler at the mark.

### `Source/CRICKETGAME/SuperOver/C26MatchGameMode.cpp` — wiring only, no rules changes
- Calls `Director->MarkContact(...)` at the moment of bat-ball contact.
- Calls `Director->MarkOutcome(Callout, Focus)` in `Resolve()`, with the focus chosen per dismissal
  type (stumps for bowled, throw end for run out, fielder for a catch, ball otherwise).
- Calls `Director->SetFieldingTarget(Intercept, ActiveFielder>=0, Running)` each field decision and
  clears it in `PrepareDelivery()`.
- Sets `MoveSpeed` on the bowler during the run-up, on the active fielder, and on both runners.

### `Source/CRICKETGAME/SuperOver/C26HUD.cpp`
- Replay badge shows the live playback rate; added a coral accent bar. Includes `C26CameraDirector.h`.

**Not yet touched this session:** `C26Simulation.*`, `Core/C26Rules.h`, `C26PlayerController.*`,
`C26Audio.*`, `C26Settings.*`, `Tests/C26Automation.cpp`, all Python tools, all `.uasset` content,
the map. Rules and scoring are untouched and remain authoritative.

---

## 3. WHAT MUST NOT BE REBUILT

- `Source/CRICKETGAME/SuperOver/Core/C26Rules.h` — engine-independent authoritative rules with
  epoch/delivery-ID guarded commits. It is good. Presentation may only read it. Do not let a
  boundary trigger, an anim notify and a fielding trigger each add score.
- `C26Simulation` — 240 Hz substepped ball with analytic bounce timing, swing/seam, segment-based
  rope crossing, and a fully geometric contact model (`FC26Contact`, `EC26ContactType`) with no RNG
  in the outcome. Extend, don't replace.
- The whole match state machine in `C26MatchGameMode` (`EC26Phase`), innings switching, run-outs,
  Play Again reset, `-C26Smoke` and `-C26Shots` harnesses.
- The HUD's visual identity (dark broadcast panels, cyan accent, ball-by-ball ledger) — refine only.
- All four Epic template variants under `Source/CRICKETGAME/Variant_*` and `Content/Variant_*`.

---

## 4. EXACT NEXT TASKS, IN ORDER

1. **Fix BUG C (blue ground).** Use the isolation tests in §1. Nothing else matters until the field
   reads as grass — it is half of every gameplay frame.
2. Re-capture and re-check `06_ready_batting`, `07_runup_batting`, `08_delivery_batting`,
   `09_in_play`, `12_replay`, `04_intro_flyover`. Confirm the athlete rewrite landed: batter should
   now be side-on with a visible bat, correctly-sized helmet with a grille, pads and gloves.
3. Tune exposure once the ground is correct. Target: grass around sRGB 80-95, pitch clearly lighter
   but never white, crowd darker than the field, athletes the brightest readable elements.
   `AutoExposureBias` is in `AC26Stadium` constructor. **The sign of this parameter was never
   confirmed this session** — change it by ±2 and capture to learn which way is darker before
   trusting it.
4. Re-enable the four pylon spot floods at quality 3 and calibrate `FloodCandelas` (start 26000,
   one capture per change).
5. Verify no regression: run `Tools/Capture.sh`, then `-C26Smoke` 10 matches, then the two
   automation tests. Then **commit** — nothing is committed yet.
   Suggested first commit: `feat(venue): rebuild Eclipse Oval geometry, lighting and materials`,
   then `feat(athlete): correct rig axes and rebuild cricket posing`,
   then `feat(camera): wire broadcast contact/outcome direction and replay cuts`.
6. Then continue the brief's priority order: bat-ball impact VFX/audio/hit-stop, fielding AI
   (only the interceptor should chase; add a backup), running between wickets polish, HUD safe
   areas, mobile quality tiers and profiling.

---

## 5. USEFUL SNIPPET — sampling pixels from a capture (no PIL needed)

```python
import struct, zlib
def load(path):
    d=open(path,'rb').read(); i=8; idat=b''; w=h=bd=ct=0
    while i<len(d):
        ln=struct.unpack('>I',d[i:i+4])[0]; typ=d[i+4:i+8]; data=d[i+8:i+8+ln]; i+=12+ln
        if typ==b'IHDR': w,h,bd,ct=struct.unpack('>IIBB',data[:10])
        elif typ==b'IDAT': idat+=data
        elif typ==b'IEND': break
    raw=zlib.decompress(idat); ch={0:1,2:3,3:1,4:2,6:4}[ct]; bpp=ch*(bd//8); stride=w*bpp
    out=bytearray(); prev=bytearray(stride); p=0
    for y in range(h):
        f=raw[p]; p+=1; line=bytearray(raw[p:p+stride]); p+=stride
        for x in range(stride):
            a=line[x-bpp] if x>=bpp else 0; b=prev[x]; c=prev[x-bpp] if x>=bpp else 0
            if f==1: line[x]=(line[x]+a)&255
            elif f==2: line[x]=(line[x]+b)&255
            elif f==3: line[x]=(line[x]+((a+b)>>1))&255
            elif f==4:
                pa=abs(b-c); pb=abs(a-c); pc=abs(a+b-2*c)
                pr=a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[x]=(line[x]+pr)&255
        out+=line; prev=line
    return w,h,ch,bytes(out)
```

---

## 6. CORRECTIONS TO EARLIER SESSIONS

- The previous handoff claimed a "transformation milestone complete" with camera, athlete and venue
  work done. That work existed in code but **three of its camera entry points were never called**,
  and the venue's outfield and pitch were rendering the **default grey material**, not the authored
  ones. Treat any earlier "verified" visual claim as unverified until a capture confirms it.
- The earlier note that athletes were fixed to true scale is correct (0.48x reference-pose scale,
  ~182 cm), but the note did not catch that the rig's facing axis disagreed with the pose code.
- "M_Crowd failed to compile on Metal (Sine input never connects via the Python API)" was the first
  instance of BUG B. It is a **class** of failure affecting every `textured=True` material, not a
  one-off.

---

## 7. GENUINE EXTERNAL LIMITATIONS (do not use these as an excuse for anything else)

Cricket-specific mocap, a bespoke licensed athlete/kit model, and recorded contact/crowd audio are
not available in this project. Everything else — scale, camera, lighting, materials, procedural
animation, venue geometry, VFX, UI — is solvable in-engine and should be.

---
---

# Historical prior-session log (superseded above; retained for context)

Last updated: 2026-09-08.

## User goal and constraints
Build and directly validate one polished single-player mobile Super Over in this Unreal project. No
broader game modes. Preserve user work, use installed UE 5.8.2, automate technical tasks. Do not
call incomplete work finished.

## Workspace
- Enclosing Git repository: `/Users/aagamjain/Desktop/CRICKET-26-GAME`, branch `main`.
- Logs: `/Users/aagamjain/Library/Logs/Unreal Engine/CRICKETGAMEEditor/`.

## Completed and verified in earlier sessions
- Baseline editor build succeeded. Original Epic mannequin/locomotion content and all four template
  variants preserved.
- Editor Python, editor scripting and procedural mesh plugins enabled; SlateCore,
  ProceduralMeshComponent and DeveloperSettings module dependencies added.
- Rules implemented in `Source/CRICKETGAME/SuperOver/Core/C26Rules.h`. Standalone
  `Tests/RulesTests.cpp` passes 19 scenarios including 10 repeated restart cycles.
- `Tools/BuildContent.py` imports the user-owned Mixamo Remy humanoid and four animations,
  synthesized sounds, generates materials and the primary map. FBX metadata confirms Adobe Mixamo
  provenance. HamzaKhan content is not used.
- Added C26Simulation (240 Hz substeps, bounce, swing, seam, geometric rope crossing, contextual
  contact), tactical AI and field-position data.
- Added C26Athlete, continuous C26Stadium, camera/replay, pooled audio and saved settings.
- Added C26MatchGameMode coordinating both innings, fielding, running, throws, score commits,
  presentation and reset; C26PlayerController touch/mouse/keyboard and C26HUD broadcast/menu
  drawing.
- Default map `/Game/Cricket26/Maps/L_SuperOver` uses native `C26MatchGameMode` and `C26Stadium`.
  There is no C26 Blueprint gameplay wrapper; the base GameState is retained.
- Mobile device/scalability profiles added; Lumen/ray tracing/Nanite dependencies removed.
- Unreal automation: `Cricket26.Rules.SuperOver` and `Cricket26.Simulation.Trajectories` both PASS.
- `-C26Smoke`: ten complete matches passed, no runtime errors, 14 athlete actors stable.

## Earlier visual-repair pass (2026-09-08)
Fixed: gameplay cameras occluded by keeper/bowler on the lens axis; athletes at 2x scale; HUD score
overlap; invisible ball at broadcast distance; capture beats timing out; helmets buried in hair;
result camera inside athletes. `Tools/FixCrowd.py` was applied once to `M_Crowd.uasset`.
Note: several of that pass's conclusions about *why* the venue looked flat were wrong — see §1.
