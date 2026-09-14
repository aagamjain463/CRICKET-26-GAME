# Character Presentation Stabilization

**Round 3, Task 4** — planted-foot stabilization, transition continuity, orientation stability.
Branch `worktree/4`. Commits `375a032` … `0c32800`.

This is the **shared presentation foundation**: the layer every athlete uses. It does not own any
sport-specific animation, and it deliberately does not touch batting, bowling or fielding clips.

---

## 1. What changed

Six source files, no assets:

| File | Change |
|---|---|
| `Source/CRICKETGAME/Characters/C26CricketerAnimInstance.h/.cpp` | Foot-IK evaluation inside the existing native animation proxy: two-bone solve per leg, pelvis offset, and the *authored* ankle published for measurement |
| `Source/CRICKETGAME/Characters/C26CharacterPresentationComponent.h/.cpp` | Lock acquisition/release, hysteresis, transition and orientation rules, and all the tuning constants in one namespace |
| `Source/CRICKETGAME/Characters/C26CharacterTests.cpp` | 4 new automation tests |
| `Source/CRICKETGAME/CRICKETGAME.Build.cs` | `AnimationCore` added, for `TwoBoneIK.h` |

Not touched: any `.uasset`, any animation clip, body geometry, face, kit, equipment, scoring,
match rules, ball physics, HUD, camera.

---

## 2. The system

### Foot stabilization

There is no new animation framework. The project already drives athletes through a custom native
animation proxy (`FC26CricketerProxy`, an `FAnimInstanceProxy`) that blends two sequence evaluators
with `FAnimNode_TwoWayBlend`. Foot stabilization is added **inside that existing evaluate pass**, so
it costs no extra tick and no extra component.

Per frame, per leg:

1. The component decides whether the authored foot is planted (near the ground plane, and the state
   is one the foundation owns). If so it records a **mark** — a world position at ground height.
2. The proxy solves a two-bone IK chain from hip to that mark with
   `AnimationCore::SolveTwoBoneIK(..., bAllowStretching=false)`, then blends the solved leg over the
   animated pose by `LockAlpha * FootIKWeight`.
3. The pole/plane target is derived from the *current* knee offset, so the authored bend direction
   survives the solve — the knee bends the way the animator drew it, not the way the solver prefers.

Marks are **weight-ramped, never switched**. A mark fades in at rate 18 and out at rate 24, so no
frame ever shows the ankle jumping between two world positions.

### Transition continuity

- **Locomotion hysteresis** — start at 22 cm/s, stop at 10 cm/s. A single threshold let a speed
  hovering at the boundary flap idle↔locomotion and restart `Start`/`Stop` every few frames.
- **Spent transitions** — a `Start`/`Stop` clip hands back once it has done its job instead of
  playing to its full length. Turn clips always play out.
- **Advancing outgoing pose** — while a blend runs, the outgoing clip keeps advancing instead of
  freezing on the switch frame. A frozen outgoing pose reads as the body stalling for the whole
  blend; it was most visible on action→recovery hand-backs.
- **Smoothstep blend weight** — the incoming pose leaves and arrives with zero velocity, so neither
  end of the blend snaps.
- Blend times were **not** globally raised. `MinBlendSeconds=0.02` is a floor, not an increase.

### Orientation stability

Authoritative re-aims (the gameplay layer snapping an athlete's facing) are absorbed by a
**mesh-only** yaw lag, bounded at 135°. The actor's rotation is never modified — this is
presentation only, and gameplay movement rules are untouched.

---

## 3. Two silent bugs found and fixed

Both were shipped, both were invisible from the outside, and both are worth knowing about.

### The release threshold sat below the athlete's own stance

`LegReach=86.f` with a 94% factor released a mark at **80.84 cm**. Measured in match, the athlete's
planted hip-to-ankle reach is **82.4 cm**, and at the planted frame of `C26_A_Run` it is
**87.59 cm** — the whole leg. **Every mark was released on the frame it was taken.**

It looked alive because marks were *still being taken*. Only the per-cycle hold count exposed it:
`352` of `426` locks held `0` frames, and `420/426` releases were `why=reach`.

Fixed by measuring the leg from the reference pose and releasing at `RefLegLength * 0.98f`
(**85.84 cm**). Segment lengths are pose-independent, which is why the leg is the right input — the
reference pose is a **straight bind pose** and says nothing about how bent the knees are in a
stance. `LegReach` was deleted, so there is no longer a second source of truth for the athlete's
own geometry.

> **Rule for future work:** any threshold in centimetres that describes the body must be *measured
> from the body*. A constant is a second, silently-diverging copy of the rig.

### The measurement added a frame of actor travel

Reading the rendered ankle via `GetSocketLocation` from inside the stabilizer combines the
**previous** frame's bone local with the **current** component transform. At 400 cm/s and 30 fps
that is ~13 cm of phantom movement. It produced `rendered 18.02 cm` with locking on versus
`authored 11.00 cm` with it off — locking appearing to make skating *worse*.

Fixed by measuring after `RefreshBoneTransforms()`, so both ankles come from the same frame. Each
lock cycle now carries its own control (`MaxDrift` rendered vs `MaxAuthoredDrift` authored, over
the same frames), which also removes the need for an A/B run: two matches differ in gameplay
timing, so a cross-run comparison measures the match, not the stabilizer.

> **Rule for future work:** never read a rendered transform from the same tick that authored it.

### Hardening: the leg measurement can now fail safely

`RefLegLength` is a **sum of distances between three bones** (`thigh_l` → `calf_l` → foot), and
`thigh_l`/`calf_l` are the skeleton's own names, not remapped by the profile. The first version
returned `ZeroVector` for an unresolved bone, which silently turns the total into a distance from
the **mesh origin** — a plausible-looking number, which is the dangerous kind. Too large and no mark
ever releases (the solver drags the foot until the hip tears it off); too small and every mark dies
on arrival again.

Three changes make the whole path fail safely:

- The bone lookup **reports success** instead of substituting a zero vector, and the result is only
  trusted when the whole chain resolved **and** lands inside `MinPlausibleLegLength..MaxPlausibleLegLength`
  (55–120 cm). Otherwise the `-1` sentinel is kept.
- `LockReleaseReach` **validates its own input** and falls back to the *most generous plausible leg*
  (120 cm × 0.98 = **117.6 cm**). A caller that forgets to validate cannot produce a threshold that
  silently never releases. It is safe for any input, including the sentinel and garbage.
- The reference pose is now measured once behind an explicit `bMeasuredRefPose` flag, because
  `RefAnkleHeight` has a legitimate value of 0 and therefore cannot double as a "not yet measured"
  sentinel — a mesh that failed to measure would have been treated as measured at height zero and
  pressed into the pitch permanently.

`FC26FootSolverTest` asserts the band **contains the real measured leg** (so the guard cannot reject
the athlete it is running on), that every garbage input fails towards holding, that the fallback
dominates every plausible leg, and that the threshold genuinely tracks the leg inside the band.

### Acquisition and release are now the same predicate

The release fires when the hip-to-mark distance exceeds the threshold, but acquisition accepted a
mark at *any* reach. Marks were therefore taken at `reach=87.9 cm` against a `85.84 cm` release —
recorded, released on the next frame, and logged as `held=0`. In a match that was **54 of 151
cycles (36%)**: the solver being handed work it was guaranteed to throw away.

Acquisition now requires the mark to sit within the release distance of the hip, so the two rules
are one predicate evaluated at two moments. Measured effect:

| | before | after |
|---|---|---|
| lock cycles | 151 | **97** |
| cycles holding 0 frames | 54 (36%) | **0 (0%)** |
| marks held ≥4 frames | 23, 76.5% removed | 23, **76.5% removed** |
| cycles that added motion | 0 | 0 |

The productive cycles are **byte-identical** (504.8 cm → 118.5 cm either way), which is the point:
the change removed only dead work. The same stabilization now costs 36% fewer lock cycles and zero
wasted acquisitions.

---

## 4. Result

151 lock cycles before the consistency fix, 97 after, fixed-step 30 fps, `DA_C26_FielderReview`,
`-C26CharacterSlice`:

| frames held | cycles | authored slide | rendered slide | removed |
|---|---|---|---|---|
| 0–1 | 8 | 60.9 cm | 22.1 cm | 63.6% |
| 2–3 | 66 | 1697.8 cm | 606.9 cm | 64.3% |
| 4–7 | 17 | 388.4 cm | 117.9 cm | 69.6% |
| 16+ | 6 | 116.4 cm | 0.6 cm | **99.5%** |

- **97/97** cycles where the authored foot slid ≥1 cm reduced the slide; 2263 cm → 748 cm overall.
- Marks held **≥4 frames** (the ones that actually do work): 504.8 cm → 118.5 cm,
  **76.5% removed**, worst rendered drift 8.21 cm.
- **Zero** cycles where locking added motion.
- **Zero** cycles that hold for no frames — every mark taken is a mark that can hold.
- Every cycle whose authored foot was still rendered **exactly 0.00 cm** — no new jitter.
- Symmetric across feet: left 65.3%, right 68.2%.

The number to quote is **76.5% on marks that hold, 99.5% on the longest holds, zero added motion**
— not the flat 67% over all cycles, which is diluted by the marks that release immediately because
the foot was never really planted.

### Foot-locking behaviour, precisely

| Event | Rule |
|---|---|
| Acquire | state is owned by the foundation, foot within 4 cm of the ground plane (6 cm below 60 cm/s), and the previous mark has faded below 0.25 |
| Release | hip-to-mark distance exceeds `0.98 × leg length`, **or** the authored ankle rises above 8 cm, **or** the state leaves the owned set |
| On release | weight ramps out at rate 24 — never a cut |
| Pelvis | compensated only when the mark sits *below* the animated ankle, clamped to 6 cm, and at half weight for a single-foot lock |
| Scope | no lock is ever acquired on a batting, bowling or fielding clip |

The scope boundary holds live: the only out-of-scope releases in a match were `why=state` ×3,
each firing the frame the state left the owned set. The 1–2 frames of residual weight are the
deliberate fade-out, because cutting the weight is exactly the popping PART B forbids.

### Performance

The solve only runs when a lock is held, and a separate LOD gate skips the stabilizer entirely for
athletes beyond the near band — 54 skip events in a match, 41 of them at `lod=2`. There is no
full-body solve, no per-frame trace (the ground probe is cached and refreshed only when the athlete
moves 25 cm or the cache ages past 0.25 s), and no extra component tick.

---

## 5. Testing

**Automation** — `Cricket26.Characters` **9/9**:

| Test | Covers |
|---|---|
| `FootSolver` | zero-weight inertness, pelvis offset, full-lock reach, no thigh/calf stretch, unreachable mark clamps, half-weight proportionality, **knee never inverts**, lock reach derived from the measured leg |
| `FootSkate` | both feet, per-foot authored stance speed, distance-per-stance invariant (within 20%), and **no new foot snapping** (`After.MaxStep ≤ Before.MaxStep + 0.5`) |
| `LocomotionTransitions` | hysteresis band, scope boundary, `Start`/`Stop` exits, orientation snap absorb and unwind |
| `ActionRecovery` | a real action→recovery hand-back (`A_C26_FastBowl_R` → `A_C26_FielderReady`), including the clip wrap |

Full `Cricket26` suite: **28 pass / 1 fail**. The failure is `Cricket26.Anim.AuthoredClips` on
`A_C26_BattingDrive`/`A_C26_BattingPull` — other agents' batting assets, geometry-level assertions
about chest facing and hands together. It predates this work and references no file this task
touched.

**In-match gate** — `172 PASS / 2 FAIL`, both `ball intersects rendered blade triangles`
(stages 0 and 2), reading the legacy-path batter `Team1_Player2`, who is absent from the ten
activated athletes. Pre-existing.

**Transitions demonstrated**: idle→movement and movement→idle through the hysteresis band,
`Start`→`Run`→`Stop`, turn clips, and an action→recovery hand-back.

---

## 6. Limitations (reported, not fixed)

1. **`Quality == Low` is never assigned**, so the Low-quality bypass in the stabilizer is
   unreachable. Wiring it needs `AC26Athlete`/`AC26Stadium`, cannot be validated at the current
   editor load, and would make Low quality *disable* the stabilization — a behavioural change, not
   a bug fix.
2. **No valid mobile performance measurement yet.** Editor load on desktop says nothing about the
   target hardware. The design is conservative (no per-frame trace, LOD-gated, only-on-lock solve),
   but that is an argument, not a measurement.
3. **`C26_A_Run` plants with a fully straight leg** (`knee bulge 0.00 cm`), so no lock can survive
   on that clip's plant. The travel assertions are guarded behind `if (PlantedReach < RefLeg * 0.9f)`
   and the limitation is reported through `AddInfo`. This is an authoring property of the clip, and
   the clip belongs to another workstream.
4. **The leg-derived release could not be re-verified on a second skeleton in-match.** Only one body
   (`SK_C26_Athlete_Review`) is in service, and the one other profile (`DA_C26_BatterReview`) fails
   its own structural gates — `C26_CHARACTER_MIGRATION_BLOCKED … (10 errors)` and
   `C26_CHARACTER_ASSET_GATE BowlerReady: missing role animation` — so only the batter activates and
   no locomotion state is ever entered. The robustness gap is instead closed by unit test: the
   plausibility band is asserted to contain the real leg, and `LockReleaseReach` is asserted safe for
   sentinel and garbage input. When a second body arrives, re-run the drift log on it and confirm the
   measured `legLength` differs and `releaseAt` tracks it.
5. **`ReviewPremiumCharacter.sh` cannot verify any of this.** `AC26CharacterReviewMode` drives
   `UC26CricketerAnimInstance` directly and never creates or ticks the presentation component, so
   `FootIKWeight`/`LockAlpha` stay at their defaults and the foot IK is inert in review captures.
   Only the `-C26CharacterSlice` match path exercises it.
