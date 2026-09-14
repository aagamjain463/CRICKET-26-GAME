"""Round-2 AFTER verification: new gloves on the Round-1 grip + shoes on feet.

- Poses the review rig at BATTER_READY_R (Round-1 placement, untouched).
- Places the v002 gloves + hero bat with the measured batter-offsets.json.
- Renders tight multi-angle grip close-ups WITH the hands visible.
- Numeric checks: hand-skin containment inside glove shells, handle-vs-glove
  penetration depth (must stay within padding), tri counts, import-gate dims.
- Fits the v002 shoes onto the standing feet with the production fit math
  (temporary scene objects only -- blends are NOT modified here) and renders
  front/side/rear standing views.
Output: Artifacts/CharacterAudit/equip_after/verify_*.png + console metrics.
"""
import json
import math
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

OUT = ROOT / 'Artifacts/CharacterAudit/equip_after'
OUT.mkdir(parents=True, exist_ok=True)
EXPORTS = ROOT / 'ArtSource/Exports/Equipment'
OFFSETS = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-offsets.json').read_text())


def import_cm(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    objs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    assert objs, path
    bpy.context.view_layer.update()
    if max(max(o.dimensions) for o in objs) < 2.0:
        # Operator-free metre->cm scaling: background-mode transform_apply
        # proved flaky here, so scale the mesh data directly (object stays
        # scale-1, exactly like the authoring pipeline's stored convention).
        for o in objs:
            for v in o.data.vertices:
                v.co *= 100.0
            o.data.update()
    bpy.context.view_layer.update()
    return objs


def offset_matrix(entry):
    m = Matrix.Identity(4)
    for i, k in enumerate('xyz'):
        m.col[i].xyz = Vector(entry['axes'][k])
    m.translation = Vector(entry['loc'])
    return m


def place(rig, bone_name, entry, objs):
    m = rig.matrix_world @ rig.pose.bones[bone_name].matrix @ offset_matrix(entry)
    for o in objs:
        o.matrix_world = m
    bpy.context.view_layer.update()


def frame_camera(target, radius, azimuth, elevation, res=(900, 900)):
    scn = bpy.context.scene
    scn.render.engine = 'BLENDER_EEVEE'
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.film_transparent = False
    scn.render.image_settings.file_format = 'PNG'
    world = bpy.data.worlds.get('C26Studio') or bpy.data.worlds.new('C26Studio')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.055, 0.062, 0.075, 1)
    world.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    scn.world = world
    cam_data = bpy.data.cameras.new('C26VerifyCam')
    cam_data.lens = 62
    cam = bpy.data.objects.new('C26VerifyCam', cam_data)
    bpy.context.collection.objects.link(cam)
    a, e = math.radians(azimuth), math.radians(elevation)
    dist = radius * 1.18 / math.tan(cam_data.angle * 0.5) * 1.05
    cam.location = target + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e),
                                    math.sin(e))) * dist
    cam.rotation_mode = 'QUATERNION'
    cam.rotation_quaternion = (target - cam.location).to_track_quat('-Z', 'Y')
    scn.camera = cam
    lights = []
    for name, ang, elev, power, size in (('Key', azimuth + 34, 44, 5.0, 1.4),
                                        ('Fill', azimuth - 78, 8, 5.0 * 0.28, 2.4),
                                        ('Rim', azimuth + 168, 30, 5.0 * 0.55, 1.0)):
        ld = bpy.data.lights.new(name, 'AREA')
        ld.energy = power * (dist ** 2) * 0.35
        ld.size = size * radius
        lo_ = bpy.data.objects.new(name, ld)
        bpy.context.collection.objects.link(lo_)
        la, le = math.radians(ang), math.radians(elev)
        lo_.location = target + Vector((math.cos(la) * math.cos(le), math.sin(la) * math.cos(le),
                                        math.sin(le))) * dist
        lo_.rotation_mode = 'QUATERNION'
        lo_.rotation_quaternion = (target - lo_.location).to_track_quat('-Z', 'Y')
        lights.append(lo_)
    return cam, lights


def shoot(path, target, radius, azimuth, elevation):
    cam, lights = frame_camera(target, radius, azimuth, elevation)
    scn = bpy.context.scene
    scn.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    for o in [cam] + lights:
        bpy.data.objects.remove(o, do_unlink=True)
    print('RENDER', path)


rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=True)
rig_lib.apply(rig, actions.BATTER_READY_R)
bat = import_cm(EXPORTS / 'SM_C26_Bat_Hero.fbx')
gl = import_cm(EXPORTS / 'SM_C26_Glove_L.fbx')
gr = import_cm(EXPORTS / 'SM_C26_Glove_R.fbx')

# gate dims + tris on the new exports BEFORE placing on 0.01-scale rig
for objs, want, tol in ((gl, 19.0, 1.5), (gr, 19.0, 1.5)):
    for o in objs:
        dims = sorted((float(d) for d in o.dimensions), reverse=True)
        tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
        print('VERIFY %s longest=%.2f (want %.1f+-%.1f) tris=%d slots=%s' % (
            o.name, dims[0], want, tol, tris, [m.name for m in o.data.materials]))
        assert abs(dims[0] - want) <= tol, o.name

place(rig, 'hand_l', OFFSETS['Bat'], bat)
place(rig, 'hand_l', OFFSETS['BattingGloveL'], gl)
place(rig, 'hand_r', OFFSETS['BattingGloveR'], gr)

# tight grip close-ups, hands visible: hide jersey/trousers/shoes only
for o in bpy.data.objects:
    if o.type == 'MESH' and ('Jersey' in o.name or 'Trousers' in o.name or 'Shoe' in o.name):
        o.hide_render = True
gc = (Vector(gl[0].matrix_world.translation) + Vector(gr[0].matrix_world.translation)) * 0.5
for i, (az, el) in enumerate(((35, 12), (150, 18), (265, 30), (205, -12))):
    shoot(OUT / ('verify_grip_%d.png' % i), gc, 0.16, az, el)

# ---- numeric: hand skin containment + handle poke-through (glove-local cm)
deps = bpy.context.evaluated_depsgraph_get()
body = next(o for o in bpy.data.objects if o.type == 'MESH' and 'BodyMesh' in o.name)
ev = body.evaluated_get(deps)
emesh = ev.to_mesh()
skin_world = [ev.matrix_world @ v.co for v in emesh.vertices]
ev.to_mesh_clear()
grip_polys = json.loads((ROOT / 'Artifacts/CharacterAudit/grip-measure.json').read_text())


def ray_inside(obj, world_pt):
    inv = obj.matrix_world.inverted()
    ro = inv @ world_pt
    res = obj.ray_cast(ro, Vector((1, 0, 0)), distance=500.0)
    return bool(res[0])


for side, gobjs, gkey, hname in (('l', gl, 'BattingGloveL', 'hand_l'),
                                 ('r', gr, 'BattingGloveR', 'hand_r')):
    hand = rig.pose.bones[hname]
    to_local = (rig.matrix_world @ hand.matrix @ offset_matrix(OFFSETS[gkey])).inverted()
    to_world = to_local.inverted()
    origin = to_local @ (rig.matrix_world @ hand.head)
    pts = []
    for w in skin_world:
        p = to_local @ w
        if (p - origin).length < 14.0 and p.z > -9.0:
            pts.append(p)
    inside = 0
    bpy.context.view_layer.update()
    for p in pts[::3]:
        if sum(1 for go in gobjs if ray_inside(go, to_world @ p)) % 2 == 1:
            inside += 1
    print('VERIFY glove_%s skin_pts=%d contained=%.1f%%' % (side, len(pts[::3]),
          100.0 * inside / max(len(pts[::3]), 1)))
    # handle poke-through: handle verts inside glove AND far from finger/palm
    # centrelines are suspicious (not part of the hidden wrap contact).
    centres = []
    for f, v in grip_polys[side]['fingers'].items():
        poly = [Vector(p) for p in v['poly']]
        centres.extend(poly)
    centres.append(origin)
    bad, bad_d = 0, 0.0
    for bo in bat:
        for v in list(bo.data.vertices)[::5]:
            pw = bo.matrix_world @ v.co
            if sum(1 for go in gobjs if ray_inside(go, pw)) % 2 == 1:
                pl = to_local @ pw
                d = min((pl - c).length for c in centres)
                if d > 2.0:
                    bad += 1
                    bad_d = max(bad_d, d)
    print('VERIFY glove_%s handle_inside_far=%d maxdist=%.2f (want 0)' % (side, bad, bad_d))

# ---- Now shoe verification: standing athlete front, side, rear
# Unhide trousers & body, reset pose to standing / rest pose or inspect standing feet
rig_lib.clear(rig)
# Import v002 shoes and fit them onto the standing feet
new_shoes = []
for side, letter in (('l', 'L'), ('r', 'R')):
    objs = import_cm(EXPORTS / ('SM_C26_Shoe_%s.fbx' % letter))
    o = objs[0]
    o.name = 'SM_C26_Shoe_' + letter
    dims = sorted((float(d) for d in o.dimensions), reverse=True)
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    print('VERIFY %s longest=%.2f (want 27.5+-1.5) tris=%d slots=%s' % (
        o.name, dims[0], tris, [m.name for m in o.data.materials]))
    assert abs(dims[0] - 27.5) <= 1.5, o.name

    # Authored shoes use +X forward. MetaHuman in this FBX faces -Y.
    o.rotation_euler = (0, 0, -math.pi / 2)
    bpy.context.view_layer.update()
    verts = [o.matrix_world @ v.co for v in o.data.vertices]
    low = Vector([min(p[i] for p in verts) for i in range(3)])
    high = Vector([max(p[i] for p in verts) for i in range(3)])
    valid = {b.name for b in rig.data.bones if b.name == 'foot_' + side or any(a.name == 'foot_' + side for a in b.parent_recursive)}
    foot = [body.matrix_world @ v.co for v in body.data.vertices if sum(g.weight for g in v.groups if body.vertex_groups[g.group].name in valid) > 0.65]
    assert foot, 'Foot skin required for fitting'
    foot_min = Vector([min(p[i] for p in foot) for i in range(3)])
    foot_max = Vector([max(p[i] for p in foot) for i in range(3)])
    desired = (foot_max - foot_min) * 1.08
    desired.z = max(desired.z, 0.13)
    sdims = high - low
    factors = Vector([desired[i] / sdims[i] for i in range(3)])
    inv = o.matrix_world.inverted()
    oldcenter = (low + high) * 0.5
    center = (foot_min + foot_max) * 0.5
    center.z = desired.z * 0.5 - 0.005
    for v in o.data.vertices:
        delta = o.matrix_world @ v.co - oldcenter
        v.co = inv @ (center + Vector([delta[i] * factors[i] for i in range(3)]))
    bpy.context.view_layer.update()
    world = o.matrix_world.copy()
    o.parent = rig
    o.matrix_world = world
    o.vertex_groups.clear()
    g = o.vertex_groups.new(name='foot_' + side)
    g.add(list(range(len(o.data.vertices))), 1, 'REPLACE')
    o.modifiers.new('CanonicalSkin', 'ARMATURE').object = rig
    new_shoes.append(o)

# Hide old shoes, bat, gloves; show body, trousers, new shoes
for o in bpy.data.objects:
    if o.type == 'MESH':
        if 'C26_Shoe' in o.name or 'Bat' in o.name or 'Glove' in o.name:
            o.hide_render = True
        elif o in new_shoes or 'Trouser' in o.name or 'BodyMesh' in o.name:
            o.hide_render = False

bpy.context.view_layer.update()
foot_c = (new_shoes[0].matrix_world.translation + new_shoes[1].matrix_world.translation) * 0.5 + Vector((0, 0, 0.10))
# Front (az=90), Side (az=0), Rear (az=270)
for name, az, el in (('front', 90, 8), ('side', 0, 8), ('rear', 270, 8)):
    shoot(OUT / ('verify_shoes_%s.png' % name), foot_c, 0.40, az, el)

print('C26_VERIFY_EQUIP_DONE')
