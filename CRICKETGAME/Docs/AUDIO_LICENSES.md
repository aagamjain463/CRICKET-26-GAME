# CRICKET 26 — Audio Licenses

## Development commentary voice (118 lines)

- **Source**: locally synthesized on the dev Mac with the system `say` tool.
  Voices: Daniel (en_GB, play-by-play) and Samantha (en_US, analyst).
  Script: `Tools/GenerateDevelopmentCommentary.py` from
  `Tools/CommentaryScript.py`.
- **Text**: 100% ORIGINAL lines written for CRICKET 26. No broadcast,
  game, YouTube, or published commentary copied, transcribed, or imitated.
  No real commentator, athlete, or celebrity voice cloned or imitated.
- **Status**: ORIGINAL DEVELOPMENT TTS — REPLACEABLE. Neutral system voices
  used as a functional stand-in; replace with premium recorded/licensed VO
  per `Docs/AUDIO_ARCHITECTURE.md` without touching gameplay code.
- **Attribution**: none required (original content, system tool output).

## Supplemental Foley (bat_mistimed, foot_plant)

- **Source**: deterministic local synthesis (`Tools/BuildOverhaulSFX.py`,
  fixed seed, no samples or recordings). Original work.
- **Status**: ORIGINAL — may ship or be replaced freely. No attribution.

## Pre-existing audio uassets (crowd beds, bat/ball/stump/keeper/UI)

- Present in `Content/Cricket26/Audio/` before this overhaul; imported from
  an external `SOURCE/Audio/Clips` path not in this repository.
- **Status**: UNVERIFIED provenance — before shipping, confirm these are
  original/licensed; otherwise replace with original recordings or extend
  the deterministic synthesis in `Tools/BuildMatchAudio.py` /
  `Tools/BuildOverhaulSFX.py`. No new third-party audio was added by this
  overhaul.

## Reference video

`https://www.youtube.com/watch?v=hZ6Jpf2i1cw` used as a QUALITY/PRESENTATION
reference only. Nothing copied, transcribed, ripped, or imitated.
