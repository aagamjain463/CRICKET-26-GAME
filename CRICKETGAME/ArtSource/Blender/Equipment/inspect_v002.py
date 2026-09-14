"""Round-2 BEFORE inspection: current glove/shoe geometry + assembled bat grip.

Loads the Round-1 review rig at BATTER_READY_R (untouched), places the current
bat + gloves with the measured batter-offsets.json matrices, and renders:
  - glove L/R turntables (isolated geometry, 3 angles each)
  - assembled two-handed grip close-ups (3 angles)
  - shoes as fitted in C26_Athlete_Review.blend (front/side/rear, standing feet)
All renders go to Artifacts/CharacterAudit/equip_before/. Nothing is modified.
"""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(ROOT / 'ArtSource/Blender/Premium'))
sys.path.insert(0, str(ROOT / 'ArtSource/Blender'))
import c26_build
import c26_rig as rig_lib
import c26_actions as actions

OUT = ROOT / 'Artifacts/CharacterAudit/equip_before'
OUT.mkdir(parents=True, exist_ok=True)
EXPORTS = ROOT / 'ArtSource/Exports/Equipment'
OFFSETS = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-offsets.json').read_text())

CM_EXPECT = {'SM_C26_Glove_L': 19.0, 'SM_C26_Glove_R': 19.0,
             'SM_C26_Shoe_L': 27.5, 'SM_C26_Shoe_R': 27.5,
             'SM_C26_Bat_Hero': 87.3}


def import_cm(path):
    """Import an equipment FBX and return meshes with geometry in cm numbers."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    objs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    assert objs, path
    dims = [max(o.dimensions) for o in objs]
    if max(dims) < 2.0:
        for o in objs:
            o.scale = (100.0, 100.0, 100.0)
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return objs


def offset_matrix(entry):
    m = Matrix.Identity(4)
    m.translation = Vector(entry['loc'])
    cols = [Vector(entry['axes'][k]) for k in 'xyz']
    for i, c in enumerate(cols):
        m.col[i].xyz = c
    return m


def place(rig, bone_name, entry, objs):
    pb = rig.pose.bones[bone_name]
    m = rig.matrix_world @ pb.matrix @ offset_matrix(entry)
    for o in objs:
        o.matrix_world = m
    bpy.context.view_layer.update()


def report(objs, label):
    for o in objs:
        tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
        d = sorted(o.dimensions, reverse=True)
        print('BEFORE %-18s verts %5d tris %5d dims %s slots %s' % (
            label, len(o.data.vertices), tris,
            [round(float(x), 2) for x in d], [m.name for m in o.data.materials]))


# ---- 1. isolated turntables of the current glove/shoe exports
c26_build.clear_scene()
iso = {}
for name in ('SM_C26_Glove_L', 'SM_C26_Glove_R', 'SM_C26_Shoe_L', 'SM_C26_Shoe_R'):
    iso[name] = import_cm(EXPORTS / (name + '.fbx'))
    report(iso[name], name)
for name, objs in iso.items():
    for i, az in enumerate((38, 158, 278)):
        c26_build.studio(objs, str(OUT / ('%s_iso_%d.png' % (name, i))),
                         azimuth=az, elevation=16, res=(700, 700))

# ---- 2. assembled grip: Round-1 stance + measured offsets, Round-1 placement
rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=True)
rig_lib.apply(rig, actions.BATTER_READY_R)
bat = import_cm(EXPORTS / 'SM_C26_Bat_Hero.fbx')
report(bat, 'SM_C26_Bat_Hero')
gl = import_cm(EXPORTS / 'SM_C26_Glove_L.fbx')
gr = import_cm(EXPORTS / 'SM_C26_Glove_R.fbx')
place(rig, 'hand_l', OFFSETS['Bat'], bat)
place(rig, 'hand_l', OFFSETS['BattingGloveL'], gl)
place(rig, 'hand_r', OFFSETS['BattingGloveR'], gr)
hands = [o for o in bpy.data.objects if o.type == 'MESH'
         and o.name.lower().startswith(('body', 'c26_body', 'face'))]
print('BODY MESHES:', [o.name for o in hands])
grip_objs = bat + gl + gr
for o in bpy.data.objects:
    if o.type == 'MESH' and o not in grip_objs:
        o.hide_render = True
for i, (az, el) in enumerate(((35, 12), (150, 18), (265, 30))):
    c26_build.studio(grip_objs, str(OUT / ('grip_assembled_%d.png' % i)),
                     azimuth=az, elevation=el, res=(900, 900), pad=1.05, key=5.0)
for o in bpy.data.objects:
    if o.type == 'MESH':
        o.hide_render = False

# ---- 3. shoes as fitted on the standing review body (front/side/rear)
shoes = [o for o in bpy.data.objects if o.type == 'MESH' and 'Shoe' in o.name]
print('FITTED SHOES:', [(o.name, [round(float(x), 1) for x in sorted(o.dimensions, reverse=True)]) for o in shoes])
for o in bpy.data.objects:
    if o.type == 'MESH' and o not in shoes:
        o.hide_render = True
for i, (az, el) in enumerate(((90, 8), (0, 8), (270, 8))):
    c26_build.studio(shoes, str(OUT / ('shoes_fitted_%d.png' % i)),
                     azimuth=az, elevation=el, res=(900, 900), pad=1.15, key=5.0)
print('C26_EQUIP_BEFORE_DONE ->', OUT)
