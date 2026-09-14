"""Check finger bone connectivity/scales in the posed review rig (read-only)."""
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'ArtSource/Blender/Premium'))
import c26_rig as rig_lib
import c26_actions as actions

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=False)
rig_lib.apply(rig, actions.BATTER_READY_R)
for side in ('l', 'r'):
    print('SIDE', side)
    for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
        chain = []
        for seg in ('01', '02', '03'):
            n = '%s_%s_%s' % (f, seg, side)
            if n not in rig.pose.bones:
                chain.append(n + '=MISSING')
                continue
            pb = rig.pose.bones[n]
            par = pb.parent.name if pb.parent else None
            chain.append('%s(par=%s,conn=%s,scale=%s)' % (
                n, par, pb.bone.use_connect,
                [round(float(v), 3) for v in pb.scale]))
        print('  ', ' '.join(chain))
    h = rig.pose.bones['hand_' + side]
    print('   hand scale:', [round(float(v), 3) for v in h.scale],
          'loc:', [round(float(v), 2) for v in h.location])
print('C26_CHAIN_DONE')
