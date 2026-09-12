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
-Y = the character's FORWARD (toes point -Y in armature space)
+X = the character's LEFT   (LeftArm tail is at +X)
+Z = up
Rest pose is a T-pose: arms straight out along +/-X, legs straight down.
```

> **Facing correction (2026-09-13).** The block above originally claimed `+Y` forward. It was
> wrong: measured against the shipped rig (toe vector in armature space is `(2.7, -21.4, 24.3)`),
> the character faces **-Y**. Every keyframe was authored under the wrong assumption, so the
> entire action arc — backlift, contact, the front-foot stride, the bowling release — landed on
> the character's BACK: the batter drove the ball through his own spine. No rigid transform or
> mirror of the solved poses can repair that (torso and limbs need different fixes); the keys are
> repaired at solve time instead: `repair_facing()` in `c26_anim_author.py` conjugates the spine
> twists by rotZ(180) (leans negate, turns keep) and rotates every IK target by
> `(x,y,z) -> (-x,-y,z)` (poles keep x/z, negate y). The keys themselves stay readable as
> authored (forward = +Y in the key comments); the repair is applied in `build_action` and by
> `Tools/rebuild_authored_clips.py` offline. Verified results on the corrected clips: contact
> hands **81 cm in front** of the hips, backlift **12 cm behind** them, bowler travels
> **71 cm down the pitch**, release hand **71 cm above** the head origin.

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

## The pipeline (current, 2026-09-13)

```
c26_anim_author.py            the keys + solve + repair_facing (single source of truth)
  |
  +- Blender path:  Blender --background C26_KitBase_v002.blend --python c26_anim_author.py
  |                 -> bake -> export_anim_fbx.py -> ArtSource/Exports/Animations/*.fbx
  |
  +- Offline path (no Blender needed; same keys, same solve -- reproduces the Blender
     result bit-exactly on the frame-1 Hips rotation):
       Tools/rebuild_authored_clips.py   solve + bake every frame  -> Animations/Solved/*.fbx
       Tools/correct_authored_anim.py    v2->v1 retarget + ground pin + 11-point
                                          geometric verification    -> Animations/Corrected/*.fbx
       Tools/ImportAnimations.py         UE import onto the shipped skeleton
```

The offline path exists because the rig-facing repair had to be proven without a Blender
install: `rebuild_authored_clips.py` imports the authoring module through a mathutils shim
(`Tools/c26_mathutils_shim.py`), rebuilds the TRUE v2 T-pose rest from the shipped asset rig
(`C26_KitBase_v001.fbx` scaled by 1/223.739 into the armature frame -- the anim FBX's own Model
defaults hold the frame-1 POSE, not the rest), re-solves every key with `repair_facing`, and
interpolates at 24 fps. `correct_authored_anim.py` then expresses the result on the shipped
skeleton (world-relative orientation copy + hips delta at the measured rig ratio 2.23739 +
per-frame ground pin) and refuses to emit anything that fails verification.

## Verification actually performed (offline, on the CORRECTED clips)

147 checks across the 18-clip library (`python3 Tools/correct_authored_anim.py`), all PASS:

- Every clip: chest faces the bowler at the stance AND the defining frame (the gate a
  whole-body yaw bug cannot hide from -- one existed; see below); stance ankles at
  ground; hips at athletic height; hands together at the stance.
- BattingDrive: contact hands together, **+76.8 cm in front** of the hips, backlift
  **-31.9 cm behind** them.
- BattingPull: contact **+83.2 cm in front**, hands **73.1 cm above** the hips (a
  genuinely horizontal shot), follow-through **48.5 cm to the leg side**.
- BattingCut: contact **57.9 cm to the off side**, **15.9 cm in front** (late --
  level with the body, as a cut is played).
- BattingSweep: contact hips **124 cm** (a deep crouch; standing is 209), hands
  **18.1 cm above** the hips (low over the ball), **60.9 cm in front** of the pad.
- BattingDefence: contact **56 cm in front**, bat **17.5 cm above** the hips (under
  the eyes), and the absorb frame stays at 13.7 cm -- no swing to finish.
- BattingHook: contact **93.0 cm above** the hips (head height -- the pull's is 73),
  and the follow-through whips **68.3 cm to the leg side and 122.6 cm above** the
  hips -- behind square, not through midwicket like the pull.
- BattingLoftedDrive: the contact is the drive's own (**78.4 cm in front**, **42.3 cm
  above** the hips) and the finish goes **overhead (361.8 absolute)**, well above the
  drive's shoulder-height follow-through.
- BattingGlance: contact **7.3 cm above** the hips (low off the hip), **42.5 cm in
  front** of the pads, and the deflect frame crosses **53.8 cm to fine leg at only
  21.7 cm above** the hips -- soft hands, no arc.
- BowlingPace: release hand **400 cm vs head 329** (fully extended), hips **74.6 cm
  down the pitch**; BowlingOffSpin: release **360** (deliberately lower -- finger
  spin), hips 65.7; BowlingLegSpin: release **399**, hips 74.6.
- BattingBackFootDefence: contact blocked **17.1 cm in front** of the hips (the
  forward defence presses 56 cm out), bat **37.0 cm above** the hips, absorb frame
  dead still.
- BattingLateCut: contact **69.3 cm off side** and **-4.2 cm in front** -- BEHIND the
  hip line (the square cut meets the ball +16 in front) -- with the slice finishing
  **73.7 cm off side** toward third man.
- BattingUpperCut: contact **66.9 cm off side** at **104.1 cm above** the hips and
  **-1.9 cm in front** -- behind the body line -- with the follow-through steering
  **67.3 cm off side and 131.0 cm above** (over the slips).
- UmpireSignalWide: arms **+142.4 / -141.6 cm** out at **86.4 cm above** the hips
  (shoulder height) at the signal frame AND still there at the hold frame (36).
- UmpireSignalSix: both hands **389 cm absolute / +189.3 above** the hips.
- UmpireSignalOut: right hand **392 cm** up, left hand **-3.1 cm above** the hips
  (hanging at his side, exactly as an out signal reads).
- UmpireSignalFour: the arms sweep **-49.4 cm** across to the left, **+51.1 cm**
  back across to the right, and finish **+132.2 / -133.0 cm** out at **42 cm above**
  the hips -- the boundary sweep, waist height.

The same facts are re-asserted IN ENGINE by `Cricket26.Anim.AuthoredClips` (all 18 clips)
(`Tests/C26ProductionTests.cpp`) for all 8 clips, sampling the imported
AnimSequences through `GetBoneTransform` after the FBX importer's own conversion.

### The second corrector bug (found with the library, 2026-09-13)

Authoring the cut exposed that the corrector's HIPS branch took its world-rotation
reference from the anim FBX's Model defaults. Those defaults are not a rest pose --
the original Blender exporter froze a stale POSE into them -- so every corrected
clip was rotated by that pose's hips twist: the whole drive came out with a
**-52.4 degree chest yaw** (batter facing midwicket), invisible to the foot-relative
checks of the time. The branch now references the TRUE v2 T-pose, which (because v2
is the v1 rig uniformly scaled and re-based by the armature node) is simply the v1
rest world rotation, with positions `(v1 - armatureNodeT) / ratio`. Verified: the
corrected cut's hands-hips offset now equals the solved clip's offset x ratio
bit-exactly, and chest yaw gates were added to every clip's checks.

## What is NOT done

1. **UE-side import + in-match playtest of the corrected clips.** The corrected FBX exist and
   pass offline verification; `Tools/ImportAnimations.py` now imports from `Corrected/`, and
   `AC26Athlete::Animate` applies both clips (re-enabled 2026-09-13, see the branch comments at
   the two `ApplyAuthoredClip` call sites), but the import + build + playtest must run on a Mac
   with UE 5.8. The in-engine gate is `Cricket26.Anim.AuthoredClips` plus the BatLab/BowlLab
   playtests.
2. **Library gaps.** Batting covers drive, lofted drive, pull, hook, square cut,
   upper cut, sweep, glance, forward defence and back-foot defence; bowling covers
   pace, off-spin and leg-spin; the four umpire signals (wide / six / out / four)
   are authored, plus the late cut. The clip table is now 1:1 with
   C26Controls::ShotFamily(): every stroke the scorecard can name has either its
   own clip or a deliberate stand-in. Deliberately NOT clip-authored:
   fielder pickup/catch/dive/throw and the keeper's take, because those actions
   solve toward the LIVE ball position and a fixed clip would aim at nothing --
   they stay procedural by design. Not yet authored: the upper cut / late cut
   split, back-foot defence, keeper crouch idle. With the offline pipeline each
   new clip is a pure-Python key-list addition to `c26_anim_author.py` plus a job
   in `Tools/rebuild_authored_clips.py`.
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
