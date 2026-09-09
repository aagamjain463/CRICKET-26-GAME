# CRICKET 26 — Super Over

Engine: installed Unreal 5.8.2. Project/module: CRICKETGAME. No engine upgrade.

The original ThirdPerson, Combat, Platforming and SideScrolling templates are preserved. Cricket code and assets live under SuperOver and /Game/Cricket26.

## Dependency map (audited 2026-09-09)

- Transformation additions (2026-09-09):
  - EC26CameraMode: 15 distinct camera states covering broadcast framing, dynamic ball flight tracking (ground, lofted, boundary), wicket reactions, and multi-angle slow-motion replays.
  - C26Athlete: Analytical two-bone limb IK, dynamic procedural AimHead() bone orientation towards targets (ball, striker, bowler), synchronized helmet/grill orientation, stance bat tapping, and differentiated stroke mechanics (defend, cut, pull, drives).
  - C26Simulation: Continuous substepped (240Hz) contact geometry, edge categorization (EC26EdgeType), Magnus aerodynamic spin, and restitution.
  - C26Stadium: Dual directional floodlight arrays (KeyLight + CrossLight), volumetric exponential height fog (Haze), 28 mowing strips, boundary rope, and realistic crease wear.

`L_SuperOver` world settings select native `AC26MatchGameMode`; native `AC26Stadium` constructs the venue. No cricket Blueprint or custom GameState currently owns match rules.

`AC26PlayerController` -> match command methods -> `FC26Simulation` / `FC26AI` -> pending outcome -> `C26::Match::Apply` (only score writer). `AC26HUD`, `AC26CameraDirector`, `AC26Athlete`, `AC26Stadium`, and `UC26Audio` consume state. Replay is presentation playback with saved live-state restoration. `UC26Settings` persists difficulty/input/quality.

Match phases currently: Menu -> Intro -> Ready -> RunUp -> Delivery -> InPlay -> Reaction -> Replay -> next Ready / Interval -> chase -> Result. Preserve this compact authoritative state machine while expanding camera-specific modes independently.

Athlete assets: `SK_Cricketer`/Mixamo Remy, compatible A_Idle/A_Run/A_Catch/A_FielderThrow, generic textures. Specialized cricket poses are procedural. Native class interfaces must remain compatible with replay and match integration. Do not replace score logic to accommodate visual changes.

Authority flows from the deterministic rules model, through the match coordinator, to the world simulation. Presentation and input never write scores. Each delivery has a monotonically increasing ID and commits once. A restart resets rules, simulation, AI, actors, replay and camera together without level travel.

The venue uses centimetres: wickets 2,012 cm apart, popping creases 122 cm in front, stumps 71.1 cm tall, ball radius 3.6 cm. The rope and boundary adjudication share the same polygon.

The mobile baseline is mobile deferred, conventional shadow maps, no dependency on Nanite, ray tracing or Lumen. Quality tiers control resolution, shadows, crowd and effects. Desktop measurements are not phone performance claims.

Presentation references (composition only): https://www.youtube.com/watch?v=hZ6Jpf2i1cw and https://www.youtube.com/watch?v=UnNLZzDEZgc. No commercial cricket content is extracted.
