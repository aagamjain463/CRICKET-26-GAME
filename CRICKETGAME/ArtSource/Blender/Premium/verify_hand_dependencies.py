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
                        ('cover-contact', actions.COVER_DRIVE['keys'][4][1]),
                        ('cover-through', actions.COVER_DRIVE['keys'][5][1])]:
    for hand, spec in [('R', original), ('L', rig_lib.mirror(original))]:
        rig_lib.apply(rig, spec)
        targets = rig_lib.apply.last_targets
        assert {'hand_l', 'hand_r'} <= targets.keys(), (label, hand, 'unsolved wrist')
        distances = [(rig.pose.bones[n].head - targets[n]).length for n in ('hand_l', 'hand_r')]
        assert max(distances) < 2, (label, hand, distances)
        results.append({'pose': label, 'handedness': hand, 'target_error_cm': distances})
(ROOT / 'Artifacts/CharacterAudit/hand-dependency-test.json').write_text(json.dumps(results, indent=2))
print('C26_HAND_DEPENDENCIES_PASS', len(results))
