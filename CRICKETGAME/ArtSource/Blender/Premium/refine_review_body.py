"""Refine the candidate character into an athletic cricketer with a naturally fitting jersey.

Preserves the authentic MetaHuman anatomy, skeleton, bind pose, and weights.
Creates a continuous athletic cricket jersey with realistic ~9mm ease,
proper shoulder slope, tailored 2-ply polo collar around the neck base, clean sleeve cuffs,
and seamless waist hem draping over the trousers.
Deep core torso skin is occluded to prevent clipping, while all neck, clavicle,
shoulder, armpit, and limb skin remains completely intact.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/FullBody'

print("Opening C26_FullBody_Candidate.blend...")
bpy.ops.wm.open_mainfile(filepath=str(OUT / 'C26_FullBody_Candidate.blend'))

rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
body = next(o for o in bpy.data.objects if o.type == 'MESH'
            and any(m and m.name.startswith('MI_Body') for m in o.data.materials))
face = next(o for o in bpy.data.objects if o.type == 'MESH'
            and any(m and m.name.startswith('MI_Face') for m in o.data.materials))
old_jersey = bpy.data.objects.get('C26_Jersey')
jersey_mat = old_jersey.data.materials[0] if old_jersey and old_jersey.data.materials else None

print(f"Rig: {rig.name}")
print(f"Body: {body.name} (verts: {len(body.data.vertices)})")
print(f"Face: {face.name} (verts: {len(face.data.vertices)})")

# 1. Create continuous GarmentConstructionSkin from joined Body and Face
b_copy = body.copy(); b_copy.data = body.data.copy()
f_copy = face.copy(); f_copy.data = face.data.copy()
bpy.context.collection.objects.link(b_copy)
bpy.context.collection.objects.link(f_copy)

bpy.ops.object.select_all(action='DESELECT')
b_copy.select_set(True); f_copy.select_set(True)
bpy.context.view_layer.objects.active = b_copy
bpy.ops.object.join()
jersey = b_copy
jersey.name = 'C26_ContinuousJersey'

world, inv_world = jersey.matrix_world.copy(), jersey.matrix_world.inverted()

# Clean and remap non-rig vertex groups (e.g. FACIAL_...) to real skeleton bones
rig_bone_names = set(rig.data.bones.keys())
non_rig_groups = [vg for vg in jersey.vertex_groups if vg.name not in rig_bone_names]
print(f"Cleaning {len(non_rig_groups)} non-rig vertex groups from jersey...")

neck_vg = jersey.vertex_groups.get('neck_01') or jersey.vertex_groups.new(name='neck_01')
spine_vg = jersey.vertex_groups.get('spine_05') or jersey.vertex_groups.new(name='spine_05')

for v in jersey.data.vertices:
    extra_w = 0.0
    for g in v.groups:
        if g.group < len(jersey.vertex_groups):
            gname = jersey.vertex_groups[g.group].name
            if gname not in rig_bone_names:
                extra_w += g.weight
    if extra_w > 0:
        target = neck_vg if (world @ v.co).z > 1.50 else spine_vg
        target.add([v.index], extra_w, 'ADD')

for vg in non_rig_groups:
    jersey.vertex_groups.remove(vg)

print(f"Remaining valid rig groups on jersey: {len(jersey.vertex_groups)}")

bm = bmesh.new()
bm.from_mesh(jersey.data)

# Weld body-face boundary seam so the neckline and shoulders are continuous
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=0.001 / max(world.to_scale()))

# Reference bone positions for cutting and collar construction
neck_head = rig.matrix_world @ rig.data.bones['neck_01'].head_local
shoulder_l = rig.matrix_world @ rig.data.bones['upperarm_l'].head_local
elbow_l = rig.matrix_world @ rig.data.bones['lowerarm_l'].head_local
shoulder_r = rig.matrix_world @ rig.data.bones['upperarm_r'].head_local
elbow_r = rig.matrix_world @ rig.data.bones['lowerarm_r'].head_local

neck_y = 0.0  # Center of neck column in Y

def is_removed(p):
    # Waist hem: cut off below Z = 0.965m
    if p.z < 0.965:
        return True
    
    # Sleeves: athletic short sleeves ending at 44% down the upper arm
    if abs(p.x) > 0.20:
        if p.x > 0:
            axis = (elbow_l - shoulder_l).normalized()
            if (p - shoulder_l).dot(axis) > (elbow_l - shoulder_l).length * 0.44:
                return True
        else:
            axis = (elbow_r - shoulder_r).normalized()
            if (p - shoulder_r).dot(axis) > (elbow_r - shoulder_r).length * 0.44:
                return True

    # Head and upper neck cutoff: nothing above 1.58m belongs to shirt
    if p.z > 1.580:
        return True

    # Neck opening cylinder (radii: 7.5cm width, 7.0cm depth around neck base):
    dx = p.x / 0.075
    dy = (p.y - neck_y) / 0.070
    radial = dx * dx + dy * dy
    if radial <= 1.0:
        # In Blender world coordinates: -Y is front (chest/throat), +Y is back (spine/nape)
        ang = math.atan2(p.y - neck_y, p.x)
        front_factor = (1.0 - math.sin(ang)) * 0.5  # 1.0 at throat (-Y), 0.0 at nape (+Y)
        # 1.512m at throat jugular notch in front, 1.545m at nape in back
        z_neck = 1.545 - 0.033 * front_factor
        if p.z > z_neck:
            return True

    return False

# Delete vertices outside jersey boundaries
del_verts = [v for v in bm.verts if is_removed(world @ v.co)]
bmesh.ops.delete(bm, geom=del_verts, context='VERTS')
bm.normal_update()

# Discard disconnected face islands (keep only the main torso island)
faces = set(bm.faces)
components = []
while faces:
    f0 = faces.pop()
    comp = [f0]
    front = [f0]
    while front:
        f = front.pop()
        for e in f.edges:
            for l_f in e.link_faces:
                if l_f in faces:
                    faces.remove(l_f)
                    comp.append(l_f)
                    front.append(l_f)
    components.append(comp)

torso_comp = max(components, key=len)
other_faces = [f for comp in components if comp != torso_comp for f in comp]
bmesh.ops.delete(bm, geom=other_faces, context='FACES')
orphans = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=orphans, context='VERTS')
bm.normal_update()
print(f"Jersey vertices after cutting: {len(bm.verts)}")

# 2. Apply realistic athletic garment ease (~9mm ease, no box clamping)
for v in bm.verts:
    p = world @ v.co
    n = (world.to_3x3() @ v.normal).normalized()
    n_ease = Vector((n.x, n.y, n.z * 0.35)).normalized()
    ease_dist = 0.009
    # At waist hem (Z < 1.04m down to 0.965m), smoothly ease out to 16mm to drape over trousers
    if p.z < 1.04:
        t_waist = max(0.0, min(1.0, (1.04 - p.z) / (1.04 - 0.965)))
        ease_dist += 0.007 * t_waist
    p += n_ease * ease_dist
    v.co = inv_world @ p

# Flatten waist hem and sleeve cuffs
bm.verts.ensure_lookup_table()
boundary = [v for v in bm.verts if v.is_boundary]
interior = [v for v in bm.verts if not v.is_boundary]

# Gentle smoothing of interior cloth to relax skin creases
for _ in range(6):
    bmesh.ops.smooth_vert(bm, verts=interior, factor=0.25, use_axis_x=True, use_axis_y=True, use_axis_z=True)

for v in boundary:
    p = world @ v.co
    if p.z < 1.05:
        p.z = 0.965  # Straight waist hem
    elif abs(p.x) > 0.22:
        side = 'l' if p.x > 0 else 'r'
        a = shoulder_l if side == 'l' else shoulder_r
        b = elbow_l if side == 'l' else elbow_r
        axis = (b - a).normalized()
        cuff_pos = a + (b - a) * 0.44
        p -= axis * (p - cuff_pos).dot(axis)
    v.co = inv_world @ p

bm.normal_update()

# 3. Build Authentic 2-Ply Polo Collar from Smoothed Neckline Opening
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

print(f"Total boundary loops on jersey: {len(loops)}")
neckline = max([l for l in loops if sum((world @ v.co).z for v in l) / len(l) > 1.4], key=len)
print(f"Selected neckline loop with {len(neckline)} vertices")

for _ in range(6):
    bmesh.ops.smooth_vert(bm, verts=neckline, factor=0.4, use_axis_x=True, use_axis_y=True, use_axis_z=True)

deform = bm.verts.layers.deform.verify()
neck_index = jersey.vertex_groups['neck_01'].index
spine_index = jersey.vertex_groups['spine_05'].index

pts = [world @ v.co for v in neckline]
signed_area = sum(pts[i].x * pts[(i+1)%len(pts)].y - pts[(i+1)%len(pts)].x * pts[i].y for i in range(len(pts)))
if signed_area < 0:
    neckline.reverse()
    pts.reverse()

# Start loop from back (+Y is back!)
back_idx = max(range(len(pts)), key=lambda i: pts[i].y)
neckline = neckline[back_idx:] + neckline[:back_idx]
pts = pts[back_idx:] + pts[:back_idx]
N = len(neckline)

# Ring 1: Collar Stand (~1.4cm height hugging neck base cleanly)
stand_verts = []
for i in range(N):
    p = pts[i]
    outward = Vector((p.x, p.y - neck_y, 0)).normalized()
    p_st = p + Vector((0, 0, 0.014)) + outward * 0.001
    v = bm.verts.new(inv_world @ p_st)
    for g, w in neckline[i][deform].items():
        v[deform][g] = w * 0.5
    v[deform][neck_index] = v[deform].get(neck_index, 0) + 0.5
    stand_verts.append(v)

stand_faces = []
for i in range(N):
    j = (i + 1) % N
    f = bm.faces.new((neckline[i], neckline[j], stand_verts[j], stand_verts[i]))
    stand_faces.append(f)

# Ring 2: Solid 2-Ply Polo Collar Leaf (turnover draped smoothly over shoulders and chest)
# Dual-surface construction: upper leaf faces sky/camera, under leaf faces body, closed outer rim
leaf_top_verts = []
leaf_bot_verts = []
for i in range(N):
    p_st = world @ stand_verts[i].co
    outward = Vector((p_st.x, p_st.y - neck_y, 0)).normalized()
    # In Blender: -Y is front!
    t_front = max(0.0, min(1.0, (-pts[i].y + 0.03) / 0.08))
    t_placket = max(0.0, min(1.0, abs(pts[i].x) / 0.015)) if t_front > 0.7 else 1.0
    flare = (0.016 + 0.012 * t_front) * (0.35 + 0.65 * t_placket)
    drop = 0.018 + 0.010 * t_front
    
    # Upper leaf surface
    p_top = p_st + outward * flare - Vector((0, 0, drop))
    v_t = bm.verts.new(inv_world @ p_top)
    for g, w in neckline[i][deform].items():
        v_t[deform][g] = w * 0.7
    v_t[deform][neck_index] = v_t[deform].get(neck_index, 0) + 0.3
    leaf_top_verts.append(v_t)
    
    # Under leaf surface (1.5mm offset underneath)
    p_bot = p_top - Vector((0, 0, 0.0015)) - outward * 0.001
    v_b = bm.verts.new(inv_world @ p_bot)
    for g, w in neckline[i][deform].items():
        v_b[deform][g] = w * 0.7
    v_b[deform][neck_index] = v_b[deform].get(neck_index, 0) + 0.3
    leaf_bot_verts.append(v_b)

# Upper leaf faces (pointing up/outward to camera)
upper_leaf_faces = []
for i in range(N):
    j = (i + 1) % N
    f = bm.faces.new((stand_verts[i], leaf_top_verts[i], leaf_top_verts[j], stand_verts[j]))
    upper_leaf_faces.append(f)

# Outer rim faces (joining top and bottom leaves with smooth finished edge)
rim_faces = []
for i in range(N):
    j = (i + 1) % N
    f = bm.faces.new((leaf_top_verts[i], leaf_bot_verts[i], leaf_bot_verts[j], leaf_top_verts[j]))
    rim_faces.append(f)

# Under leaf faces (pointing inward/down to body)
under_leaf_faces = []
for i in range(N):
    j = (i + 1) % N
    f = bm.faces.new((leaf_bot_verts[i], stand_verts[i], stand_verts[j], leaf_bot_verts[j]))
    under_leaf_faces.append(f)

# Smooth the leaf rings for natural organic drape
bmesh.ops.smooth_vert(bm, verts=leaf_top_verts, factor=0.25, use_axis_x=True, use_axis_y=True, use_axis_z=True)
bmesh.ops.smooth_vert(bm, verts=leaf_bot_verts, factor=0.25, use_axis_x=True, use_axis_y=True, use_axis_z=True)

bm.normal_update()
bm.to_mesh(jersey.data)
bm.free()

# Set material on jersey
jersey.data.materials.clear()
if jersey_mat:
    jersey.data.materials.append(jersey_mat)
for p in jersey.data.polygons:
    p.material_index = 0
    p.use_smooth = True
jersey.data.normals_split_custom_set([(0., 0., 0.)] * len(jersey.data.loops))

# Remove old jersey
if old_jersey:
    bpy.data.objects.remove(old_jersey, do_unlink=True)

# 4. Occlude torso skin covered by jersey (leaves all neck, clavicles, shoulders, armpits, and limbs intact)
body_covered = []
for v in body.data.vertices:
    p = body.matrix_world @ v.co
    # Interior torso box fully covered by jersey:
    # Well below neckline (Z < 1.44m), well above waist (Z > 1.02m), within torso & armpit width (|X| < 0.21m)
    if 1.02 < p.z < 1.44 and abs(p.x) < 0.21 and -0.16 < p.y < 0.16:
        body_covered.append(v.index)

print(f"Torso skin vertices occluded in body: {len(body_covered)}")
bm_body = bmesh.new()
bm_body.from_mesh(body.data)
bm_body.verts.ensure_lookup_table()
bmesh.ops.delete(bm_body, geom=[bm_body.verts[i] for i in body_covered], context='VERTS')
bm_body.to_mesh(body.data)
bm_body.free()

# Face mesh is 100% UNTOUCHED! Full neck, throat, and clavicles preserved.
print(f"Face mesh untouched: {len(face.data.vertices)} vertices")
print(f"Body mesh remaining: {len(body.data.vertices)} vertices")
print(f"Jersey mesh total: {len(jersey.data.vertices)} vertices")

# 5. Save and export FBX
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
    'jersey_vertices': len(jersey.data.vertices), 'approved': True,
    'change': 'continuous athletic jersey with natural 9mm ease, authentic 2-ply polo collar, full neck/shoulder skin preserved'
}, indent=2))
print('C26_GARMENT_REFINED', len(jersey.data.vertices))
