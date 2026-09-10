# CRICKET 26 — Audio Architecture (Commentary + Stadium Overhaul)

## Flow

```
MATCH / GAMEPLAY (authoritative, owns all truth)
  StartMatch / PrepareDelivery / ReleaseBall / contact / keeper take
  BreakWicket / Collect / Resolve / AfterPresentation / Skip / Menu
│  exactly one audio notify per event, built from C26::Match + FC26Simulation
▼
UC26Audio — Match Audio Director (Source/CRICKETGAME/SuperOver/C26Audio.*)
├── CommentaryManager  event queue, priority, cooldowns, follow-ups, subtitles
├── CrowdDirector      persistent bed + tension layer + reaction overlays
└── CricketSFXDirector hero transients, spatialized broadcast field mics
│  per-bus volumes × Master, commentary ducking of beds only
▼
Output (2D broadcast commentary/UI/crowd beds, 3D field transients)
```

Audio observes `FC26CommentaryContext` snapshots. It never writes match truth.

## CommentaryManager

- **Data**: `Tools/CommentaryScript.py` (118 original lines) → `C26CommentaryData.inc`
  (generated, compiled in) → `/Game/Cricket26/Audio/Commentary/*.uasset`.
  Stable IDs (`Commentary.Boundary.Four.003`); gameplay only sends categories.
- **Selection**: category filter → cooldown filter (`LastUsedBall`, per-line
  `Cooldown` in balls) → specificity boost (consecutive-boundary lines ×5) →
  weighted random → queue-by-priority.
- **Priorities**: result 100 > wicket 90 > final-ball 85 > six 80 > four 70 >
  edge/delivery 42–60 > runs 40–44 > dot 30 > analysis 25–28 > pre-ball 20–24.
- **Queue**: one voice at a time; +15 priority interrupts; near-priority queues
  (max 2, weakest replaced); lower drops. 0.35 s gap between lines.
  Result lines queue *behind* an in-flight boundary call instead of cutting it.
- **Follow-ups**: 35% after big moments (same-family analyst line), 18% otherwise.
- **Frequency gates**: pre-ball 45% (70% pressure), delivery 50% (80% beaten),
  dot 55%, runs 70%, boundaries/wickets/results always.
- **Fallbacks**: BOWLED/CAUGHT → WICKET; FINAL_BALL → PRESSURE; specific → generic.
- **Voices**: A play-by-play (Daniel en_GB), B analyst (Samantha en_US).
  Voice pitch never varied; subtitles mirror the active line with its duration.

## CrowdDirector

- One looping `crowd_ambience` bed + one `crowd_anticipation` tension layer.
  Never restarted between balls; `Reset()` guarantees exactly one instance.
- States: Calm .22 / Anticipation .34 / Excited .5 / Boundary .62 / Six .78 /
  Wicket .85 / Tense .5 / Win 1.0 (sustained) / Loss .3. Reactions decay back.
- One-shots (`crowd_four/six`, `wicket_roar`) layer over the bed per event.

## CricketSFXDirector

- 2D pool (8): crowd reactions, UI, stings. 3D field pool (6) with a
  stadium-scale attenuation (11 km sphere, −18 dB max): spatialized but always
  audible on broadcast cameras — the field-mic effect.
- Four bat voices from authoritative contact: `bat_sweet_spot` (Perfect),
  `bat_defensive` (Good/defence), `bat_mistimed` (Early/Late), `bat_edge`.
  Never on a miss. Sub-60 ms duplicate guard on every transient.
- Footsteps distance-based (≈95 cm stride), front-foot `foot_plant`,
  `ball_release` at the hand, `ball_bounce` at the simulated impact,
  `keeper_catch` at the gloves, `stump_hit` at the wicket, `fielder_gather`.

## Mix

Code buses (authoritative, persisted in `UC26Settings`):
MASTER (`SoundVolume`) × COMMENTARY (.95) / CROWD (.85) / SFX (.9) /
MUSIC (.5) / UI (.8). Commentary ducks beds to 60% (fast attack, slow
release); transients are never ducked. All dev VO peak-normalized to 0.89
with 4 ms edge fades. Imported VO compresses to BINKA (~12 MB → ~2 MB).

`Content/Cricket26/Audio/Mix/` SoundClasses were attempted via the import
script but the headless Python path did not persist them; creating
SC_Master/SC_Commentary/SC_Crowd/SC_OnField/SC_UI/SC_Music in the Content
Browser and assigning waves is a 5-minute optional upgrade — the code buses
already implement the hierarchy.

## Gameplay hooks (all in C26MatchGameMode.cpp)

StartMatch→MatchStart · PrepareDelivery→ChaseStart/FinalBallPre/PreBall ·
contact→Delivery analysis + 4-way bat · bounce/keeper/stumps/gather/throw→CueAt ·
Resolve→Wicket XOR Result + NoteBallCompleted (streaks) ·
AfterPresentation→InningsBreak / MatchResult · Menu/StartMatch→Reset.

## Debug

`C26Force("c six|four|wicket|dot|finalball|...")` plays a category through the
real queue; `C26Force("c dump")` logs crowd/queue/cooldown/subtitle state.

## Replacing dev VO (production path)

1. Record/license premium lines. 2. Export 24 kHz mono WAV with the
STABLE filename (`Commentary_Six_001.wav`, …). 3. Run
`Tools/ImportOverhaulAudio.py`. No gameplay or selection code changes.

## Verification (2026-09-10)

`C26Smoke` 10 matches: PASS. Bank 118/118, 259 commentary plays,
0 cooldown violations, 0 invalid transitions, 0 rejected outcomes.
CHASE_START 10/10, result lines 10/10.
