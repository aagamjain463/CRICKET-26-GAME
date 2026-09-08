# CRICKET 26 — Super Over

Engine: installed Unreal 5.8.2. Project/module: CRICKETGAME. No engine upgrade.

The original ThirdPerson, Combat, Platforming and SideScrolling templates are preserved. Cricket code and assets live under SuperOver and /Game/Cricket26.

Authority flows from the deterministic rules model, through the match coordinator, to the world simulation. Presentation and input never write scores. Each delivery has a monotonically increasing ID and commits once. A restart resets rules, simulation, AI, actors, replay and camera together without level travel.

The venue uses centimetres: wickets 2,012 cm apart, popping creases 122 cm in front, stumps 71.1 cm tall, ball radius 3.6 cm. The rope and boundary adjudication share the same polygon.

The mobile baseline is mobile deferred, conventional shadow maps, no dependency on Nanite, ray tracing or Lumen. Quality tiers control resolution, shadows, crowd and effects. Desktop measurements are not phone performance claims.

Presentation references (composition only): https://www.youtube.com/watch?v=hZ6Jpf2i1cw and https://www.youtube.com/watch?v=UnNLZzDEZgc. No commercial cricket content is extracted.
