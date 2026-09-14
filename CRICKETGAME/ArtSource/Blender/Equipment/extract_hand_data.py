"""Round-2 measurement: posed hand geometry in each glove's measured frame.

Round-1 placement (BATTER_READY_R + batter-offsets.json) is READ, never written.
Prints finger segment endpoints, skin radii, palm widths and the bat-handle axis
in glove-local coordinates, so the v002 glove is authored around the real grip.
"""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'ArtSource/Blender/Premium'))
import c26_rig as rig_lib
import c26_actions as actions

OFFSETS = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-offsets.json').read_text())

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=True)
rig_lib.apply(rig, actions.BATTER_READY_R)
print('ARMATURE:', rig.name)
print('MESHES:', sorted(o.name for o in bpy.data.objects if o.type == 'MESH'))


def offset_matrix(entry):
    cols = [Vector(entry['axes'][k]) for k in 'xyz']
    m = cols[0].to_track_quat('X', 'Z').to_matrix().to_4x4()  # placeholder, replaced below
    from mathutils import Matrix
    m = Matrix.Identity(4)
    for i, c in enumerate(cols):
        m.col[i].xyz = c
    m.translation = Vector(entry['loc'])
    return m


for side, gkey in (('l', 'BattingGloveL'), ('r', 'BattingGloveR')):
    print('=' * 70)
    print('HAND', side)
    hand = rig.pose.bones['hand_' + side]
    glove = rig.matrix_world @ hand.matrix @ offset_matrix(OFFSETS[gkey])
    to_local = glove.inverted()
    print('glove origin world:', [round(float(v), 2) for v in glove.translation])
    fingers = [b.name for b in rig.pose.bones
               if any(k in b.name for k in ('thumb', 'index', 'middle', 'ring', 'pinky'))
               and b.name.endswith('_' + side) and 'metacarpal' not in b.name
               and 'tip' not in b.name and 'nail' not in b.name]
    print('finger bones:', sorted(fingers))
    for name in sorted(fingers):
        pb = rig.pose.bones[name]
        a = to_local @ (rig.matrix_world @ pb.head)
        b = to_local @ (rig.matrix_world @ (pb.head + pb.tail - pb.head))
        tail = to_local @ (rig.matrix_world @ pb.tail)
        print('  %-18s head %s tail %s len %.2f' % (
            name, [round(float(v), 2) for v in a], [round(float(v), 2) for v in tail],
            (tail - a).length))
    for probe in ('hand_' + side, 'lowerarm_' + side, 'middle_01_' + side):
        if probe in rig.pose.bones:
            w = to_local @ (rig.matrix_world @ rig.pose.bones[probe].head)
            print('  %-18s %s' % (probe, [round(float(v), 2) for v in w]))
print('C26_HAND_DATA_DONE')
