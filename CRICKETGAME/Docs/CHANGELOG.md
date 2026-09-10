# CRICKET 26 — Changelog

## 2026-09-10 — Complete commentary + stadium audio overhaul

- Event-driven commentary: 118 original lines, 2 voices, priority queue,
  cooldowns, follow-ups, subtitles, pressure/final-ball/result awareness.
- Living crowd: persistent bed + tension layer + reaction overlays + ducking.
- Cricket SFX: 4-way bat contact, spatialized bounce/stumps/keeper/footsteps.
- Mix buses: master/commentary/crowd/SFX/music/UI volumes, persisted.
- Verified: build succeeds, 10-match smoke PASS, 259 plays, 0 violations.
- See `Docs/AUDIO_ARCHITECTURE.md`, `Docs/AUDIO_LICENSES.md`.

## 2026-09-10 — Complete professional front-end and match HUD overhaul

- New front end: Home, Play browser, team select, matchup, cinematic toss,
  6 future hubs (honest PREVIEW states), categorized settings, visual guide.
- New match HUD: broadcast score bug, strips, restyled controls, animated
  callouts, replay tag, interval card, result + real-data summary, pause hub
  with confirms, subtitle width cap, subtitles/reduced-motion settings.
- Design system: tokens, tracked kickers, ghost type, corner-cut CTAs,
  scrims, toasts, motion language. 25-beat screenshot suite, 16:9 + 19.5:9.
- See `Docs/UI_DESIGN_SYSTEM.md`.

## 2026-09-10 — UI premium pass: overlap purge + broadcast-noir grade

- Rewrote all screens on cursor-stacked layout with measured widths; fixed
  every reported text/element collision (hero, score bug, pause, result).
- New look: cinematic vignette grade, glass panels with hairline borders,
  shield crests with initials, ghost numerals, tracked labels, corner-cut
  CTAs. Verified 25/25 beats, full match, zero errors.
