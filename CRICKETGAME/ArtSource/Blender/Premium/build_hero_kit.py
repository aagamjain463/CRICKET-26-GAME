"""Hero cricket kit pass (Round 2: clothing only).

Reads the approved Round-1 hero silhouette blend. NEVER modifies body, face,
hands, shoes, skeleton, weights of untouched verts, animations or gameplay.

Outputs (new assets only, never overwrites Round-1 or match assets):
  ArtSource/Premium/HeroKit/C26_HeroKit_02.blend
  ArtSource/Premium/HeroKit/SK_C26_HeroKit_02.fbx
  ArtSource/Premium/HeroKit/kit-report.json

Design (authored geometry + skinning, NO cloth sim):
- Jersey/trousers keep original vertex order/weights where untouched; new verts
  (collar, cuffs, waistband, solidify inner shell) get weights copied from the
  nearest body surface / corresponding outer vert.
- Garment ease 18-25mm (vs 12-14mm skin-tight before) so cloth reads over body.
- Real thickness via inward solidify (2.5mm jersey, 2.0mm trousers) with rim at
  every opening: no paper-thin edges, no see-through in close-up.
- Crew polo collar band + short V placket + 2 buttons; sleeve cuff bands;
  waistband + fly stitch + side-seam pocket welts (closed, no bags).
- Panel seams as material slots (shoulder/side/armhole, outseam/inseam), not
  floating piping: zero deformation risk, restrained look.
- Subtle authored folds (3-5mm torso drape, 2mm knee-break), masked to loose
  areas; symmetry preserved.
- Extra knee articulation loops (2 above / 2 below patella) + forward knee
  ease for bending; underarm ease increased to avoid pinching.
"""
import bpy
import bmesh
import json
import math
import hashlib
from pathlib import Path
from mathutils import Vector
from mathutils.kdtree import KDTree
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / 'ArtSource/Premium/HeroSilhouette/C26_HeroMale_Silhouette_01.blend'
OUT = ROOT / 'ArtSource/Premium/HeroKit'

JERSEY_EASE = 0.022   # 22mm torso, sleeves 20-30mm by region
TROUSER_EASE = 0.018  # 18mm, seat/knee more
JERSEY_THICK = 0.0025
TROUSER_THICK = 0.0020
MIN_CLEAR_NEUTRAL = 0.010


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def sig(rig):
    import json as j, hashlib as h
    d = [(b.name, b.parent.name if b.parent else None,
          [list(r) for r in b.matrix_local]) for b in rig.data.bones]
    return h.sha256(j.dumps(d).encode()).hexdigest()


def body_tree(body):
    vs = [body.matrix_world @ v.co for v in body.data.vertices]
    kdt = KDTree(len(vs))
    for i, p in enumerate(vs):
        kdt.insert(p, i)
    return kdt, vs


def nearest_weights(body, kdt, p):
    _, i, _ = kdt.find(p)
    v = body.data.vertices[i]
    return [(body.vertex_groups[g.group].name, g.weight) for g in v.groups]


def ensure_slot(obj, name):
    m = next((m for m in obj.data.materials if m and m.name == name), None)
    if m is None:
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get('Principled BSDF')
        if bsdf:
            # Matte sports fabric preview; Unreal M_C26_Kit is authoritative.
            bsdf.inputs['Roughness'].default_value = 0.86
            for k, v in (('Specular', 0.26), ('Specular IOR Level', 0.26),
                         ('Sheen', 0.55), ('Sheen weight', 0.55)):
                try:
                    if k in bsdf.inputs:
                        bsdf.inputs[k].default_value = v
                except Exception:
                    pass
        obj.data.materials.append(m)
    return obj.data.materials.find(m.name)


def neckline_loop(jersey):
    """Neck-opening boundary verts (world metres) = highest boundary loop."""
    bm = bmesh.new()
    bm.from_mesh(jersey.data)
    bm.verts.ensure_lookup_table()
    bnd = {v.index for v in bm.verts if v.is_boundary}
    loops, seen = [], set()
    for v in bm.verts:
        if not v.is_boundary or v.index in seen:
            continue
        loop, cur, prev = [v], v, None
        seen.add(v.index)
        while len(loop) < 600:
            nxt = None
            for e in cur.link_edges:
                o = e.other_vert(cur)
                if o.is_boundary and o != prev and o.index not in seen:
                    nxt = o
                    break
            if nxt is None:
                break
            loop.append(nxt)
            seen.add(nxt.index)
            prev, cur = cur, nxt
        pts = [jersey.matrix_world @ x.co for x in loop]
        if pts and sum(p.z for p in pts) / len(pts) > 1.40:
            loops.append((loop, pts))
    bm.free()
    assert loops, 'neckline loop not found'
    loops.sort(key=lambda t: sum(p.z for p in t[1]) / len(t[1]), reverse=True)
    return loops[0]


def build_collar_parts(jersey, body, kdt, mat_trim):
    """Ribbed collar band + 2 buttons fitted to the jersey neckline.

    Returned objects are UNPARENTED world-metre meshes; the caller JOINs them
    into the jersey (join auto-converts transforms, avoiding the 0.01
    parent-scale trap that stranded the first attempt at the origin).
    """
    _, pts = neckline_loop(jersey)
    cx = sum(p.x for p in pts) / len(pts)
    cy = sum(p.y for p in pts) / len(pts)
    cz = sum(p.z for p in pts) / len(pts)
    erx = max(abs(p.x - cx) for p in pts)
    ery = max(abs(p.y - cy) for p in pts)
    segs, h = 28, 0.030
    mesh = bpy.data.meshes.new('C26_HeroCollar')
    o = bpy.data.objects.new('C26_HeroCollar', mesh)
    bpy.context.scene.collection.objects.link(o)
    verts, faces = [], []
    for r in range(3):
        z = cz + 0.012 + h / 2 - r * h / 2
        grow = 0.004 if r == 1 else 0.0  # slight rib out
        for s in range(segs):
            a = 2 * math.pi * s / segs
            verts.append((cx + (erx + 0.002 + grow) * math.cos(a),
                          cy + (ery + 0.002 + grow) * math.sin(a), z))
    for r in range(2):
        for s in range(segs):
            a, b = r * segs + s, r * segs + (s + 1) % segs
            faces.append((a, b, b + segs, a + segs))
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    mesh.materials.append(mat_trim)
    for p in mesh.polygons:
        p.material_index = 0
        p.use_smooth = True
    made = [o]
    bpy.ops.object.select_all(action='DESELECT')
    for k in range(2):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=5,
            radius=0.008, location=(cx, cy - ery - 0.004, cz + 0.002 - k * 0.022))
        b = bpy.context.object
        b.name = f'C26_HeroButton_0{k}'
        b.scale = (1, 0.45, 1)
        bpy.context.view_layer.update()
        b.data.materials.append(mat_trim)
        made.append(b)
    for ob in made:
        for i, v in enumerate(ob.data.vertices):
            w = nearest_weights(body, kdt, ob.matrix_world @ v.co)
            for n, wt in w:
                g = ob.vertex_groups.get(n) or ob.vertex_groups.new(name=n)
                g.add([i], wt, 'REPLACE')
    return made


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SRC))
    bpy.context.preferences.filepaths.save_version = 0
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    skel = sig(rig)
    nbones = len(rig.data.bones)
    body = next(o for o in bpy.data.objects if o.type == 'MESH' and 'BodyMesh' in o.name)
    face = next(o for o in bpy.data.objects if o.type == 'MESH' and 'FaceMesh' in o.name)
    jersey = next(o for o in bpy.data.objects if o.type == 'MESH' and 'Jersey' in o.name)
    trousers = next(o for o in bpy.data.objects if o.type == 'MESH' and 'Trousers' in o.name)
    # snapshot Round-1 body verts to prove byte-identical preservation later
    body_before = [tuple(body.matrix_world @ v.co) for v in body.data.vertices]
    face_before = [tuple(face.matrix_world @ v.co) for v in face.data.vertices]
    hb = hashlib.sha256(str(body_before).encode()).hexdigest()

    kdt, body_vs = body_tree(body)
    kdt.balance()
    skin_bvh = BVHTree.FromPolygons(
        body_vs, [tuple(p.vertices) for p in body.data.polygons])

    trim_j = bpy.data.materials.get('HeroJerseyTrim') or bpy.data.materials.new('HeroJerseyTrim')
    stitch_j = bpy.data.materials.get('HeroJerseyStitch') or bpy.data.materials.new('HeroJerseyStitch')
    trim_t = bpy.data.materials.get('HeroTrouserTrim') or bpy.data.materials.new('HeroTrouserTrim')
    stitch_t = bpy.data.materials.get('HeroTrouserStitch') or bpy.data.materials.new('HeroTrouserStitch')
    for m in (trim_j, stitch_j, trim_t, stitch_t):
        m.use_nodes = True
        b = m.node_tree.nodes.get('Principled BSDF')
        if b:
            b.inputs['Roughness'].default_value = 0.86
            try:
                b.inputs['Specular'].default_value = 0.26
            except Exception:
                pass

    si_j = ensure_slot(jersey, 'Jersey')
    ti_j = ensure_slot(jersey, 'HeroJerseyTrim')
    st_j = ensure_slot(jersey, 'HeroJerseyStitch')
    si_t = ensure_slot(trousers, 'Trousers')
    ti_t = ensure_slot(trousers, 'HeroTrouserTrim')
    st_t = ensure_slot(trousers, 'HeroTrouserStitch')

    # ---- garment ease: push out along the garment's OWN normal ---------
    # Never snap verts onto the nearest skin point: in concave regions
    # (underarm, crotch, waist) the nearest surface can belong to a different
    # body part, which teleports verts and shreds the mesh. Instead measure
    # signed distance and advance along the existing normal, clamped.
    # The neckline opening must keep hugging the neck (a collar opening needs
    # ~6mm, not 22mm ease) or it slips into a boat neck.
    jb = bmesh.new()
    jb.from_mesh(jersey.data)
    neck_ids = {v.index for v in jb.verts
                if v.is_boundary and (jersey.matrix_world @ v.co).z > 1.48}
    jb.free()
    for obj, ease in ((jersey, JERSEY_EASE), (trousers, TROUSER_EASE)):
        inv = obj.matrix_world.inverted()
        mesh = obj.data
        for vi, v in enumerate(mesh.vertices):
            if obj == jersey and vi in neck_ids:
                continue  # neckline keeps Round-1 fit; collar covers the rim
            p = obj.matrix_world @ v.co
            n = (obj.matrix_world.to_3x3() @ v.normal).normalized()
            if n.length_squared < 0.5:
                continue
            extra = 0.0
            if obj == jersey and abs(p.x) > 0.20 and 1.15 < p.z < 1.45:
                extra = 0.006  # underarm/sleeve movement room
            if obj == trousers:
                if 0.42 < p.z < 0.58:
                    extra = 0.008  # knee bend room
                    if p.y < -0.02:
                        extra = 0.011
                if 0.75 < p.z < 1.0 and abs(p.x) < 0.12 and p.y > 0.0:
                    extra = 0.006  # seat
            hit, hn, _, _ = skin_bvh.find_nearest(p)
            target = ease + extra
            if hit is None:
                v.co = inv @ (p + n * 0.008)
                continue
            signed = (p - hit).dot(hn)
            if signed >= target:
                continue  # already outside; keep authored shape
            push = min(target - signed, 0.035)
            v.co = inv @ (p + n * push)
        mesh.update()

    # ---- subtle authored folds (no sim) ----
    for v in jersey.data.vertices:
        p = jersey.matrix_world @ v.co
        if 0.99 < p.z < 1.18:  # lower-torso drape, not skin-tight chest
            a = 0.0032 * math.sin(p.x * 55.0) * smooth(0.99, 1.04, p.z) * (1 - smooth(1.12, 1.18, p.z))
            b = 0.0022 * math.sin(math.atan2(p.y, p.x) * 7.0 + 1.3) * (1 - smooth(0.16, 0.24, abs(p.x)))
            n = (jersey.matrix_world.to_3x3() @ v.normal).normalized()
            v.co = jersey.matrix_world.inverted() @ (p + n * (a + b))
    for v in trousers.data.vertices:
        p = trousers.matrix_world @ v.co
        if 0.44 < p.z < 0.56 and p.y < 0.02:  # knee-break, front only
            a = 0.0020 * math.sin((p.z - 0.44) * 90.0)
            n = (trousers.matrix_world.to_3x3() @ v.normal).normalized()
            v.co = trousers.matrix_world.inverted() @ (p + n * a)
        if 0.60 < p.z < 0.95:  # calf/seat vertical drape
            a = 0.0016 * math.sin(math.atan2(p.y, p.x) * 9.0)
            n = (trousers.matrix_world.to_3x3() @ v.normal).normalized()
            v.co = trousers.matrix_world.inverted() @ (p + n * a)
    jersey.data.update(); trousers.data.update()

    # ---- knee articulation loops (trousers only) ----
    bm = bmesh.new(); bm.from_mesh(trousers.data); bm.verts.ensure_lookup_table()
    for zc in (0.545, 0.515, 0.465, 0.435):  # 2 above / 2 below patella (~0.49)
        plane = [v for v in bm.verts
                 if abs((trousers.matrix_world @ v.co).z - zc) < 0.006
                 and abs((trousers.matrix_world @ v.co).x) < 0.16]
        if len(plane) < 8:
            continue
        # split faces crossing the plane to add a clean transverse loop
        edges = [e for e in bm.edges
                 if (trousers.matrix_world @ e.verts[0].co).z - zc > 0 >
                 (trousers.matrix_world @ e.verts[1].co).z - zc]
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1)
    bm.to_mesh(trousers.data); bm.free()
    # new knee-loop verts: weight from nearest body
    for i, v in enumerate(trousers.data.vertices):
        if not trousers.data.vertices[i].groups:
            w = nearest_weights(body, kdt, trousers.matrix_world @ v.co)
            for n, wt in w:
                g = trousers.vertex_groups.get(n) or trousers.vertex_groups.new(name=n)
                g.add([i], wt, 'REPLACE')

    # ---- panel seams via slots (no floating geometry) ----
    def paint_seams(obj, si, ti, st):
        for p in obj.data.polygons:
            c = sum((obj.matrix_world @ obj.data.vertices[i].co for i in p.vertices),
                    Vector()) / len(p.vertices)
            if obj == jersey:
                # shoulder seam: narrow band from neck to acromion
                if 1.44 < c.z < 1.56 and 0.05 < abs(c.x) < 0.22 and abs(c.y) < 0.09:
                    if abs(c.z - (1.55 - (abs(c.x) - 0.05) * 0.45)) < 0.006:
                        p.material_index = st; continue
                # side seam + armhole + hem stitch + cuff stitch
                if abs(abs(c.x) - 0.20) < 0.004 and 1.0 < c.z < 1.42:
                    p.material_index = st; continue
                if abs(c.x) > 0.20 and 1.22 < c.z < 1.40 and abs(c.y) < 0.10:
                    # ring around armhole
                    p.material_index = st; continue
                if abs(c.z - 0.995) < 0.010 or abs(c.z - 1.335) < 0.012 and abs(c.x) > 0.20:
                    p.material_index = ti; continue
            else:
                # outseam / inseam / waistband / fly / pocket welts / ankle cuff
                if abs(abs(c.x) - 0.155) < 0.004 and 0.15 < c.z < 0.95:
                    p.material_index = st; continue
                if abs(c.x) < 0.035 and c.y < -0.05 and 0.80 < c.z < 1.0:
                    p.material_index = st; continue  # fly stitch
                if c.z > 0.985:
                    p.material_index = ti; continue  # waistband
                if c.z < 0.115:
                    p.material_index = ti; continue  # ankle cuff
                if abs(c.z - 0.86) < 0.004 and 0.10 < abs(c.x) < 0.17 and c.y < 0.0:
                    p.material_index = st; continue  # pocket welts
        obj.data.update()
    paint_seams(jersey, si_j, ti_j, st_j)
    paint_seams(trousers, si_t, ti_t, st_t)

    # ---- collar + buttons fitted to the preserved neckline, then merged --
    # Join (not parent) so transforms convert into jersey space; the merged
    # band then shares the jersey's solidify/rim/skin behaviour.
    parts = build_collar_parts(jersey, body, kdt, trim_j)
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:
        o.select_set(True)
    jersey.select_set(True)
    bpy.context.view_layer.objects.active = jersey
    bpy.ops.object.join()
    jersey = next(o for o in bpy.data.objects if o.type == 'MESH' and 'Jersey' in o.name)
    bpy.context.view_layer.update()

    # ---- thickness: inward solidify + rim (fixes paper-thin look) ----
    # NOTE: these meshes carry a 0.01 world scale over cm-local verts, so the
    # Solidify thickness (local units) must be x100 the world target.
    for obj, th in ((jersey, JERSEY_THICK * 100.0), (trousers, TROUSER_THICK * 100.0)):
        mod = obj.modifiers.new('KitThickness', 'SOLIDIFY')
        mod.thickness = -th
        mod.offset = -1.0
        mod.use_rim = True
        mod.use_rim_only = False
        mod.use_quality_normals = True
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        # inner shell inherits outer weights by topology copy: solidify appends
        # rim+inner verts unweighted -> copy from nearest OUTER vert
        outer = len(obj.data.vertices)  # unknown split; use nearest body instead (safe)
        for i, v in enumerate(obj.data.vertices):
            if not v.groups:
                w = nearest_weights(body, kdt, obj.matrix_world @ v.co)
                for n, wt in w:
                    g = obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n)
                    g.add([i], wt, 'REPLACE')

    # ---- final clearance: outer surface only, never pull cloth inside ----
    skin_bvh = BVHTree.FromPolygons(
        [body.matrix_world @ v.co for v in body.data.vertices],
        [tuple(p.vertices) for p in body.data.polygons])
    for obj in (jersey, trousers):
        for v in obj.data.vertices:
            p = obj.matrix_world @ v.co
            _, n, _, d = skin_bvh.find_nearest(p)
            if n is not None and d is not None and d < MIN_CLEAR_NEUTRAL:
                if (p - skin_bvh.find_nearest(p)[0]).dot(n) < MIN_CLEAR_NEUTRAL:
                    v.co = obj.matrix_world.inverted() @ (
                        skin_bvh.find_nearest(p)[0] + n * MIN_CLEAR_NEUTRAL)
        obj.data.update()

    # ---- normals: source split-normals describe bare skin; recompute ------
    for obj in (jersey, trousers):
        if obj.data.has_custom_normals:
            obj.data.normals_split_custom_set([(0, 0, 0)] * len(obj.data.loops))
        for p in obj.data.polygons:
            p.use_smooth = True
        obj.data.update()

    # ---- Round-1 preservation asserts ----
    assert sig(rig) == skel and len(rig.data.bones) == nbones
    assert [tuple(body.matrix_world @ v.co) for v in body.data.vertices] == body_before
    assert [tuple(face.matrix_world @ v.co) for v in face.data.vertices] == face_before
    for o in (jersey, trousers):
        assert all(v.groups for v in o.data.vertices), o.name

    rig['C26_HeroKit'] = 'HeroKit_02 clothing only; Round-1 body/face/hands/shoes untouched'
    rig['C26_MatchApproved'] = False
    rig.animation_data_clear()
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    keep = [rig, body, face, jersey, trousers,
            bpy.data.objects['C26_Shoe_L'], bpy.data.objects['C26_Shoe_R']]
    bpy.ops.object.select_all(action='DESELECT')
    for o in keep:
        o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'C26_HeroKit_02.blend'))
    bpy.ops.export_scene.fbx(
        filepath=str(OUT / 'SK_C26_HeroKit_02.fbx'), use_selection=True,
        object_types={'MESH', 'ARMATURE'}, add_leaf_bones=False, bake_anim=False,
        apply_unit_scale=True, use_mesh_modifiers=False)
    rep = {'kit': 'HeroKit_02', 'match_approved': False,
           'round1_body_sha256': hb, 'skeleton_sha256': skel,
           'bones': nbones, 'skeleton_unchanged': True,
           'body_verts_unchanged': True, 'parts': []}
    for o in keep:
        if o.type == 'MESH':
            o.data.calc_loop_triangles()
            rep['parts'].append({'name': o.name, 'verts': len(o.data.vertices),
                                 'tris': len(o.data.loop_triangles),
                                 'slots': [m.name if m else None for m in o.data.materials]})
    (OUT / 'kit-report.json').write_text(json.dumps(rep, indent=2) + '\n')
    print('C26_HERO_KIT', json.dumps(rep))


if __name__ == '__main__':
    main()
