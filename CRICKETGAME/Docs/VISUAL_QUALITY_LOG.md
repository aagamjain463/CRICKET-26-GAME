# Visual quality log

2026-09-09 fresh baseline `audit_baseline_0909`, 1600×900 Mac. Subjective scores
against the requested premium target, not earlier handoff claims. NV = not verified.

| Area | Baseline /10 | Evidence / limitation |
|---|---:|---|
| Player models | 3 | Exposed skin, primitive kit |
| Player proportions | 5 | Plausible height, crouched silhouette |
| Batting animation | 2 | Procedural; no authored drive |
| Bowling animation | 2 | Procedural arm arc / weak momentum |
| Locomotion | 3 | Procedural cadence, sliding risk |
| Fielding animation | 2 | Early ball stop, generic hands |
| Ball physics | 6 | Analytic trajectory; unit tests pass |
| Bat-ball contact | 3 | No rendered geometry assertion |
| Pitch | 5 | Correct proportions, too uniform |
| Outfield | 5 | Green/readable, repetitive bands |
| Stadium | 4 | Repetitive flat structural detail |
| Crowd | 3 | Regular rows and identical silhouettes |
| Lighting | 5 | Readable; weak night identity |
| Shadows | 5 | Visible grounding, helmet disconnect |
| Camera | 5 | Striker feet cropped in live batting |
| Replay | 4 | Close view exposes pose/equipment errors |
| UI | 7 | Clear identity; overlays use large area |
| Sound | NV | Existing cues; baseline capture muted |
| Responsiveness | NV | Manual/device test outstanding |
| Tactile feedback | NV | Needs supported phone |
| Mobile performance | NV | No phone measurements |

Golden Delivery is NOT accepted merely because automation passes.

## 2026-09-09, kit and drive pose — `gate_kit2` (Claude Opus 5)

Evidence: `Artifacts/Captures/gate_kit2/`, inspected with `Tools/CropFrame.py`. Only areas this
session actually touched are re-scored; everything else keeps the baseline number above.

| Area | Baseline /10 | Now /10 | What changed, and what is measurable |
|---|---:|---:|---|
| Player models | 3 | 5 | Gloves have finger rolls, pads have bolsters and wings, helmet has ear/nape coverage. Torso is still one smooth volume. |
| Batting animation | 2 | 4 | Pelvis transfers forward onto the striding foot, front knee takes the load, back heel rolls onto its toe, chest goes over the ball. Still procedural, not authored. |
| Bat-ball contact | 3 | 7 | `C26_GATE_CONTACT gap_cm=0.523` against the generated blade triangles, at `Z=-57.9` on an 83 cm bat — the widest point of the willow. Was -75.7, the toe. |
| Locomotion | 3 | 4 | Lifted feet roll onto the toe instead of hanging flat; applies to the run-up as well as the drive. |
| Bowling animation | 2 | 3 | Approach has stride length, knee drive, tucked arm drive and lean. The gather is still only 0.31 s and the action is still procedural. |
| Replay | 4 | 5 | The close-up now survives magnification: hands, pads and blade all read. The head does not. |

Not re-scored and still unverified: sound, responsiveness, tactile feedback, mobile performance.
No measurement exists on a phone. The desktop editor frame time for a delivery is mean 20.4 ms,
p95 22.3 ms with screenshots enabled, which is not a device claim.

