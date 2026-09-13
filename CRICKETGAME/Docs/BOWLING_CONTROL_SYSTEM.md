# Bowling control system

Touch-first, player-authored bowling. The player states intent; the model decides
how much of it a real bowler could actually deliver.

```
SELECT DELIVERY TYPE -> SELECT EXACT PITCH TARGET -> SET MOVEMENT DIRECTION
-> SET MOVEMENT AMOUNT -> SET PACE -> START RUN-UP -> USE RELEASE BAR
-> RELEASE IN TOO EARLY / EARLY / GOOD / PERFECT / NO BALL
-> ACTUAL DELIVERY COMPOSED FROM ALL OF IT
```

## Where it lives

| File | Owns |
|---|---|
| `C26Delivery.h` | The whole bowling model: libraries, pace windows, the release bar, and `Compose()` — plan + execution + ability -> actual ball. |
| `C26Types.h` | `FC26BowlingPlan` (intent), `FC26BowlerProfile` (ability), `FC26BowlingTuning` (every constant), `FC26ReleaseBar`. |
| `C26MatchGameMode.cpp` | Pointer routing, `StartDelivery`/`ReleaseBall`, `Compose` call, the trajectory preview, the crease offset. |
| `C26Simulation.cpp` | The integrator. Applies swing in the air and deviation off the pitch, in different phases of flight. |
| `C26HUD.cpp` | Draws the controls and the `— BOWLING PLAN —` debug readout. |
| `Tests/C26BowlLab.cpp` | 39-delivery scripted playtest through the real pointer path. |

## Control map (1600x900 design space)

| Control | Where | Does |
|---|---|---|
| Delivery carousel | arrows at x=84 / x=338, y=426 | Steps through `DeliveryLibrary`, rebuilt per bowler. |
| Pitch target | drag anywhere on the pitch | Sets `TargetLine` / `TargetLength` continuously. Presets reposition it; fine adjustment stays manual. |
| Movement dial | centre (1352, 640), r=78 | Distance from centre = amount. Off-centre = direction (only for deliveries whose direction is the player's to choose). |
| Pace slider | x 1184..1520, y=742 | `PaceNormalized`, mapped into the bowler's window for that delivery. |
| Crease | (134, 687) | Over / around the wicket. Moves the release point, not the aim. |
| START RUN-UP | (1380, 840) | Locks the plan and starts the run-up. |
| Release | tap the pitch during the run-up | Commits the delivery at the current meter value. |

Every control is owned by the pointer that grabbed it, so two fingers drive two
controls without stealing each other, and a second finger cannot re-release the ball.

The delivery **type carousel is the only selectable option** on the planning
screen (`STOCK PACE` is the default, and it is the first entry of every seam
bowler's locker). The six quick preset buttons — YORKER, 4TH OFF, BOUNCER,
WIDE Y, SL CUT, IN YORK — and the four-line delivery-plan readout were removed:
they duplicated what the carousel, the pitch drag and the dial already say.

## The release bar

Bands, left to right, at Normal difficulty (`ActiveBar`):

```
TOO EARLY | EARLY | GOOD | PERFECT | NO BALL
  0.00      0.42    0.62   0.90      0.957
```

- **NO BALL sits directly next to PERFECT.** The reward and the risk are one
  decision apart, which is the whole point.
- **The PERFECT band is deliberately narrow** — 5.7% of the bar (it was 10.2%
  before `PerfectStart` moved from 0.855 to 0.90). Narrowing it is the difficulty
  knob: the top reward is a tighter target, and `DifficultyWidth` scales it
  further (Easy x1.45, Hard x0.72, Expert x0.55).
- The bar is snapped to the bowling animation, not a UI timer: `NoBallStart` is
  derived from the front-foot plant, and the meter is `PhaseTime / RunUpDuration`.
- Quality is a continuous curve, never a bucket. The peak sits 18% into the
  Perfect band — as close to the no-ball line as it is possible to be.
- Releasing past the line is a **real no-ball**: one extra run, the ball does not
  count toward the over, and the free hit is armed. It is decided by where the
  meter was, never by a roll.
- Releasing early costs pace, accuracy and movement — signed and graduated, so a
  terrible release lands ~160 cm from the aim and a perfect one ~2 cm.

## Movement: two phases of flight, not one renamed effect

`FC26DeliveryPlan` carries two separate accelerations, and the integrator applies
them in different phases:

- **`Swing`** — in the air, before the ball pitches. Conventional swing starts at
  release. **Reverse swing** starts only after `SwingOnset`, which is authored as a
  *fraction of that delivery's flight* and resolved to seconds at release, so it is
  late at every pace and length.
- **`Deviation`** — off the pitch, after the bounce. This is a cutter, a leg break
  or an off break. There is no air movement at all.
- **`Seam`** — the presentation hook at the moment of impact.

Direction is authored **batter-relative** (+1 = away from the batter) and only
converted to world X inside `Compose()`, so all four combinations of bowler arm and
batter hand work with no hardcoded sign anywhere.

`Release()` trims the launch by exactly the displacement the movement is about to
produce (`C26Delivery::SwingDeflection`), so the ball still touches down on the point
the player aimed at — whether the movement runs the whole flight or only its last
part.

### Measured, not asserted

`Tools/BowlLab.sh` re-integrates every delivered ball twice — once as bowled, once
with all movement stripped and the identical launch — and measures the lateral
separation at three points: half way, at the pitch, and at the bat. From a passing
run:

| delivery | half way | in air | off pitch | total | target error |
|---|---|---|---|---|---|
| outswinger | +2.2 | **+8.8** | +4.8 | +13.6 | 2.2 cm |
| inswinger | -2.2 | **-8.8** | -4.8 | -13.6 | 4.2 cm |
| leg cutter | +0.0 | **+0.1** | **+5.6** | +5.7 | 1.1 cm |
| reverse out | **+0.0** | +7.5 | +8.2 | **+15.7** | 3.1 cm |
| steered plain ball | +1.4 | +5.6 | +2.3 | +7.9 | 5.8 cm |

A cutter moves ~0 cm in the air and ~6 cm off the pitch. Reverse swing is dead
straight at half flight and only bends after that. Those are the two facts the
delivery library is built on, and they are read off the trajectory rather than
taken from the parameter that requested them.

## How to bowl each delivery

| Delivery | Type | Dial | Pace | Watch for |
|---|---|---|---|---|
| **Outswinger** | OUTSWINGER | push up 0.7 (direction is locked by the name) | ~0.62 (140 km/h) | Pitches 2 cm from the aim, bends 8.8 cm away in the air. |
| **Inswinger** | INSWINGER | push up 0.7 | ~0.62 | Mirror of the above, into the batter. |
| **Leg cutter** | LEG CUTTER | push up 0.7-0.9 | ~0.62-0.95 | No air movement, 5.6 cm break off the pitch. |
| **Off cutter** | OFF CUTTER | push up 0.7-0.9 | lower effort | Breaks the other way off the pitch. |
| **Slower ball** | SLOWER BALL | low | any | Comes out ~17 km/h slower; the pace window is capped by the type. |
| **Reverse swing** | REVERSE OUT / IN | push up 0.7 | higher effort | Straight for the first 45% of the flight, then the biggest bend in the set. |
| **Hand-built** | STOCK PACE | push **off-centre** | any | The direction you push is the way it swings. |

Reverse swing is conditional: it is only in the locker once the ball is four balls
old, or immediately with `-C26ReverseAlways`.

## Verifying it

```bash
bash Tools/BowlLab.sh bowllab            # 39 deliveries, expects C26_BOWL_PASS failures=0
bash Tools/BowlLab.sh bowllab shots      # + a PNG per delivery
```

`-C26BowlLabFPS=60` fixes the simulation cadence so a release lands on the same
meter value on every machine. The lab presses the real carousel, drags the real
target, turns the real dial, moves the real slider and releases on the real bar
through `AC26PlayerController`, so the HUD hit-testing, the design-space
conversion, pointer ownership and the one-release guard are all exercised.

## Debug overlay

`-C26Debug` (or the `C26Controls` exec) draws the `— BOWLING PLAN —` panel:
state, type, movement phase, desired vs actual target and the error between them,
desired vs actual pace, wanted direction and amount, actual swing/deviation/onset,
release meter, band, quality, no-ball flag, the live bar boundaries, both hands and
the crease. Development only — it is inside `#if !UE_BUILD_SHIPPING`.

## Known limitations

- **Conventional swing is a bow, not a pitch-point offset.** Because the aim is
  honoured exactly, the visible signature of swing is the curve of the path plus
  the angle it arrives on. The bat sees ~5 cm of extra movement. If a bigger
  visible break is wanted, raise `MaxSwingAccel` — it is a single tuning constant.
- **Spin deliveries are wired but the two bowlers are seamers**, so the spin
  library (off break, googly, doosra, flipper) is reachable only by changing
  `BowlerProfile.Kind` in `RefreshBowlerProfile`.
- **Stamina is per-over only.** `FC26BowlerProfile::Stamina` drains across the six
  balls and never recovers across overs, because the Super Over is one over long.
