# PREMIUM CHARACTER SYSTEM — CRICKET 26

Status: 2026-09-13. This is the honest state of the character/motion overhaul: what was broken
and why (root-cause report), what is fixed and verified (and how), what is wired but awaiting the
Mac build/playtest, and what is explicitly not done. Nothing here is claimed on the strength of
"it compiles" — every claim below names where it is verified.

---

## 1. Root-cause report

Four visible symptoms, four distinct mechanisms. Two are presentation-logic bugs (fixed in C++),
two are asset-pipeline bugs in the authored clips (fixed in the offline pipeline).

### 1.1 Why batting and bowling looked awkward (and got worse when the authored clips were wired in)

**The game had no cricket animation at all.** Every pose was solved procedurally in
`AC26Athlete::Animate` — correct positions, but a pose solver cannot author *motion*: there is no
swing arc, no gather, no follow-through, no weight transfer. The authored clips were written to
fix exactly that (`Docs/AUTHORED_ANIMATION.md`), but the first export **could not have worked**,
for four compounding reasons, all measured:

1. **Root mismatch.** The clips' FBX root Model is `Armature`, but the skeleton the game skins to
   has root bone `Armature.001`. The armature node carries `T(0,5.78,0) R(-90,0,0) S=100`, and UE
   binds raw curve locals verbatim as skeleton locals — the node transform is never applied. Net
   error: rotations off by 90°, translations off by 100×.
2. **Rest-pose mismatch.** The clips were authored on the v2 rig (metres, T-pose); the shipped
   skeleton is v1 (asset units, different local frames — up to ~143° apart per bone). A curve
   value is a *local* rotation; binding it to a different local frame scrambles the pose.
3. **Units.** v1/v2 bone-length ratio is 223.739 (measured on `LeftLeg`). Unconverted, every
   translation is ~238× too big after the missing ×100 node scale.
4. **Rig-facing inversion** (the deep one). The authoring script's header claimed the rig faces
   **+Y** in armature space. Measured, the shipped rig faces **−Y** (toe vector `(2.7, −21.4,
   +24.3)`). Every keyframe was authored under the wrong assumption, so the entire action arc —
   backlift, contact, the front-foot stride, the bowling release — happened on the character's
   **back**. Measured on the first corrected attempt: backlift hands +73.6 cm and contact hands
   −61 cm along the body's facing axis — the batter drove the ball through his own spine, and the
   bowler released behind his head. This is why the clips were disabled in
   `AC26Athlete::Animate` and the procedural solve kept ownership (see the DISABLED comments,
   now replaced by re-enable comments).

### 1.2 Why players glided

Movement changed pose instantly: each `Animate` branch answered "where is this body *right now*"
and the athlete was snapped onto that answer every frame — ready→running, running→gather,
gather→throw — a one-frame jump each way, six times an over per fielder, and while *within* a
state the limbs were re-solved to static poses with no motion through them. The fix already in
the code: `SmoothPose(Dt, ...)` filters every authored pose over time ("Now the body travels
there" — the comment above the final `ApplyLocalPose` call), and `ApplyRecordedMotion()` layers
the recorded run/idle cycles for locomotion.

### 1.3 Why fielders wore batting pads

Equipment was not role-gated: the kit components (pads, helmet, bat) were attached to every
athlete, so all eleven fielders stood in full batting kit. The fix already in the code:
`AC26Athlete::ApplyVisualRole()` + `ApplyDetail()` are the single equipment authority —
batters alone carry a bat; batters and keepers wear pads/headwear; bowlers and fielders show
team clothing and shoes; the grill and gloves fade out by distance tier (LOD, below). Switching
roles re-runs Configure, so no stale kit survives an innings change.

### 1.4 Why lower bodies were missing / half-body players

Two contributors. (a) The animated kit's garment meshes are prototype-grade (`Tools/ImportKit.py`
built them; torso coverage is a known open item — the body is `SK_Cricketer_KitBase`-derived and
the lower-body garments do not yet match the reference quality). (b) The authored-clip binding
errors in 1.1 contorted the athletes when the clips were enabled, which read as broken bodies.
(a) is still open — see §5. The hero-scan path (full-body static meshes) was a documented dead
end for animation and is kept only as an optional visual.

---

## 2. The facing repair (the fix that unlocked the clips)

No rigid transform or mirror of the *solved* poses can repair 1.1.4 — the torso needs identity
while the limbs need a Z-flip, so every candidate transform breaks one or the other (proven by
exhaustive case analysis). The repair is applied to the *keys* at solve time, in
`repair_facing()` in `ArtSource/Blender/Animation/c26_anim_author.py` (single source of truth,
used by both the Blender path and the offline path):

- Spine spec quaternions conjugate by rotZ(180°): turns/chest twists keep their body-relative
  meaning, leans and y-rotations negate (the imagined forward is the actual backward).
- IK targets, shoulder-relative offsets and hips offsets rotate by `(x,y,z) → (−x,−y,z)`:
  the arc, stride and weight transfer move to the front; left/right swap exactly as a proper
  rotation swaps them, so a right-hander stays a right-hander.
- IK poles keep x and z, negate y: pole x was iterated against real renders (elbow OUTWARD);
  pole y was authored under the imagined convention.

Result (measured on the corrected clips): a coherent right-handed batter facing the bowler with
backlift **12 cm behind** the hips and contact **81 cm in front** of them, and a bowler who
travels **71 cm down the pitch** and releases **71 cm above** the head origin.

---

## 3. The pipeline (offline, no Blender required)

```
c26_anim_author.py            keyframes + IK solve + repair_facing  (single source of truth)
  |
  +- Blender:    Blender --background C26_KitBase_v002.blend --python c26_anim_author.py
  |              -> ArtSource/Exports/Animations/*.fbx         (v2 rig frame)
  |
  +- Offline:    python3 Tools/rebuild_authored_clips.py       -> Animations/Solved/*.fbx
                 python3 Tools/correct_authored_anim.py <solved files>
                                                               -> Animations/Corrected/*.fbx
                 Tools/ImportAnimations.py (UE)                -> /Game/Cricket26/Animations
```

- `Tools/rebuild_authored_clips.py` imports the authoring module through a mathutils shim
  (`Tools/c26_mathutils_shim.py`), reconstructs the TRUE v2 T-pose rest from the shipped asset
  rig (`C26_KitBase_v001.fbx` ÷ 223.739, re-based into the armature frame — the anim FBX's own
  Model defaults hold the frame-1 pose, not the rest), re-solves every key with the repair, and
  bakes every frame at 24 fps. It reproduces the original Blender solve bit-exactly (validated:
  frame-1 Hips rotation dot product = 1.000000 against the shipped file's curves).
- `Tools/correct_authored_anim.py` retargets onto the shipped skeleton: world-relative
  orientation copy per bone, hips delta at the measured rig ratio (2.23739), per-frame ground
  pin, root rename to `Armature.001`, v1 Lcl defaults. It **exits non-zero if any check fails**.

### Verification (offline, all PASS)

| Clip | Check | Result |
|---|---|---|
| BattingDrive | stance ankles at ground | 24.7 / 29.3 cm |
| | stance hips (athletic crouch) | 193 cm |
| | hands together on handle (stance / contact) | 17.9 / 18.5 cm |
| | **contact hands in FRONT of hips** | **+81.2 cm** |
| | **backlift hands behind hips** | **−12.1 cm** |
| BowlingPace | ankles at ground | 25.0 / 24.7 cm |
| | hips | 196 cm |
| | mark: ball in both hands | 2.7 cm span |
| | release arm above head | 398 vs 327 cm |
| | travel down the pitch | 71.5 cm |

### In-engine gate

`Cricket26.Anim.AuthoredClips` (in `Tests/C26ProductionTests.cpp`) loads the **imported**
AnimSequences and re-asserts the same facts through `GetBoneTransform` — i.e. after the FBX
importer has done its own conversion. If it fails with an asset error, run
`Tools/ImportAnimations.py`; if it fails a geometry check, the pipeline was bypassed.

---

## 4. What is wired in C++ (and the safety rails)

- `AC26Athlete::Animate` applies both clips through `ApplyAuthoredClip` (re-enabled 2026-09-13):
  delta-from-clip-rest retarget, weight-blended, with the two-phase time warp pinning the
  clips' defining frames to the match's own timing authority (`BatContactPoseTime`,
  `ReleasePoseTime`). Full clip authority through the stroke, short ramps at entry/exit.
- **Never-T-pose fallback:** if a clip fails to load, the branches guard on the asset pointer
  and the procedural solve keeps ownership; `GatherDrivenBones` falls back to driving all bones
  if a clip cannot be interrogated.
- Gameplay is untouched: the clips only *represent* the match's decisions. Scoring, simulation,
  bowling control loop, pull/release batting, fielding decisions, match state, super over,
  commentary, toss — all unchanged (automation suites still green on the same code paths).
- LOD: grill (hero tier only), gloves (not distant), pads/shoes/body at every tier; equipment
  LOD strategy documented in `ApplyDetail` comments.

---

## 5. What is NOT done (honest list)

1. **Mac verification of this session's changes.** The corrected FBX pass 11/11 offline checks,
   but the UE import + build + in-match playtest of the *corrected* clips has not run yet
   (sandbox has no UE). The exact sequence is in §6.
2. **Shot library breadth.** Only the drive and pace-bowling clips are authored. Pull, hook,
   sweep, leg glance, cuts, defence, off-spin/leg-spin actions, keeper dive/stump, umpire
   signals, fielder pickup/throw/catch/dive are not authored. The offline pipeline makes each
   one a key-list addition to `c26_anim_author.py` — no Blender round-trip needed.
3. **Garment mesh quality.** Torso prototype-grade meshes remain the biggest visual gap for
   "premium" (open item since before this session; never overwrite `SK_Cricketer_KitBase`).
4. **Equipment LOD meshes** (tiers exist, mesh swaps not).
5. **Mobile perf profiling** not started.
6. **Bowling gather** (pre-release 0.31 s) is still procedural; the clip covers the action from
   the run-up mark, and the gather blends procedurally at the entry ramp.

---

## 6. How to verify on the Mac (exact sequence)

```bash
cd ~/Desktop/CRICKET-26-GAME/CRICKETGAME

# 1. Re-run the offline pipeline (proves the FBX on disk, ~2 s):
python3 Tools/rebuild_authored_clips.py
python3 Tools/correct_authored_anim.py \
    ArtSource/Exports/Animations/Solved/A_C26_BattingDrive.fbx \
    ArtSource/Exports/Animations/Solved/A_C26_BowlingPace.fbx
#    -> all 11 checks must print PASS

# 2. Import the corrected clips into UE:
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/RunUAT.sh" -ScriptsForProject=... # or:
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd.app/Contents/MacOS/UnrealEditor-Cmd" \
    "$PWD/CRICKETGAME.uproject" -run=pythonscript -script="$PWD/Tools/ImportAnimations.py"

# 3. Build:
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
    CRICKETGAMEEditor Mac Development "$PWD/CRICKETGAME.uproject" -WaitMutex -NoHotReloadFromIDE

# 4. Automation gate (includes the new Cricket26.Anim.AuthoredClips):
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd.app/Contents/MacOS/UnrealEditor-Cmd" \
    "$PWD/CRICKETGAME.uproject" -ExecCmds="Automation RunTests Cricket26;Quit" -unattended -nopause

# 5. Playtest in an actual match: BatLab / BowlLab maps, then a full match:
#    - striker's drive: backlift behind the body, contact in front of the front foot
#    - bowler: overhead arm, ball leaves the hand at the release frame, runs in toward the batter
#    - fielders: team clothing, no pads; keeper padded; umpire unpadded
#    - no T-pose at any moment, no gliding on role changes
```

If step 5 shows anything wrong, capture which frame/role and compare against the offline
verifier's numbers before touching C++ — the pipeline is the more likely culprit, and it is
safe to iterate on it without a rebuild.

---

## 7. File map (what changed in this session)

| File | Change |
|---|---|
| `ArtSource/Blender/Animation/c26_anim_author.py` | facing header corrected; `repair_facing()` added and applied in `build_action`; mark/gather keys hold the ball in both hands |
| `Tools/c26_mathutils_shim.py` | NEW — pure-Python Vector/Quaternion/Matrix for importing the authoring module outside Blender |
| `Tools/rebuild_authored_clips.py` | NEW — offline solve + bake, writes `Animations/Solved/` |
| `Tools/correct_authored_anim.py` | path args, extra checks (backlift behind, bowler travel), dst basename fix |
| `ArtSource/Exports/Animations/Solved/*.fbx` | NEW — re-solved clips (v2 frame) |
| `ArtSource/Exports/Animations/Corrected/*.fbx` | REGENERATED — both clips, 11/11 checks |
| `Tools/ImportAnimations.py` | imports from `Corrected/`, pipeline documented |
| `Source/.../C26Athlete.cpp` | both `ApplyAuthoredClip` branches re-enabled with history |
| `Source/.../Tests/C26ProductionTests.cpp` | NEW `Cricket26.Anim.AuthoredClips` in-engine gate |
| `Docs/AUTHORED_ANIMATION.md` | facing correction documented; pipeline + verification sections; stale TODOs rewritten |
| `Docs/PREMIUM_CHARACTER_SYSTEM.md` | this report |
