"""Refine the existing candidate, preserving its rig and original source file.

The first jersey combined overlapping MetaHuman body and facial shoulder surfaces.
Use a single connected skin surface, reshape the fabric and finish its boundaries.
This remains a review asset, never an automatic match approval.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/FullBody'
bpy.ops.wm.open_mainfile(filepath=str(OUT / 'C26_FullBody_Candidate.blend'))
rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
body = next(o for o in bpy.data.objects if o.type == 'MESH'
            and any(m and m.name.startswith('MI_Body') for m in o.data.materials))
old = bpy.data.objects['C26_Jersey']
material = old.data.materials[0]
jersey = body.copy()
jersey.data = body.data.copy()
bpy.context.collection.objects.link(jersey)
jersey.name = 'C26_ContinuousJersey'
world, inverse = jersey.matrix_world.copy(), jersey.matrix_world.inverted()
bm = bmesh.new()
bm.from_mesh(jersey.data)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.0005 / max(world.to_scale()))

def inside(p):
    if not .975 < p.z < 1.54:
        return False
    if abs(p.x) < .23:
        return True
    side = 'l' if p.x > 0 else 'r'
    shoulder = rig.matrix_world @ rig.data.bones['upperarm_' + side].head_local
    elbow = rig.matrix_world @ rig.data.bones['lowerarm_' + side].head_local
    return (p - shoulder).dot((elbow - shoulder).normalized()) < (elbow - shoulder).length * .60

bmesh.ops.delete(bm, geom=[v for v in bm.verts if not inside(world @ v.co)], context='VERTS')
bm.normal_update()
for v in bm.verts:
    p = world @ v.co
    n = (world.to_3x3() @ v.normal).normalized()
    n.z *= .2
    p += n * .027
    # Cloth bridges the abdomen rather than tracing every skin indentation.
    if abs(p.x) < .20 and 1.0 < p.z < 1.43:
        if p.y < -.045:
            p.y = min(p.y, -.168)
        elif p.y > .045:
            p.y = max(p.y, .115)
    v.co = inverse @ p
boundary = [v for v in bm.verts if v.is_boundary]
interior = [v for v in bm.verts if not v.is_boundary]
for _ in range(12):
    bmesh.ops.smooth_vert(bm, verts=interior, factor=.45, use_axis_x=True, use_axis_y=True, use_axis_z=True)
for v in boundary:
    p = world @ v.co
    if p.z < 1.10:
        p.z = .975
    elif abs(p.x) > .23:
        side = 'l' if p.x > 0 else 'r'
        a = rig.matrix_world @ rig.data.bones['upperarm_' + side].head_local
        b = rig.matrix_world @ rig.data.bones['lowerarm_' + side].head_local
        axis = (b - a).normalized()
        p -= axis * (p - (a + (b - a) * .60)).dot(axis)
    v.co = inverse @ p
# The body source ends at the facial shoulder cutout. Bridge that ONE neckline
# to a fitted neck ring; copying the entire face surface created the old overlap.
edges = {e for e in bm.edges if e.is_boundary}
loops = []
while edges:
    edge = edges.pop()
    loop = [edge.verts[0], edge.verts[1]]
    while loop[-1] != loop[0]:
        following = next((e for e in loop[-1].link_edges if e in edges), None)
        if following is None:
            break
        edges.remove(following)
        loop.append(following.other_vert(loop[-1]))
    if loop[-1] == loop[0]:
        loops.append(loop[:-1])
neckline = max(loops, key=lambda loop: sum((world @ v.co).z for v in loop) / len(loop))
neck = rig.matrix_world @ rig.data.bones['neck_01'].head_local
deform = bm.verts.layers.deform.verify()
spine_index = (jersey.vertex_groups.get('spine_05') or jersey.vertex_groups.new(name='spine_05')).index
neck_index = (jersey.vertex_groups.get('neck_01') or jersey.vertex_groups.new(name='neck_01')).index
previous = neckline
new_faces = []
for t in (.33, .67, 1.0):
    ring = []
    for base in neckline:
        p = world @ base.co
        angle = math.atan2(p.y - neck.y, p.x - neck.x)
        target = neck + Vector((.056 * math.cos(angle), .052 * math.sin(angle), .022))
        vertex = bm.verts.new(inverse @ p.lerp(target, t))
        for group, weight in base[deform].items():
            vertex[deform][group] = weight * (1 - t)
        vertex[deform][spine_index] = vertex[deform].get(spine_index, 0) + .85 * t
        vertex[deform][neck_index] = vertex[deform].get(neck_index, 0) + .15 * t
        ring.append(vertex)
    for i in range(len(ring)):
        j = (i + 1) % len(ring)
        new_faces.append(bm.faces.new((previous[i], previous[j], ring[j], ring[i])))
    previous = ring
print('C26_NECKLINE', len(neckline), 'loops', [len(loop) for loop in loops])
# New bridge faces must face outward; inward winding renders invisible and
# exposes body skin through the collar/shoulder area.
bm.normal_update()
for f in new_faces:
    c = world @ f.calc_center_median()
    outward = Vector((c.x - neck.x, c.y - neck.y, 0.15))
    n = (world.to_3x3() @ f.normal).normalized()
    if n.dot(outward) < 0:
        f.normal_flip()
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
bm.to_mesh(jersey.data)
bm.free()
jersey.data.materials.clear()
jersey.data.materials.append(material)
for p in jersey.data.polygons:
    p.material_index = 0
    p.use_smooth = True
jersey.data.normals_split_custom_set([(0., 0., 0.)] * len(jersey.data.loops))
bpy.data.objects.remove(old, do_unlink=True)

# Hide torso-weighted skin inside the jersey volume so it cannot show through
# the collar/shoulder bridge. Limbs keep their skin; only spine/pelvis/neck
# weighted torso verts inside the garment region are removed.
TORSO_GROUPS = {'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01',
                'clavicle_l', 'clavicle_r', 'neck_02'}
for obj in [o for o in bpy.data.objects if o.type == 'MESH' and
            any(m and m.name.startswith(('MI_Body', 'MI_Face_Skin')) for m in o.data.materials)]:
    deform = obj.vertex_groups
    covered = []
    for v in obj.data.vertices:
        p = obj.matrix_world @ v.co
        if not (.96 < p.z < 1.57 and abs(p.x) < .26 and -.20 < p.y < .22):
            continue
        weights = [(deform[g.group].name, g.weight) for g in v.groups if g.group < len(deform)]
        if not weights:
            continue
        top = max(weights, key=lambda kv: kv[1])[0]
        if top in TORSO_GROUPS:
            covered.append(v.index)
    if covered:
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bm.verts.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[bm.verts[i] for i in covered], context='VERTS')
        bm.to_mesh(obj.data)
        bm.free()
    print('C26_OCCLUDE', obj.name, len(covered))

bpy.ops.object.select_all(action='DESELECT')
for obj in bpy.data.objects:
    if obj.type in {'MESH', 'ARMATURE'}:
        obj.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'C26_Athlete_Review.blend'))
bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_C26_Athlete_Review.fbx'), use_selection=True,
    object_types={'MESH', 'ARMATURE'}, add_leaf_bones=False, bake_anim=False,
    apply_unit_scale=True, use_mesh_modifiers=False)
(ROOT / 'Artifacts/CharacterAudit/garment-refinement.json').write_text(json.dumps({
    'source': 'C26_FullBody_Candidate.blend', 'review': 'C26_Athlete_Review.blend',
    'jersey_vertices': len(jersey.data.vertices), 'approved': False,
    'change': 'single-surface jersey; smoothed fabric; precise covered-torso occlusion'
}, indent=2))
print('C26_GARMENT_REFINED', len(jersey.data.vertices))
