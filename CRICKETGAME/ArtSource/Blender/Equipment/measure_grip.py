"""Round-2 grip measurement: finger polylines, skin radii, handle axis, palm size.

Reads Round-1 placement only. Writes Artifacts/CharacterAudit/grip-measure.json
consumed by the v002 glove authoring script. All values in glove-local cm.
"""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'ArtSource/Blender/Premium'))
import c26_rig as rig_lib
import c26_actions as actions

OFFSETS = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-offsets.json').read_text())
EXPORTS = ROOT / 'ArtSource/Exports/Equipment'
OUT = ROOT / 'Artifacts/CharacterAudit/grip-measure.json'


def offset_matrix(entry):
    m = Matrix.Identity(4)
    for i, k in enumerate('xyz'):
        m.col[i].xyz = Vector(entry['axes'][k])
    m.translation = Vector(entry['loc'])
    return m


rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=True)
rig_lib.apply(rig, actions.BATTER_READY_R)

# Posed skin verts (evaluated) for the hand regions, in rig-local cm.
deps = bpy.context.evaluated_depsgraph_get()
body = next(o for o in bpy.data.objects if o.type == 'MESH' and 'BodyMesh' in o.name)
ev = body.evaluated_get(deps)
mesh = ev.to_mesh()
skin_world = [(ev.matrix_world @ v.co) for v in mesh.vertices]
ev.to_mesh_clear()

data = {}
for side, gkey in (('l', 'BattingGloveL'), ('r', 'BattingGloveR')):
    hand = rig.pose.bones['hand_' + side]
    glove = rig.matrix_world @ hand.matrix @ offset_matrix(OFFSETS[gkey])
    to_local = glove.inverted()
    W = lambda p: to_local @ (rig.matrix_world @ p)  # noqa
    skin_local = [to_local @ w for w in skin_world]

    fingers = {}
    for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
        if f == 'thumb':
            joints = ['thumb_01_' + side, 'thumb_02_' + side, 'thumb_03_' + side]
        else:
            joints = [f + '_metacarpal_' + side, f + '_01_' + side, f + '_02_' + side, f + '_03_' + side]
        pts = [W(rig.pose.bones[j].head) for j in joints]
        # fingertip: extend last segment by distal length
        d = (pts[-1] - pts[-2])
        tip = pts[-1] + d.normalized() * d.length * 0.9
        poly = [tuple(round(float(v), 3) for v in p) for p in pts + [tip]]
        # skin radius per segment: 90th percentile radial distance of skin verts
        # sliced around the segment (robust to palm/neighbour-finger verts).
        radii = []
        for a, b in zip(pts, pts[1:] + [tip]):
            a, b = Vector(a), Vector(b)
            ab = b - a
            dists = []
            for s in skin_local:
                sv = Vector(s) - a
                t = sv.dot(ab) / max(ab.length_squared, 1e-9)
                if -0.15 <= t <= 1.15:
                    d = (sv - ab * max(0.0, min(1.0, t))).length
                    if d <= 1.05:
                        dists.append(d)
            r = max(dists) if dists else 0.8
            radii.append(round(min(max(r, 0.6), 2.2), 3))
        fingers[f] = {'poly': poly, 'skin_radii': radii}
    # palm frame landmarks
    wrist = W(hand.head)
    knuckles = [Vector(fingers[f]['poly'][1 if f != 'thumb' else 0]) for f in ('index', 'middle', 'ring', 'pinky')]
    palm_w = max((k - wrist).length for k in knuckles)
    spread = max((knuckles[i] - knuckles[j]).length for i in range(4) for j in range(i + 1, 4))
    data[side] = {
        'glove_origin_world_m': [round(float(v), 4) for v in glove.translation],
        'fingers': fingers,
        'wrist_local': [round(float(v), 3) for v in wrist],
        'palm_wrist_to_knuckle': round(palm_w, 3),
        'knuckle_spread': round(spread, 3),
    }

# Bat handle axis in each glove frame: handle top + 20cm down the handle.
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(EXPORTS / 'SM_C26_Bat_Hero.fbx'))
bat = next(o for o in bpy.data.objects if o not in before and o.type == 'MESH')
if max(bat.dimensions) < 2.0:
    bat.scale = (100.0, 100.0, 100.0)
    bpy.context.view_layer.objects.active = bat
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
bpy.context.view_layer.update()
for side, gkey, bone in (('l', 'BattingGloveL', 'hand_l'), ('r', 'BattingGloveR', 'hand_r')):
    hand = rig.pose.bones[bone]
    batm = rig.matrix_world @ rig.pose.bones['hand_l'].matrix @ offset_matrix(OFFSETS['Bat'])
    if max(bat.dimensions) > 50:
        bat.matrix_world = batm
        bpy.context.view_layer.update()
    hand = rig.pose.bones['hand_' + side]
    to_local = (rig.matrix_world @ hand.matrix @ offset_matrix(OFFSETS[gkey])).inverted()
    top = to_local @ (batm @ Vector((0, 0, 3.0)))
    dn = to_local @ (batm @ Vector((0, 0, -20.0)))
    data[side]['handle_top'] = [round(float(v), 3) for v in top]
    data[side]['handle_down'] = [round(float(v), 3) for v in dn]
    data[side]['handle_radius'] = 1.85

OUT.write_text(json.dumps(data, indent=1))
print('C26_GRIP_MEASURED ->', OUT)
