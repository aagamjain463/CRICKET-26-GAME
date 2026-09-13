"""Per-shot contact deltas: where the hands travel from stance to contact.

The runtime warp aligns authored contact hands with the sim contact point. It
must use the CONTACT hand position, not the stance one: the stride/swing itself
covers ~70cm, and warping from the stance double-counts it (measured: clamped
70cm overshoot, gap worsened 67->82cm). D is in armature centimetres.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
import c26_rig as rig_lib
import c26_actions as actions

OUT = ROOT / 'Artifacts/CharacterAudit/contact-deltas.json'

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=False)


def midpoint(spec):
    rig_lib.apply(rig, spec)
    return (rig.pose.bones['hand_l'].head + rig.pose.bones['hand_r'].head) * 0.5


out = {}
for hand, ready in [('R', actions.BATTER_READY_R),
                    ('L', rig_lib.mirror(actions.BATTER_READY_R))]:
    m0 = midpoint(ready)
    for shot in actions.SHOTS:
        keys = {f: s for f, s in shot['keys']}
        contact = shot['keys'][4][1]
        if hand == 'L':
            contact = rig_lib.mirror(contact)
        m1 = midpoint(contact)
        d = m1 - m0
        out[f"{shot['name']}_{hand}"] = [round(v, 3) for v in d]
OUT.write_text(json.dumps(out, indent=1))
print('C26_CONTACT_DELTAS', len(out))
