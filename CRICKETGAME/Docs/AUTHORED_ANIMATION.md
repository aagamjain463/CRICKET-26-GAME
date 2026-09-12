# CRICKET 26 — Authored Cricket Animation

Status: **clips authored and exported, NOT yet imported or wired into the match.**
Branch: `feature/metahuman-presentation-swap`. Nothing in this document changes gameplay.

## Why this exists

Every cricket action in the game is posed procedurally in C++ (`AC26Athlete::Animate`).
`Docs/TRANSFORMATION_AUDIT.md` records the consequence honestly: *"Cricket action source: all
batting/bowling is procedural UPoseableMeshComponent posing... **ASSET BLOCKER: PREMIUM STRAIGHT
DRIVE**, **ASSET BLOCKER: PREMIUM FAST BOWLING ACTION**."* There was no animation asset for a
batting stroke or a bowling action anywhere in the project. This is the first one.

## What was built

Two real keyframed clips, authored on the exact rig the game already skins to — no retargeting:

| Clip | Frames @24fps | Length | Content |
|---|---:|---:|---|
| `A_C26_BattingDrive` | 1–36 | 1.50 s | stance → trigger → top of backlift → downswing → contact → follow-through → recover |
| `A_C26_BowlingPace` | 1–46 | 1.92 s | at the mark → gather → bound → back-foot landing → front-foot brace → release → follow-through → recover |

Source, tooling and exports:

```
ArtSource/Blender/Animation/c26_anim_author.py   authoring (the source of truth)
ArtSource/Blender/Animation/dump_pose.py         numeric verification
ArtSource/Blender/Animation/render_rig_proxy.py  stick-figure contact sheets
ArtSource/Blender/Animation/render_anim_sheet.py full-mesh contact sheets
ArtSource/Blender/Animation/export_anim_fbx.py   FBX export
ArtSource/Exports/Animations/A_C26_*.fbx         exported clips
```

Rebuild:

```bash
cd ArtSource/Blender/Characters
"/Applications/Blender.app/Contents/MacOS/Blender" --background C26_KitBase_v002.blend \
  --python ../Animation/c26_anim_author.py -- --out /tmp/c26anim/out
```

## The rig, measured (do not assume this)

`ArtSource/Blender/Characters/C26_KitBase_v002.blend` → `Armature`, **67 bones**, names carry the
`mixamorig:` prefix. `AC26Athlete::RebuildReference()` strips `mixamorig:`/`mixamorig_` before
building its bone map, which is why the C++ refers to `Hips`, `LeftArm`, `RightForeArm` and so on.

```
+Y = the character's FORWARD (toes point +Y)
+X = the character's LEFT   (LeftArm tail is at +X)
+Z = up
Rest pose is a T-pose: arms straight out along +/-X, legs straight down.
```

## Two things that cost real time — do not rediscover them

**1. The pose must be solved with IK, not by guessing joint angles.** The first version of
`c26_anim_author.py` set bone angles directly. Measured with `dump_pose.py`, it put the batter's
two hands **0.83 m apart**. Both hands being on the bat handle is the defining feature of a batting
animation and no amount of angle-guessing reaches it. The script now drives limbs by *where the
hands and feet must be* and solves the joint angles with a two-bone solver; the spine chain is
still authored as rotations because that is how a stance is actually described. Measured result:
hands **5.7 cm apart** on the handle, held for the whole clip.

**2. Rotate from the bone's INHERITED direction, not its rest direction.** Aiming a limb by
rotating its *rest* direction onto a target silently bakes in the whole spine chain's rotation and
the limb lands somewhere else. The correct source direction is `base_quaternion @ (0,1,0)` — where
the bone points *after* its parents are posed. This was the bug that made the first IK attempt
miss by 40 cm.

## Gotchas in the source .blend (all cost a render cycle each)

- **The character's objects live in a collection that is not linked into the scene.**
  `view_layer.objects` is empty on open, so anything that renders or selects must link them first.
  This is why every headless render came back as bare background.
- **Workbench and EEVEE render empty under `--background` on macOS** — no GPU context. Use Cycles.
- `Armature` sits at `z = 0.0578`, so world-space Z read-outs carry a constant +5.8 cm against
  armature-space targets. Subtract it before concluding the IK missed.
- The **body mesh in this file is not usable for motion review**: no leg geometry and long spikes
  off unweighted vertices. Use `render_rig_proxy.py` (stick figure), not a mesh render.

## Verification actually performed

Numeric, via `dump_pose.py` — not by eye:

- Hands stay on the handle for the whole drive (5.7 cm apart at every key).
- Hand path: stance hip-height → backlift high and **behind** (`y −0.20`) → downswing → contact
  **in front of the front foot** (`y +0.30`) → follow-through **up** (`z 1.22`).
- Front foot strides 20 cm forward down the pitch; back foot stays put.
- Hips travel forward `y +0.01 → +0.14` — the weight transfer onto the front foot.
- Head stays `0.51–0.555 m` above the hips throughout — nobody folds over.
- Bowling arm traces a real circle: back and low (`z 1.22`) → high behind (`z 1.56`) → top of the
  circle (`z 1.68`) → over (`z 1.67`) → down past the hip (`z 1.22`).
- Bowling front foot plants at the crease (`y 0.64`) and **stays planted** while the hips travel
  over it to `y 0.35`.

## What is NOT done

1. **Not imported into Unreal.** The FBX are in `ArtSource/Exports/Animations/`; there is no
   `Tools/ImportAnimations.py` yet and no `AnimSequence` asset under `Content/Cricket26/Animations/`.
2. **Not wired into the athlete.** `AC26Athlete::ApplyRecordedMotion()` exists but **nothing calls
   it** — verified by grep; it is dead code. It also only handles run/idle, not a batting stroke or
   a bowling action. Wiring these clips means new code, and `UPoseableMeshComponent` has no
   `PlayAnimation`, so the clips must be sampled through `UAnimSequence::GetBoneTransform()` into
   `Pose` the way `ApplyRecordedMotion` already does for run/idle.
3. **Timing authority.** `C26MatchGameMode` owns release and contact timing. The clips must be
   *driven* by those instants, not allowed to own them — the same rule the audio director follows.
   `A_C26_BattingDrive` contact is at frame 23 of 36 (0.958 s in) and `A_C26_BowlingPace` release is
   at frame 31 of 46 (1.292 s in); those are the frames to sync against.
4. **Quality.** This is a competent first authored pass, not mocap and not Cricket 24. It is
   structurally correct — right hand path, right weight transfer, right arm circle — and it is
   editable in Blender by hand from here, which the procedural poser never was.

## Related, still blocked

`Content/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001.uasset` exists with a
sculpted athletic body but **no skeleton and no skinned mesh** — MetaHuman auto-rigging runs on
Epic's cloud service and needs an authenticated Epic account with MetaHuman entitlement. One manual
editor sign-in unblocks it; then `Tools/MetaHumanBuildPlayer001.py` completes rig + assembly
unchanged. See `Docs/AI_HANDOFF.md`.
