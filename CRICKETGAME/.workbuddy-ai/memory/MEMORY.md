# CRICKET 26 — durable project conventions

Distilled, cross-session facts. Daily detail lives in `YYYY-MM-DD.md`.

## The pose clock (this bites constantly)

`C26MatchGameMode` drives load-bearing actions by **assigning `ActionTime` and calling
`Animate(0)`** — the `EventPose` test (`C26MatchGameMode.cpp:1745`) forces `Dt = 0` for the
bowler's release frame, the batter's contact frame, and the whole fielding sequence while
`ThrowClock`/`CatchClock`/`KeeperTakeClock` are live.

**Rule:** inside those actions, `Dt` is not a usable clock. Anything integrated per frame
(`X += Dt*rate`) is dead code. Solve it **in closed form from `ActionTime`** — which is also
frame-rate independent. `ShownSpeed` and `GaitPhase` are snapped to zero by the first
`Animate(0)`, so any state the pose needs from the *previous* frame must be captured in
`SetAction()` on the frame the action changes.

## Where the animation actually lives

Two independent paths, easy to confuse:

- **Procedural pose solvers** in `C26Motion.h` (pure header, athlete-cm space, `Rig(fwd,right,up)`),
  consumed by `AC26Athlete::Animate`. This is what ships for fielding/catching/diving/ready.
- **Authored clips** (`A_C26_*` uassets, baked offline by `ArtSource/Blender/Premium/c26_actions.py`
  via `Tools/rebuild_authored_clips.py`). Separate pipeline; `Docs/AUTHORED_ANIMATION.md`.

A Blender keyframe change does **not** affect the procedural solvers, and vice versa.

## Verify with the lab, not with screenshots

`Tools/*.sh` labs assert contracts per case and print text-diffable `C26_*_PASS|FAIL` lines.
Screenshots are weak evidence here: `Tools/Capture.sh` walks a fixed beat list and may never
reach the state you changed (proven for the bowling trajectory preview). Prefer the lab's
per-case numbers. Labs are launched as **background tasks**; a log with no summary line is an
*aborted* run, not a failing one.

## Known pre-existing failure — do not chase

`Cricket26.Anim.AuthoredClips` fails (chest facing / hands-together geometry checks on the
baked `A_C26_*` clips). Documented as pre-existing legacy in
`Docs/PREMIUM_CHARACTER_CONTINUATION.md`. Full-suite baseline is therefore **25/26**.

## Reach and the ball/hand gap

The fielding contract is the in-match one, not the solver test: `Tools/FieldGate.sh` drives
`-game -C26GoldenGate -C26GateSuite` and prints `C26_SUITE_PICKUP gap_cm` (contract < 14). It is
the only thing that sees the ball, because from `ThrowClock >= 0.20` the ball **is**
`ReceivingPosition()` — the midpoint of the two posed palms.

Three traps, all hit at once, all now fixed:

- **`ArmSpan` is the shoulder→wrist chain (51.8 cm), not a fingertip-to-fingertip span.** Every
  reach limit in `C26Athlete.cpp` reads it as `ArmSpan*.95–.98`. A lone `*.492` halved the arm.
- **`AnkleZ` is not the ground.** It is the ankle *joint*'s height above the rig origin (11.8).
  The turf is ~5 cm *below* the rig origin; a ball resting on it sits at **mesh-local Z = −1.40**
  (world 3.60 = `BallRadius`). Any absolute floor expressed from `AnkleZ` holds the hands at shin
  height. `world = actorLocation + meshLocal` (`meshScale` is 1.0; the 0.48 is baked into
  `Reference`).
- **`ReachableArm` runs before `ShoulderReach`,** which protracts the clavicle and carries the
  socket toward the ball — so the arm is clamped against a socket it no longer has.
  **Re-clamping the already-clamped point is a no-op** (a clamp only pulls inward, and that point
  is then inside the new reach). You must **re-project the raw target** from the new socket.

Also: the **displayed** pose is filtered toward the solved one (`PoseLag`, 0.038 s for `Pickup`),
and possession transfers at `ThrowClock = 0.20`. Hands that arrive at 0.20 are still ~16 cm high
on screen at the handoff — so the gather's hands deliberately lead the body (arrive at 0.17).

**Attributing a gap like this by reasoning about coordinate spaces failed twice.** Add a temporary
log inside `Animate` printing the shoulder, raw target, clamped target, achieved wrist and both
palms, then fix the one link the numbers name. One build beats three guesses.

## Tooling traps

- `grep` with `\|` alternation silently returns **empty** in this shell. Use the Grep tool
  (ripgrep), or one literal per `grep`.
- Build the **`CRICKETGAMEEditor`** target — `UnrealEditor -game` loads
  `Binaries/Mac/libUnrealEditor-CRICKETGAME.dylib`, which only the Editor target produces.
- A green build with **0 actions** compiled nothing. Read the action count.
- `-C26GateSuite` is required to reach the `C26_SUITE_*` contracts; `Tools/FieldGate.sh` wraps it.
- The suite drives a live randomised match, so its PASS/FAIL totals **fluctuate run to run**
  (batting and keeper contracts dominate). A stable `gap_cm` is the meaningful signal; totals are
  only for spotting a regression.
- Pre-existing, out of scope: `C26_GATE_CONTACT gap_cm≈67.4` — a systematic batting blade offset.

