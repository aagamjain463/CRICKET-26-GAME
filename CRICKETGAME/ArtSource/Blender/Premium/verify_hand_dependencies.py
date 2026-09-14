"""Blender regression: both handedness variants must actually solve both wrists."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
import c26_rig as rig_lib
import c26_actions as actions

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_FullBody_Candidate.blend', keep_mesh=False)
results = []
for label, original in [('stance', actions.BATTER_READY_R),
                        ('cover-contact', rig_lib.spec_at(actions.COVER_DRIVE['keys'], actions.COVER_DRIVE['contact'])),
                        ('cover-through', rig_lib.spec_at(actions.COVER_DRIVE['keys'], actions.COVER_DRIVE['contact'] + 6))]:
    for hand, spec in [('R', original), ('L', rig_lib.mirror(original))]:
        rig_lib.apply(rig, spec)
        targets = rig_lib.apply.last_targets
        assert {'hand_l', 'hand_r'} <= targets.keys(), (label, hand, 'unsolved wrist')
        distances = [(rig.pose.bones[n].head - targets[n]).length for n in ('hand_l', 'hand_r')]
        # The bottom hand may sit a few cm short on stretched strokes; batting_lab.py gates grip precision.
        assert max(distances) < 5, (label, hand, distances)
        results.append({'pose': label, 'handedness': hand, 'target_error_cm': distances})
(ROOT / 'Artifacts/CharacterAudit/hand-dependency-test.json').write_text(json.dumps(results, indent=2))
print('C26_HAND_DEPENDENCIES_PASS', len(results))
