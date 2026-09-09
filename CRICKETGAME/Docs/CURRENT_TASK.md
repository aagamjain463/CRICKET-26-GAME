# Current task

Kit readability pass, branch `work/match-world-reborn`.

## State

Head/face and torso are done and green. `Tools/GoldenGate.sh kit_readability5`
passes (`C26_GATE_PASS failures=0`, release 0.000 cm, contact 0.523 cm at
Z=-57.9 on the 83 cm blade), automation 3/3 PASS
(`Cricket26.Rules.SuperOver`, `Cricket26.Simulation.GoldenDelivery`,
`Cricket26.Simulation.Trajectories`), full-match smoke in progress
(`Artifacts/smoke_kit.log`, 7+ matches clean before the operator timeout
killed the wrapper mid-match-8, re-launched in background).

What changed, all in `Source/CRICKETGAME/SuperOver/C26Athlete.cpp`
(rules/scoring untouched) plus a unity-build `-Wshadow` fix in
`Tests/C26GoldenGate.cpp` (`Striker` collided with the anonymous-namespace
`Striker` in `C26CameraDirector.cpp`; renamed `StrikerPos`):

- `Skin` MID (M_Surface, mid-brown, roughness .62) overrides the `Bodymat`
  slots. The imported skin response went near-black on vertical surfaces
  under the night rig; forearms, neck and face now read at every distance.
  Eyes share the slot and take the same tone at this fidelity.
- `Peak`/`Shell`/`Grill` no longer cast shadows. Headwear sat millimetres
  from the face and threw the whole head into shadow.
- `Uniform` sections 3/4/5: collar band off the neck bone, chest placket,
  hem band off the pelvis. All dimensions derive from posed bones.
- Trousers: hip 10.4 -> 11.6 with top ring raised, mid-thigh 9.1 -> 10.2,
  knee 7.8 -> 8.4. The imported thighs are heavier than a tailor's chart
  and skin poked through once it turned bright.
- Pads: mouth stops just above the knee (21.5) and runs wide (10.5);
  a short lit flare (section 1, Gear) rolls the mouth so the replay camera
  meets binding instead of unlit tube. A tall narrow mouth climbed into the
  bent thigh and gaped; a flat annulus seal sat in shadow as a black disc.
  A thin dark crescent can survive at extreme grazing angles and reads as
  strap, not hole.

Cascadeur MCP verified live (`127.0.0.1:8765` answers, `{"error": "Use POST
/mcp or POST /run"}`) but deliberately unused here: this pass is skinned
cloth/kit, where procedural overlays track the pose without touching skin
weights. Blender 5.2.1 MCP verified connected; used for measurement
(`C26_KitBase` scene: `Tops.001` 2183 verts) rather than re-export, for the
same reason. No Meshy usage.

## Next, in order

1. **Confirm smoke 10/10** in `Artifacts/smoke_kit.log`, then commit.
2. **The gather is 0.31 s long.** `C26Field::ReleasePoseTime` and the
   bowling arm arc are derived from each other; lengthening the gather
   means re-deriving both together. Cascadeur is available for cleaning
   the gather arc if the motion is authored there first and release sync
   re-verified with the gate. Do it deliberately and re-run the gate.
3. **Outfield and stadium.** Only after the athletes stop being the weakest
   thing on screen.

Do not expand the stadium, the crowd or the shot library. Premium authored
or captured cricket motion is still an open asset gate: the stroke is
procedural and refined, not mocap.
