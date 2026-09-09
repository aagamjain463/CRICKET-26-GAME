# Current task

Golden Delivery presentation, branch `work/golden-delivery-contact`.

## State

The technical gate is closed and green. `Tools/GoldenGate.sh <label>` renders
drive -> miss -> restart -> drive and asserts against the rendered frame:
release on the hand (0.000 cm), ball on the blade (0.523 cm gap), contact 57.9 cm
down an 83 cm bat (the widest point of the willow), one score commit per delivery,
clean epoch after restart. Run it after any change to the athlete, the camera,
the simulation or the match flow. `C26_GATE_PASS failures=0` is the bar.

## Next

Character fidelity is now the visible bottleneck, in this order:

1. **Torso silhouette.** `SK_Cricketer_KitBase` renders the shirt as one smooth
   volume; the near arm merges into the chest at replay distance. Needs sleeve
   and shoulder separation, not more polygons.
2. **Head and face.** The head is a dark mass under the helmet at any distance
   closer than the broadcast lens. The grille resolves; the face does not.
3. **Bowler run-up weight.** The toe-off roll now applies to every lifted foot,
   so the run-up should be re-inspected in `02_runup` for knee drive and lean.

Do not expand the stadium, the crowd or the shot library. Premium authored or
captured cricket motion is still an open asset gate: the stroke is procedural
and refined, not mocap.
