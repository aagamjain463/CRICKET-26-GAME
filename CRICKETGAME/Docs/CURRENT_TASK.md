# Current task

## Just completed — bowling control loop (2026-09-12)

The bowling loop is rebuilt and proven: type → exact pitch target → movement
direction → amount → pace → run-up → release bar → actual ball. Reference:
`Docs/BOWLING_CONTROL_SYSTEM.md`.

- `Tools/BowlLab.sh` — **`C26_BOWL_PASS deliveries=39 failures=0`**, 2993 assertions,
  every delivery driven through the real pointer path.
- `Tools/BatLab.sh` — **`C26_LAB_PASS deliveries=27 failures=0`**.
- Automation — **9/9 PASS**.

Two real defects were fixed on the way: the tree did not compile
(`C26HUD.cpp::DrawBowlingTarget` read members of a deleted charge model), and reverse
swing never actually swung because its onset was resolved as seconds rather than as a
fraction of the flight.

## Open before anything else

**`Tools/GoldenGate.sh` has six failures**, and the "Green" claim in the section below
is stale because of it. All six are camera composition — `human-scale batter occupies
readable gameplay frame` and `bowler clear of top HUD`, repeated in each of the three
stages. They measure where `Athletes[]` project to screen
(`C26GoldenGate.cpp:79-81`), so they are a framing problem rather than a gameplay one,
and they postdate the last recorded passing gate (`Artifacts/gate_final.log`,
2026-09-09). Fix the framing camera before treating the gate as green again.

---

Milestone 2 — Next-Gen Cricketers, branch `work/match-world-reborn`.

## State

**Stale — see "Open before anything else" above.** As of the last Milestone 2 run:
`Tools/GoldenGate.sh m2_final` passed (`C26_GATE_PASS failures=0`,
release 0.000 cm, contact 1.200 cm at Z=-57.9 measured against **724 authored
blade triangles**), automation 3/3 PASS (`Cricket26.Rules.SuperOver`,
`Cricket26.Simulation.GoldenDelivery`, `Cricket26.Simulation.Trajectories`).
Desktop delivery frame time improved from mean 21.3 ms to **17.74 ms** because
the per-frame procedural equipment rebuild is gone.

The whole equipment layer is now authored geometry instead of C++ ring-lofts.
See `Docs/VISUAL_QUALITY_LOG.md` for the scored before/after and
`Docs/ASSET_PIPELINE.md` for how to rebuild any of it.

## Next, in order

1. **The base character's torso mesh is the last prototype-grade surface.**
   `Body` has no torso or thigh geometry at all -- Mixamo deleted everything
   under the clothes -- so a jersey cannot be derived from it, and `Bottoms`
   stops at the knee so it cannot be full cricket trousers. The shirt is still
   the base character's street top recoloured, with procedural collar, placket,
   hem and sleeve overlays on top. Fixing this properly means authoring a
   jersey and trousers as new skinned meshes with weights transferred onto the
   existing skeleton, exported as a new `SK_Cricketer_*` asset. Do not
   overwrite `SK_Cricketer_KitBase`; `Tools/ImportKit.py` already exists and
   asserts height, width and skeleton sharing against it.
2. **Equipment LODs are not built.** `Tools/ImportEquipment.py` reports
   `lods=1`: `EditorStaticMeshLibrary.set_lods` is deprecated in 5.8 and
   `StaticMeshEditorSubsystem` did not resolve in the commandlet. Detail tiers
   currently work by hiding whole components (`AC26Athlete::ApplyDetail`),
   which is effective but coarser than real LODs.
3. **The gather is still 0.31 s** and the bowling action is still procedural.
   Unchanged from Milestone 1; Milestone 3 territory.

Do not expand the stadium, the crowd or the shot library. Premium authored or
captured cricket motion is still an open asset gate.
