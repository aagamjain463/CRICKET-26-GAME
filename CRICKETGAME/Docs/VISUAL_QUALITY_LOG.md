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

## 2026-09-09, kit readability — `kit_readability5` (Muse Spark)

Evidence: `Artifacts/Captures/kit_readability5/`, inspected with `Tools/CropFrame.py`
at 2-4x on `0_10_replay_contact.png`. Only areas this session touched are
re-scored; everything else keeps its earlier number. Gate identical to baseline
(release 0.000 cm, contact 0.523 cm at Z=-57.9), automation 3/3 PASS.

| Area | Before /10 | Now /10 | What changed, measurably |
|---|---:|---:|---|
| Player models | 5 | 7 | Skin reads mid-brown at all distances (was near-black mass); collar, placket and hem band break the balloon torso; pad mouths carry lit binding. |
| Replay | 5 | 6 | Close-up survives magnification: face, forearms, gloves, pads, blade all read. Pad mouth can keep a thin dark crescent at grazing angles (reads as strap). Shirt back is still one smooth volume behind the number. |
| Shadows | 5 | 6 | Face self-shadow removed (headwear no longer casts); pad-mouth shadow replaced with lit flare geometry. |

Still procedural, not authored: batting, bowling, locomotion, fielding scores
unchanged. Desktop editor delivery frame time mean 21.3 ms, p95 25.7 ms with
screenshots on (+3 sections/athlete, ~600 tris) — not a device claim.


## 2026-09-09, Milestone 2 next-gen cricketers — `m2_final` (Claude Opus 5)

Evidence: `Artifacts/Captures/m2_final/` and `Artifacts/Captures/m2_kit2/`, inspected with
`Tools/CropFrame.py` at 2-3x. Gate `C26_GATE_PASS failures=0`, contact 1.200 cm at Z=-57.9 against
724 authored blade triangles, automation 3/3 PASS. Only areas this session touched are re-scored.

| Area | Before /10 | Now /10 | What changed, measurably |
|---|---:|---:|---|
| Player models | 7 | 8 | Every piece of equipment is authored geometry (9,984 tris across 10 meshes) instead of C++ ring-lofts. Torso is still the base character's street top. |
| Cricket bat | 3 | 8 | 1,620-tri willow with a real blade profile: parallel edges, 10.8 cm width, 3.8 cm blunt edges, spine ridge, definite shoulders, splice binding, cane handle, banded rubber grip. Was a 10-sided ellipse. |
| Helmet | 5 | 8 | Shell with a rim that sweeps up over the face and down over the ears and nape, downturned peak with thickness and trim, crown crest, and a 5-bar round-section titanium grille with side stems. Was an engine sphere plus a squashed sphere. |
| Pads | 6 | 8 | Three vertical bolsters, knee roll, side wings, three straps that follow the bolster surface, buckles, instep flap. Unmistakable at gameplay distance. |
| Gloves | 5 | 7 | Four waisted finger rolls arcing over the knuckles, moulded thumb guard, cuff band and wrist strap. |
| Shoes | 2 | 7 | Authored cricket shoe: white upper, dark midsole, team flash, lace bands, six studs. Replaces the base character's dark street trainers, which read as two black holes at the point of ground contact. |
| Kit readability | 6 | 8 | Cricket whites. Team-coloured trousers made the striker one teal mass; cream trousers separate legs from torso and give the white pads something to sit against. |
| Materials | 5 | 7 | 21 distinct kit instances with separated roughness (helmet shell .20, blade .42, pad face .76, grip .88) instead of one grey "Gear" shared by pads and gloves. |
| Batting stance | 4 | 6 | Knees genuinely loaded (crouch -13 to -19), wider base, more side-on. Still procedural. |
| Bowling presentation | 3 | 5 | The bowler at his mark now has his own pose -- tall, square, ball in both hands at chest, weight rocking -- instead of sharing the crouched fielder idle. |
| Mobile performance | NV | NV | Desktop delivery frame time mean 21.3 -> 17.74 ms, p95 25.7 -> 16.67 ms, because the per-frame procedural equipment rebuild is gone. Not a device claim. |

Not re-scored: pitch, outfield, stadium, crowd, lighting, camera, UI, sound, responsiveness,
tactile feedback. No measurement exists on a phone.

Blender was used for all ten equipment meshes. Cascadeur MCP was verified live
(`127.0.0.1:8765` answers `tools/list`) and deliberately unused: this pass is geometry and
material work, and the free tier cannot export the authored motion that would justify it.
