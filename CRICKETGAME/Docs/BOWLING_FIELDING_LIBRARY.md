# Premium bowling, fielding and wicketkeeping library (Round 5)

Source: `ArtSource/Blender/Premium/c26_actions.py` (section "round 5"). Same architecture as the
Round 4 batting library: technique authored as measurements, cumulative beats, dense per-frame
solve, one right-handed source per action. Opposite-handed clips are the FULL sagittal mirror of
the whole pose (root, feet, hips, torso, shoulders, arm, wrist, release), never an arm flip.

## Clips

| Clip | What distinguishes it |
|---|---|
| FastBowl_R / _L | right-arm / left-arm fast: high bound, fully side-on back-foot contact, 104cm braced stride, tall release, long carry |
| FastMedium_R / _L | semi-open action, lower bound, 86cm stride, shorter follow-through |
| OffSpin_R | right-arm off-spin: straight approach, small hop, chest-on, high arm, finger grip, compact pivot |
| OffSpin_L | left-arm orthodox (the whole-body mirror of finger spin) |
| LegSpin_R / _L | wrist spin: angled approach, big gather jump, side-on, 90cm stride into a flexed knee, rounder arm, cocked wrist, large rotation and pivot |
| Pickup / PickupRunning | stationary gather / gather in stride with crow hop, both ending in the side-on throwing load |
| Throw / ThrowQuick | crow-hop overarm return / flat snap throw for close run-outs |
| Catch / CatchHigh / CatchLow | chest, overhead (fingertips to 194cm) and low catches, each with a give |
| DiveCatch_L / _R, DiveStop_L / _R | full-length dives (catch / ground stop), roll up onto a knee and stand |
| SlideSave | boundary slide onto the right hip, right hand sweeps the ball |
| KeeperReady, KeeperShuffle_L / _R | crouch/rise loop, square lateral shuffle (feet never cross) |
| KeeperReceive / KeeperTakeLow / KeeperDive_L / _R | normal take with give, stay-down low take, full-stretch dives, gather into recovery |

Bowling clips are authored in WORLD space along the run-up. Every bowler's root runs -2700 to -995
(`C26MatchGameMode` RunUp curve) and drifts on at 330cm/s after release; `c26_rig.shift_to_actor`
subtracts that travel at bake, so planted feet stay planted once the game moves the root.

## Gameplay integration

`UC26CharacterPresentationComponent::Variant` picks the clip when the action starts and latches it
until the action ends (no mid-action swaps):

- Bowling: `BowlingKey` (pace / finger spin / wrist spin) with the actor's arm; FastMedium/Medium
  bowlers use `FastMedium_*`. `RefreshBowlerProfile` now copies `bLeftArm` and `Kind` onto the bowler
  actor - before this, left-arm V. SEN rendered a right-arm action and released from the right hand.
- Pickup: `PickupRunning` when the fielder arrived above 150cm/s.
- Throw: `ThrowQuick` within 18m of either set of stumps.
- Catch: side/height of the ball in actor space, only once it is within 2.6m: dive L/R, high, low.
- Dive: `DiveCatch_*` / `DiveStop_*` by ball height and side; the dive clip carries on through the
  match's Dive -> Pickup/Catch switch because it already holds that event at the hands-on-ball frame.
- Keeper: `KeeperDive_*` / `KeeperTakeLow` / `KeeperReceive`; moving keeper shuffles by direction.
- Every variant falls back to the Round 3 base clip if a profile lacks it.

Fixed on the way: event-driven athletes are posed with `Dt==0`, so the blend clock never advanced and
the whole pickup rendered the previous run pose (ball met hands 85-90cm off the turf). The blend
now advances on action time. Engine-measured gather miss: 11-16cm.

## Verification

```bash
B=/Applications/Blender.app/Contents/MacOS/Blender
$B --background --python ArtSource/Blender/Premium/action_lab.py [-- --render]   # C26_ACTION_LAB PASS clips 24
$B --background --python ArtSource/Blender/Premium/author_cricket_actions.py -- --only FastBowl_R,...
UnrealEditor CRICKETGAME.uproject -ExecutePythonScript="$PWD/Tools/ImportRound5Actions.py"
```

`action_lab.py` checks every frame: bowling-side back foot / opposite front foot, front shoulder
leads at back-foot contact, bowling hand above the head and on its own side at release, arm tilt,
hips open before the chest, no squat through the delivery, no world-space foot skating, no
foot/knee through the floor, no wrist dislocation, no hand teleport, no pelvis/chest snap, palms on
the event target, body over the gameplay root, and fast/medium/spin actions measurably different.

Results (2026-09-14):
- Action lab 24/24 PASS; batting lab still 14/14 PASS.
- GoldenGate `round5_outfield` (premium bowler + 9 fielders): release gap 0.000cm, `FastBowl_R`
  right hand 35-38cm above the head; `PickupRunning` then `ThrowQuick` selected; only the 2
  pre-existing old-batter blade failures remain.
- BowlLab with the premium bowler: `C26_BOWL_PASS deliveries=39 failures=0`; N. ARCHER plays
  `FastBowl_R` (arm R), V. SEN plays `FastMedium_L` (arm L, left hand 31-35cm above the head).
- BatLab with the premium batter: `C26_LAB_PASS deliveries=27 failures=0`.
- Engine review captures (`Artifacts/CharacterAudit/round5_engine_review.png`): OffSpin_L raises the
  left arm, LegSpin_R the right, DiveCatch_R dives to the athlete's right, KeeperDive_L to the left.
- Automation: all pass except the pre-existing legacy `Cricket26.Anim.AuthoredClips`.

## Limitations

- Keeper clips are imported and selectable, but the match keeper still uses the old visual: there is
  no keeper kit on the new skeleton, so `InspectRole(KEEPER)` correctly fails.
- No spinner exists in the two match bowler profiles; spin is validated in the lab and review map.
- No runtime hand IK: gathers match by authored reach (11-16cm engine miss at the ball).
- The match starts a pickup at the gather moment, so the approach to the ball is short, and a
  dive that ends in a stop still blends into the throw load on the match's throw clock.
- SlideSave is authored and selectable (`EC26Action::Slide`) but the match never sets that action.
- Motion is kinematically authored, not captured.
