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

1. **Head and face.** The head is a dark mass under the helmet at any distance
   closer than the broadcast lens. The grille resolves; the face does not. This
   is the most obvious remaining prototype signal in `0_10_replay_contact.png`.
2. **The gather is 0.31 s long.** `C26Field::ReleasePoseTime` and the bowling
   arm arc are derived from each other: the arm reaches the top of its circle at
   `ActionTime == ReleasePoseTime` exactly, which is what keeps the ball leaving
   the hand. Lengthening the gather means re-deriving both together, and it
   touches release synchronisation, so do it deliberately and re-run the gate.
3. **Outfield and stadium.** Only after the athletes stop being the weakest
   thing on screen. The brief is explicit that a beautiful stadium around poor
   cricket motion is the wrong order.

Do not expand the stadium, the crowd or the shot library. Premium authored or
captured cricket motion is still an open asset gate: the stroke is procedural
and refined, not mocap.
