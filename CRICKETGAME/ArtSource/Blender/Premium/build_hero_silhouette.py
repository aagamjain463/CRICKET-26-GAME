"""Isolated male hero bind-mesh sculpt. Run with Blender --background --python.

No bone/weight/animation edits. Measurements are metres in world space. The
existing full anatomical source supplies topology; the review jersey supplies
its single-surface topology. Outputs never replace either source or match assets.
"""
import bpy
import json
import math
import hashlib
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'ArtSource/Premium/FullBody'
OUT = ROOT / 'ArtSource/Premium/HeroSilhouette'


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def profile(z, keys):
    if z <= keys[0][0]:
        return keys[0][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if z <= b:
            return va + (vb - va) * smooth(a, b, z)
    return keys[-1][1]


def signature(rig):
    data = [(b.name, b.parent.name if b.parent else None,
             [list(row) for row in b.matrix_local]) for b in rig.data.bones]
    return hashlib.sha256(json.dumps(data).encode()).hexdigest()


def weight_signature(obj):
    data = [[(obj.vertex_groups[g.group].name, g.weight) for g in v.groups]
            for v in obj.data.vertices]
    return hashlib.sha256(json.dumps(data).encode()).hexdigest()


def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return [[round(f(p[i] for p in points) * 100, 3) for i in range(3)]
            for f in (min, max)]


def surface_tree(objects, max_x=None):
    vertices, faces = [], []
    for obj in objects:
        offset = len(vertices)
        vertices.extend(obj.matrix_world @ v.co for v in obj.data.vertices)
        faces.extend(tuple(offset + i for i in p.vertices) for p in obj.data.polygons
                     if max_x is None or all(abs(vertices[offset + i].x) < max_x for i in p.vertices))
    return BVHTree.FromPolygons(vertices, faces)


def rebuild():
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / 'C26_FullBody_Candidate.blend'))
    bpy.context.preferences.filepaths.save_version = 0
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    body = next(o for o in bpy.data.objects if o.type == 'MESH' and
                any(m and m.name.startswith('MI_Body') for m in o.data.materials))
    face = next(o for o in bpy.data.objects if o.type == 'MESH' and
                any(m and m.name.startswith('MI_Face_Skin') for m in o.data.materials))
    old = bpy.data.objects['C26_Jersey']
    jersey_material = old.data.materials[0]
    bpy.data.objects.remove(old, do_unlink=True)
    with bpy.data.libraries.load(str(SOURCE / 'C26_Athlete_Review.blend'), link=False) as (src, dst):
        dst.objects = ['C26_ContinuousJersey']
    jersey = dst.objects[0]
    bpy.context.scene.collection.objects.link(jersey)
    bpy.context.view_layer.update()
    world = jersey.matrix_world.copy()
    jersey.parent = rig
    jersey.matrix_world = world
    for mod in jersey.modifiers:
        if mod.type == 'ARMATURE':
            mod.object = rig
    jersey.data.materials.clear()
    jersey.data.materials.append(jersey_material)
    # Library loading can bring in an unlinked parent; only the source rig exports.
    for obj in list(bpy.data.objects):
        if obj.type == 'ARMATURE' and obj != rig:
            bpy.data.objects.remove(obj, do_unlink=True)
    bpy.context.view_layer.update()
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    before_weights = {o.name: weight_signature(o) for o in meshes}
    before_skeleton = signature(rig)
    before_bounds = {o.name: bounds(o) for o in meshes}
    joints = {b.name: rig.matrix_world @ b.head_local for b in rig.data.bones}
    original_body = [body.matrix_world @ v.co for v in body.data.vertices]

    def sculpt(p):
        """Continuous rest-space shape field, not pose/bone scaling."""
        q = p.copy()
        x, y, z = p
        ax = abs(x)
        # Iliac crest -> abdominal wall -> ribcage. The skeletal hip/shoulder
        # centres remain fixed; this removes excess lateral soft-tissue flare.
        central = 1.0 - smooth(.18, .29, ax)
        # Masculinise the female base: strongly narrow waist/hips, broaden
        # chest/shoulders into a V-taper. Joint centres untouched.
        sx = profile(z, [(.72, 1), (.86, .86), (.98, .84), (1.08, .94),
                         (1.19, 1.16), (1.30, 1.16), (1.42, 1.08), (1.53, 1)])
        q.x += x * (sx - 1) * central
        # Reduce pelvic anterior/posterior bulk and carry a straighter waist.
        sy = profile(z, [(.73, 1), (.94, .84), (1.06, .85), (1.18, 1.0), (1.27, 1)])
        q.y = -.015 + (y + .015) * (1 + (sy - 1) * central)
        # Replace the source's breast-shaped front with a broad, flatter male
        # pectoral/rib envelope. Blend at sternum, lateral ribs and abdomen.
        thorax = smooth(1.15, 1.28, z) * (1 - smooth(1.44, 1.51, z))
        front = 1 - smooth(-.055, -.005, y)
        width = profile(z, [(1.15, .21), (1.30, .25), (1.43, .27), (1.51, .255)])
        depth = profile(z, [(1.15, .125), (1.30, .135), (1.40, .138), (1.51, .105)])
        target_y = -.005 - depth * math.sqrt(max(.08, 1 - (q.x / width) ** 2))
        q.y += (target_y - q.y) * thorax * front * (1 - smooth(.19, .26, ax))
        # The source has a high posterior shoulder mound and protruding lower
        # abdomen. Establish a shallow thoracic curve and supported abdominal
        # wall, rather than a hunched yoke above a hollow lumbar waist.
        back = smooth(.015, .065, y) * smooth(1.20, 1.34, z) * (1 - smooth(1.52, 1.59, z))
        back_depth = profile(z, [(1.20, .09), (1.40, .105), (1.50, .088), (1.59, .05)])
        back_y = .005 + back_depth * math.sqrt(max(.12, 1 - (q.x / .28) ** 2))
        q.y += (back_y - q.y) * back * (1 - smooth(.19, .28, ax))
        abdomen = smooth(.98, 1.075, z) * (1 - smooth(1.15, 1.24, z)) * front * central
        abdomen_y = -.005 - .115 * math.sqrt(max(.12, 1 - (q.x / .23) ** 2))
        q.y += (abdomen_y - q.y) * abdomen
        # Limb cross sections are adjusted around joint-to-joint axes, tapering
        # to zero at every joint. Existing knee/elbow loops and pivot placement
        # survive exactly. Hands and feet keep their already plausible sizes.
        side = 'l' if x >= 0 else 'r'
        for start, end, amount, peak in (
                ('upperarm_', 'lowerarm_', .13, .43),
                ('lowerarm_', 'hand_', .10, .32),
                ('thigh_', 'calf_', .055, .42),
                ('calf_', 'foot_', .10, .34)):
            a, b = joints[start + side], joints[end + side]
            axis = b - a
            t = (p - a).dot(axis) / axis.length_squared
            if not 0 < t < 1:
                continue
            closest = a + axis * t
            radial = p - closest
            limit = .095 if start in ('thigh_', 'calf_') else .078
            if radial.length > limit or (start.endswith('arm_') and ax < .20):
                continue
            bell = smooth(0, peak, t) * (1 - smooth(peak, 1, t))
            q += radial * amount * bell * (1 - smooth(limit * .85, limit, radial.length))
        return q

    # Body and trousers share a smooth spatial field. The facial mesh includes
    # a shoulder/neck yoke: only that body region participates below 1.59m.
    # All actual facial/head vertices (and footwear) remain exactly untouched.
    for obj in meshes:
        if obj == jersey or obj.name.startswith('C26_Shoe'):
            continue
        inverse = obj.matrix_world.inverted()
        for v in obj.data.vertices:
            p = obj.matrix_world @ v.co
            if obj == face and p.z >= 1.59:
                continue
            q = sculpt(p)
            if obj == face:
                q = p.lerp(q, 1 - smooth(1.53, 1.59, p.z))
            v.co = inverse @ q
        obj.data.update()

    # Conform the existing single-surface shirt with the SAME sculpt field as
    # the body, so the original tailored ease is preserved. Only verts that end
    # up inside/too close to skin get a minimal push-out; verts already outside
    # are left alone so sleeves can never snap onto the forearm.
    skin = surface_tree([body, face])
    inverse = jersey.matrix_world.inverted()

    def ensure_clearance(p, minimum=0.012, target=0.014):
        nearest, normal, _, distance = skin.find_nearest(p)
        if nearest is None or distance is None:
            return p
        signed = (p - nearest).dot(normal)
        if signed >= minimum and distance >= minimum:
            return p
        return nearest + normal * target

    for v in jersey.data.vertices:
        p = sculpt(jersey.matrix_world @ v.co)
        v.co = inverse @ ensure_clearance(p)
    # Relax only the garment interior, then re-establish minimum skin clearance.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(jersey.data)
    interior = [v for v in bm.verts if not v.is_boundary]
    for _ in range(5):
        bmesh.ops.smooth_vert(bm, verts=interior, factor=.35,
                             use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.normal_update()
    bm.to_mesh(jersey.data)
    bm.free()
    for v in jersey.data.vertices:
        p = jersey.matrix_world @ v.co
        v.co = inverse @ ensure_clearance(p)
    for obj in meshes:
        if obj != face:
            if obj.data.has_custom_normals:
                obj.data.normals_split_custom_set([(0, 0, 0)] * len(obj.data.loops))
            for poly in obj.data.polygons:
                poly.use_smooth = True
            obj.data.update()
    assert signature(rig) == before_skeleton
    assert all(weight_signature(o) == before_weights[o.name] for o in meshes)
    assert all(v.groups for o in meshes for v in o.data.vertices)
    rig['C26_Hero_ID'] = 'HeroMale_Silhouette_01'
    rig['C26_Scope'] = 'Offline bind-mesh proportions only; original skeleton and weights'
    rig['C26_MatchApproved'] = False
    bpy.context.scene['C26_Inspection'] = 'Run review_hero_silhouette.py for neutral front/side/back/three-quarter evidence'
    # Save/export in original reference pose, never bake the inspection pose.
    rig.animation_data_clear()
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in [rig] + meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'C26_HeroMale_Silhouette_01.blend'))
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_C26_HeroMale_Silhouette_01.fbx'),
        use_selection=True, object_types={'MESH', 'ARMATURE'}, add_leaf_bones=False,
        bake_anim=False, apply_unit_scale=True, use_mesh_modifiers=False)
    report = {
        'hero': 'HeroMale_Silhouette_01', 'match_approved': False,
        'source': ['C26_FullBody_Candidate.blend', 'C26_Athlete_Review.blend'],
        'skeleton_sha256': before_skeleton, 'bone_count': len(rig.data.bones),
        'skeleton_unchanged': True, 'weights_unchanged': True,
        'face_head_detail_unchanged_above_m': 1.59, 'parts': [],
        'joint_lengths_cm': {}, 'body_cross_sections_cm': {},
    }
    for a, b in [('upperarm_l', 'lowerarm_l'), ('lowerarm_l', 'hand_l'),
                 ('thigh_l', 'calf_l'), ('calf_l', 'foot_l')]:
        report['joint_lengths_cm'][a + ':' + b] = round((joints[a] - joints[b]).length * 100, 3)
    for z in (.90, 1.00, 1.10, 1.20, 1.30, 1.40):
        indices = [i for i, p in enumerate(original_body) if abs(p.z - z) < .012 and abs(p.x) < .21]
        values = []
        for points in (original_body, [body.matrix_world @ v.co for v in body.data.vertices]):
            ps = [points[i] for i in indices]
            values.append([round((max(p[i] for p in ps) - min(p[i] for p in ps)) * 100, 3) for i in (0, 1)])
        report['body_cross_sections_cm'][str(z)] = {'before_width_depth': values[0], 'after_width_depth': values[1]}
    for obj in meshes:
        obj.data.calc_loop_triangles()
        report['parts'].append({'name': obj.name, 'vertices': len(obj.data.vertices),
            'triangles': len(obj.data.loop_triangles), 'bounds_before_cm': before_bounds[obj.name],
            'bounds_after_cm': bounds(obj), 'weights_sha256': before_weights[obj.name],
            'unweighted_vertices': sum(not v.groups for v in obj.data.vertices)})
    (OUT / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('C26_HERO_SILHOUETTE', json.dumps(report))


if __name__ == '__main__':
    rebuild()
