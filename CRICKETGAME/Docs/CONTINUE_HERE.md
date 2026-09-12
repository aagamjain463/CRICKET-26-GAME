# CONTINUE HERE — CRICKET 26 player + motion upgrade

**Read this file first in a new chat.** It is self-contained: task, state, next steps, commands,
and the traps that already cost time. Written 2026-09-11 by a WorkBuddy session that ran out of
context.

---

## 0. UPDATE 2026-09-13 — authored clips FIXED and re-enabled (read PREMIUM_CHARACTER_SYSTEM.md)

The authored-clips blocker is resolved AND the shot library is in. Root cause of the
"stroke through the back": the authoring script assumed the rig faces armature +Y; it
faces -Y. The repair (`repair_facing()` in `c26_anim_author.py` — conjugate spine
twists by rotZ(180), rotate IK targets by (x,y,z)->(-x,-y,z), poles keep x/z negate y)
is applied at solve time and by the offline pipeline:

    python3 Tools/rebuild_authored_clips.py        # solve+bake all 8 clips -> Solved/
    python3 Tools/correct_authored_anim.py         # -> Corrected/, 66/66 checks PASS

Library (all verified): BattingDrive/Pull/Cut/Sweep/Defence + BowlingPace/OffSpin/
LegSpin. C26Athlete selects per shot intent (Defending/ShotAngle/StrideIntent) and
delivery type (DeliveryStyle), with fallback chain library -> base clip ->
procedural. A SECOND corrector bug was found and fixed while authoring the cut: the
hips retarget referenced the FBX Model defaults (a stale frozen POSE) instead of the
true T-pose, rotating every corrected clip by ~52 deg of chest yaw — chest-yaw gates
now exist in both the offline checks and the in-engine test
(Cricket26.Anim.AuthoredClips covers all 8 clips).

**Not yet done: the UE import + build + playtest on this Mac** — exact sequence in
`Docs/PREMIUM_CHARACTER_SYSTEM.md` §6. Everything else in this file below is older
context (meta-human paths etc.) and still applies.

---

## 1. The task

The user's request, verbatim:

> "BRO I WANT TO CHANGE THE PLAYERS IN MY GAME INTO PREMIUM REALISTIC ONES AND MAKE THEIR MOVEMENT
> AND MOTION SMOOTH AND REALISTIC. DO ONLY THIS. TAKE INSPIRATION FROM VIDEO ATTACHED"

Reference video: `~/Downloads/vidssave.com Cricket 24 PS5 Gameplay _ India Vs Australia Dynamic
Broadcast Camera 4K60 HDR 360P (1).mp4` — Cricket 24 on PS5. Photoreal licensed players, TV
broadcast camera, mocap-grade motion. That is the *quality bar*, not something that can be copied
(licensed faces/kits/mocap).

**"DO ONLY THIS"** means: do not expand into stadium, crowd, UI, audio, or shot libraries. Players
and motion only.

When offered a choice, the user picked the two heaviest routes:

- **Players → MetaHuman photoreal.** Not the existing hero scans.
- **Motion → author real cricket clips.** Not refining the procedural poser.

---

## 2. Where the game is

| | |
|---|---|
| Project root | `/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME` |
| Engine | Unreal Engine **5.8.2** (`/Users/Shared/Epic Games/UE_5.8`) |
| Branch | `feature/metahuman-presentation-swap` |
| Repo root | `/Users/aagamjain/Desktop/CRICKET-26-GAME` (git) |
| Build | Green. `Result: Succeeded` |
| DCCs installed | Blender **5.2.1** (`/Applications/Blender.app`), Cascadeur |

**Ignore `/Users/aagamjain/Documents/Unreal Projects/CRICKETGAME`** — that is an untouched stock
template copy. The live project is the Desktop one.

The project has its own conventions. Read before changing anything:

- `Docs/AI_HANDOFF.md` — the running handoff log, newest entry at the top
- `Docs/CURRENT_TASK.md` — what the project considers next
- `Docs/TRANSFORMATION_AUDIT.md` — KEEP / IMPROVE / REPLACE / DEFER, and the asset-blocker list
- `Docs/VISUAL_QUALITY_LOG.md` — scored before/after with evidence
- `Docs/GAME_ARCHITECTURE.md` — file ownership

Build command (used constantly):

```bash
cd /Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  CRICKETGAMEEditor Mac Development "$PWD/CRICKETGAME.uproject" \
  -WaitMutex -NoHotReloadFromIDE 2>&1 | tail -30
```

---

## 3. State of the players (the honest version)

Every athlete in the live match renders as **one shared stylized skinned mesh**,
`SK_Cricketer_Match`, posed entirely by C++ in `AC26Athlete::Animate()`, with bat/helmet/pads/
gloves/shoes attached as separate static meshes. It reads as a chunky mannequin, not a cricketer.
Baseline captures: `Artifacts/Captures/resume_2012/`.

Three better player assets exist on disk and are **all unused**:

| Asset | Fidelity | Why it is not live |
|---|---|---|
| `SM_C26_Player_*` (10) | High, 4K PBR | Static meshes — they physically cannot animate |
| `SK_Cricketer_Hero*` (10) | High, 30–45 MB Blender sources | Each imported onto its **own** skeleton; Interchange refused to merge bone trees with `SK_Cricketer_KitBase`, so the C++ poser cannot drive them |
| `MH_C26_Player_001` | Photoreal | Auto-rig needs Epic's cloud + an authenticated account |

The hero static-mesh path was tried and deliberately reverted — see the comment at
`C26Athlete.cpp` ~line 553: *"The generated role meshes have incompatible bind poses and baked
equipment."* Do not re-attempt it without solving the bind-pose problem first.

---

## 4. State of the motion

All batting and bowling is procedural `UPoseableMeshComponent` posing driven by authored pose keys
in `C26Motion.h`. `Docs/TRANSFORMATION_AUDIT.md` lists it plainly:

> **ASSET BLOCKER: PREMIUM STRAIGHT DRIVE**
> **ASSET BLOCKER: PREMIUM FAST BOWLING ACTION**

There were no animation assets. `Content/Cricket26/Animations/` holds only `A_Idle`, `A_Run`,
`A_Catch`, `A_FielderThrow`, and none of them drives the current poses.

---

## 5. What this session did

Authored the project's **first real cricket animation clips** on the production rig.

| Clip | Frames @24 fps | Length | Content |
|---|---:|---:|---|
| `A_C26_BattingDrive` | 1–36 | 1.50 s | stance → trigger → top of backlift → downswing → **contact @ f23** → follow-through → recover |
| `A_C26_BowlingPace` | 1–46 | 1.92 s | mark → gather → bound → back-foot landing → front-foot brace → **release @ f31** → follow-through → recover |

New files:

```
ArtSource/Blender/Animation/c26_anim_author.py    authoring — the source of truth
ArtSource/Blender/Animation/dump_pose.py          numeric pose verification
ArtSource/Blender/Animation/render_rig_proxy.py   stick-figure contact sheets
ArtSource/Blender/Animation/render_anim_sheet.py  full-mesh contact sheets
ArtSource/Blender/Animation/export_anim_fbx.py    FBX export
ArtSource/Exports/Animations/A_C26_*.fbx          exported clips
Tools/ImportAnimations.py                         UE import (animation-only FBX)
Docs/AUTHORED_ANIMATION.md                        full technical write-up
```

Rebuild the clips:

```bash
cd /Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Blender/Characters
"/Applications/Blender.app/Contents/MacOS/Blender" --background C26_KitBase_v002.blend \
  --python ../Animation/c26_anim_author.py -- --out /tmp/c26anim/out
```

**Verified numerically, not by eye** (via `dump_pose.py`):

- Both hands stay on the bat handle — **5.7 cm apart** at every key of the drive
- Hand path: hip height → backlift high *and behind* (`y −0.20`) → contact *in front of the front
  foot* (`y +0.30`) → follow-through up (`z 1.22`)
- Front foot strides 20 cm down the pitch; hips travel forward `y +0.01 → +0.14` (weight transfer)
- Head stays 0.51–0.555 m above the hips throughout
- Bowling arm traces a real circle: back/low → high behind (`z 1.56`) → top (`z 1.68`) → over →
  down past the hip. Front foot plants at the crease (`y 0.64`) and stays planted while the hips
  travel over it to `y 0.35`

---

## 6. Next steps, in order

### Step 1 — confirm the import landed

`Tools/ImportAnimations.py` was run headless. Confirm two `AnimSequence` assets exist at
`/Game/Cricket26/Animations/A_C26_BattingDrive` and `A_C26_BowlingPace`, bound to the **same
skeleton as `SK_Cricketer_Match`**. The script logs `C26_ANIM imported=... bound=True`. If
`bound=False`, the bind is wrong and the clip will never play — fix before going further.

### Step 2 — wire the clips into `AC26Athlete`

**`AC26Athlete::ApplyRecordedMotion()` is dead code — nothing calls it.** Grep confirms it. It also
only handles run/idle, not a batting stroke or a bowling action. Read it first
(`Source/CRICKETGAME/SuperOver/C26Athlete.cpp`); it already shows the correct pattern for
sampling a clip into the poser:

```cpp
const FAnimExtractContext Context(Time, false);
Clip->GetBoneTransform(Sample, FSkeletonPoseBoneIndex(J), Context, false);
// then retarget rotations onto Reference[] and blend into Pose[]
```

Critical constraint: **`UPoseableMeshComponent` has no `PlayAnimation`.** It derives from
`USkinnedMeshComponent`, not `USkeletalMeshComponent`, so an AnimInstance never runs. The clips
must be sampled per frame through `UAnimSequence::GetBoneTransform()` into `Pose`, exactly as
`ApplyRecordedMotion` already does.

### Step 3 — do not let the clip own the timing

`C26MatchGameMode` is the sole authority on release and contact. The clip must be *driven* by those
instants, not the other way round — same rule the audio director already follows. Sync points:
batting contact is **frame 23 of 36 (0.958 s)**, bowling release is **frame 31 of 46 (1.292 s)**.

### Step 4 — verify in captures, not by assertion

Run the capture harness and actually look at the PNGs. The baseline to beat is
`Artifacts/Captures/resume_2012/`. Report measured before/after.

---

## 7. The MetaHuman half — blocked on the user, one step

`MH_C26_Player_001.uasset` exists with a sculpted athletic body (182 cm, 47 cm shoulders — see
`Tools/MetaHumanBuildPlayer001.py`) but has **no skeleton and no skinned mesh**. There is no `Body/`
folder next to it.

Auto-rigging calls Epic's cloud service (`mh-uemhc-autorig-service`) and needs an authenticated
Epic account **with MetaHuman entitlement**. The previous session hit `Server Error` / 300 s
timeouts, and `USE_FULL_RIG = False` in the build script is a workaround for that, not a design
choice.

**The entire unblock is one manual sign-in:**

1. Open the project in the editor.
2. **Edit → Editor Preferences → General → Accounts** → sign into an Epic account.
3. Re-run `Tools/MetaHumanBuildPlayer001.py` — the script needs **no changes**.

```bash
cd /Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/CRICKETGAME.uproject" \
  -run=pythonscript -script="$PWD/Tools/MetaHumanBuildPlayer001.py" -unattended -nosplash -stdout
```

Note: the build step is set to `pipeline_quality = LOW` because MEDIUM's material bake ran 45+
minutes without finishing on this 16 GB Mac. Raise it only when validating the final look.

After the MetaHuman is built, the authored clips must be **retargeted** onto the MetaHuman skeleton
(UE's IK Retargeter) — they were authored on the `mixamorig:` 67-bone rig.

---

## 8. Traps already paid for — do not rediscover

**Blender / authoring**

1. **Solve limbs with IK, never by guessing joint angles.** The first version set angles directly
   and put the batter's hands **0.83 m apart**. Hands on the bat handle is the entire point of a
   batting animation. Drive limbs by hand/foot *targets*; keep the spine chain as rotations.
2. **Rotate a limb from its INHERITED direction, not its rest direction.** The source direction
   must be `base_quaternion @ (0,1,0)` — where the bone points *after* its parents are posed. Using
   the rest direction silently bakes in the spine chain's rotation and the limb lands ~40 cm off.
3. **A two-bone IK writes both the upper and lower bone.** The hierarchy walk then reaches the
   lower bone and overwrites it with its un-IK'd base. Park IK results in a dict and consume them
   when the loop arrives.
4. `C26_KitBase_v002.blend` keeps its objects in a collection **not linked into the scene** —
   `view_layer.objects` is empty on open. Link them before rendering or selecting, or you get bare
   background every time.
5. **Workbench and EEVEE render empty under `--background` on macOS** (no GPU context). Use Cycles.
6. `Armature` sits at `z = 0.0578`, so world-space Z read-outs carry a constant **+5.8 cm** against
   armature-space targets. Subtract before concluding the IK missed.
7. The body mesh in `C26_KitBase_v002.blend` is **not usable for motion review** — no leg geometry,
   long spikes off unweighted vertices. Use the stick-figure proxy renderer.
8. Blender 5.x moved fcurves behind the **slotted-action API** (`action.layers → strips →
   channelbags`). `Action.fcurves` raises `AttributeError`. There is a helper for this.
9. To read frames out of an `.mp4` on this machine: **no ffmpeg, no pyobjc**. Use Swift +
   AVFoundation (`swiftc` is available). `qlmanage` fails with a sandbox error.

**Unreal**

10. **`ApplyRecordedMotion()` is dead code.** Do not assume a clip path already works because the
    function exists.
11. `UPoseableMeshComponent` has no `PlayAnimation` — sample clips manually.
12. The project's C++ strips `mixamorig:` / `mixamorig_` from bone names
    (`AC26Athlete::RebuildReference`), which is why the code says `Hips` while the rig says
    `mixamorig:Hips`.
13. `RebuildReference()` applies a **0.48 ground scale** when the bound mesh is taller than 250
    units, because legacy Mixamo rigs import at ~340–380 cm. Anything that assumes real-world
    centimetres without accounting for this will be wrong by 2×.

**Rig convention** (`C26_KitBase_v002.blend` → `Armature`, 67 bones):

```
+Y = the character's FORWARD (toes point +Y)
+X = the character's LEFT   (LeftArm tail is at +X)
+Z = up
Rest pose is a T-pose: arms straight along +/-X, legs straight down.
```

---

## 9. Working agreements this project follows

- **Build green before and after.** Run the build command above.
- **Verify with captures, not assertions.** Look at the PNGs.
- **Never overwrite** `SK_Cricketer_KitBase` — `Tools/ImportKit.py` validates against it.
- **Fail-safe imports**: a bad import must leave the base rig and every previously-imported asset
  untouched so it can never take the match down.
- **Update `Docs/AI_HANDOFF.md`** with a new dated section at the top when you finish.
- Report measured numbers. This project's docs are unusually honest about what is *not* verified —
  keep that standard.

---

## 10. Blunt status summary

| Item | State |
|---|---|
| Authored batting clip | **Done**, verified numerically, exported |
| Authored bowling clip | **Done**, verified numerically, exported |
| Clips imported into UE | Run this session — confirm it landed (Step 1) |
| Clips wired into the match | **Not done** — Step 2 |
| Motion visible in-game | **Not done** |
| MetaHuman player | **Blocked** on the user's Epic sign-in |
| "Premium realistic players" | **Not achieved.** Photoreal is blocked; the fallback hero meshes have an unsolved bind-pose problem |
| "Cricket 24 quality" | **Not achieved and not achievable here.** No mocap, no licensed assets |

Be honest with the user about the last two rows. The clips are a competent, structurally correct
first authored pass that is hand-editable in Blender from here — which the procedural poser never
was. That is real progress, but it is not Cricket 24.
