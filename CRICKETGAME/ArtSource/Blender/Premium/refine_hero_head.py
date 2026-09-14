"""Authored, millimetre-scale anatomical edits to ONE Player_001 variant.

No remeshing, new rig, material editing, body edits or runtime dependencies.
Always rebuilds from the unchanged Athlete_Review source, never accumulates edits.
"""
import hashlib
import json
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend'
OUT = ROOT / 'ArtSource/Premium/HeroHead'
FACE = 'SKM_MH_C26_Player_001_FaceMesh.001'


def smooth(a, b, x):
    t = max(0., min(1., (x - a) / (b - a)))
    return t * t * (3. - 2. * t)


def influence(p, center, radius):
    d = sum(((p[i] - center[i]) / radius[i]) ** 2 for i in range(3))
    # Compact support: exact zero outside three radii, no distant neck drift.
    return math.exp(-2. * d) * (1. - smooth(4., 9., d))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def mesh_contract(obj, geometry=True):
    mesh = obj.data
    result = {'matrix': [list(row) for row in obj.matrix_world],
              'polygons': [(list(p.vertices), p.material_index, p.use_smooth) for p in mesh.polygons],
              'groups': [g.name for g in obj.vertex_groups],
              'weights': [[(g.group, g.weight) for g in v.groups] for v in mesh.vertices],
              'uv': {uv.name: [list(v.uv) for v in uv.data] for uv in mesh.uv_layers},
              'materials': [m.name if m else None for m in mesh.materials],
              'parent': obj.parent.name if obj.parent else None,
              'modifiers': [(m.name, m.type, m.object.name if m.type == 'ARMATURE' and m.object else None)
                            for m in obj.modifiers]}
    if geometry:
        result['vertices'] = [list(v.co) for v in mesh.vertices]
    return digest(result)


def main():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    OUT.mkdir(parents=True, exist_ok=True)
    face = bpy.data.objects[FACE]
    assert face.data.shape_keys is None, 'Review new facial shape keys before editing basis geometry'
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    rig_before = digest([(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local)))
                         for b in rig.data.bones])
    untouched = {o.name: mesh_contract(o) for o in bpy.data.objects if o.type == 'MESH' and o != face}
    topology_before = mesh_contract(face, geometry=False)
    original = [face.matrix_world @ v.co for v in face.data.vertices]
    sections = {s: {v for p in face.data.polygons if p.material_index == s for v in p.vertices}
                for s in range(len(face.data.materials))}
    membership = {i: s for s, ids in sections.items() for i in ids}
    eyes = {}
    for section in (3, 4):
        points = np.array([original[i] for i in sections[section]])
        fit = np.linalg.lstsq(np.column_stack((2 * points, np.ones(len(points)))),
                             np.sum(points * points, axis=1), rcond=None)[0]
        center = Vector(fit[:3])
        radius = math.sqrt(fit[3] + fit[:3] @ fit[:3])
        eyes[section] = (center, radius)
    # All dimensions below are metres. Existing left/right individuality is retained.
    eye_scale = .92
    eye_recess = Vector((0, .0014, 0))
    inverse = face.matrix_world.inverted()
    changed = []
    max_move = 0.
    for v, p in zip(face.data.vertices, original):
        section = membership.get(v.index)
        q = p.copy()
        if p.z <= 1.59:
            continue  # Exact frozen attachment band, including all neck boundary vertices.
        if section in (3, 4):
            c, _ = eyes[section]
            q = c + eye_recess + (p - c) * eye_scale
        else:
            # Transport the socket, actual lid thickness, tear-line, occlusion and lash
            # attachments together. Never shrink a globe alone inside a fixed lid opening.
            es = 3 if p.x > 0 else 4
            c, _ = eyes[es]
            distance = (p - c).length
            eye_weight = 1. - smooth(.0185, .034, distance)
            if eye_weight and section in (0, 5, 6, 7, 8, 9):
                q += ((p - c) * (eye_scale - 1.) + eye_recess) * eye_weight
                nc = c + eye_recess
                before_lid = q.copy()
                upper = influence(p, (c.x, -.109, c.z + .0043), (.013, .012, .0048))
                lower = influence(p, (c.x, -.108, c.z - .0045), (.013, .012, .0037))
                q.z += -.00110 * upper + .00030 * lower
                # Preserve radial clearance/thickness as the lid slides on the sphere.
                # Front hemisphere only; no artificial flattening of the inner socket.
                if before_lid.y < nc.y:
                    radial_sq = (before_lid - nc).length_squared
                    yz = radial_sq - (q.x - nc.x) ** 2 - (q.z - nc.z) ** 2
                    if yz > 0:
                        q.y = nc.y - math.sqrt(yz)
        if section == 0:
            delta = Vector((0, 0, 0))
            # Vault: less juvenile dome, slightly flatter frontal plane; no lower-face scaling.
            crown = smooth(1.735, 1.817, p.z)
            delta.z -= .0080 * crown
            delta.x -= p.x * .025 * crown
            delta.y += .0012 * influence(p, (0, -.106, 1.758), (.065, .035, .031))
            # Paired anatomical landmarks; independent falloffs preserve surface continuity.
            for side in (-1, 1):
                landmarks = [
                    # Mandibular angle and lateral chin: broader, readable jaw plane.
                    ((side * .058, -.029, 1.625), (.025, .054, .027), (side * .0055, -.0008, -.0006)),
                    ((side * .018, -.112, 1.609), (.017, .024, .016), (side * .0016, -.0011, -.0003)),
                    # Zygomatic prominence and restrained submalar hollow.
                    ((side * .054, -.083, 1.691), (.021, .030, .020), (side * .0024, -.0024, .0008)),
                    ((side * .047, -.084, 1.660), (.025, .029, .018), (-side * .0010, .0012, 0)),
                    # Supraorbital ridge sits lower over the relaxed upper lid.
                    ((side * .029, -.112, 1.732), (.024, .019, .009), (0, -.0020, -.0008)),
                    # Alar volume: narrow a little, retain nostril rim and columella.
                    ((side * .0125, -.129, 1.676), (.010, .016, .010), (-side * .0011, 0, 0)),
                    # Mouth corners flow into the muzzle instead of a tiny central mouth.
                    ((side * .022, -.119, 1.644), (.011, .017, .013), (side * .0014, .0002, 0)),
                ]
                for center, radius, movement in landmarks:
                    delta += Vector(movement) * influence(p, center, radius)
            # Chin projection, bridge, tip and vermilion volume are separate controls.
            for center, radius, movement in [
                ((0, -.119, 1.610), (.025, .022, .016), (0, -.0032, -.0004)),
                ((0, -.120, 1.705), (.009, .017, .020), (0, -.0014, 0)),
                ((0, -.139, 1.681), (.011, .013, .011), (0, .0016, -.0005)),
                ((0, -.125, 1.651), (.022, .012, .006), (0, .00090, -.0003)),
                ((0, -.124, 1.639), (.022, .012, .006), (0, .00110, .0002)),
            ]:
                delta += Vector(movement) * influence(p, center, radius)
            # Slightly tuck the pinna without changing its authored helix/antihelix folds.
            ear = smooth(.072, .087, abs(p.x)) * influence(p, (p.x, -.001, 1.691), (.1, .043, .040))
            delta.x -= math.copysign(.0020 * ear, p.x)
            delta.y += .0009 * ear
            # Exact zero at the frozen head/neck band, C1-continuous above it.
            q += delta * smooth(1.59, 1.605, p.z)
        movement = (q - p).length
        if movement > 1e-9:
            v.co = inverse @ q
            changed.append(v.index)
            max_move = max(max_move, movement)
    face.data.update()
    # Recompute this reshaped face's split normals, not body or material implementation.
    face.data.normals_split_custom_set([(0., 0., 0.)] * len(face.data.loops))
    assert mesh_contract(face, geometry=False) == topology_before, 'Topology/UV/skin contract changed'
    for name, fingerprint in untouched.items():
        assert mesh_contract(bpy.data.objects[name]) == fingerprint, 'Out-of-scope change: ' + name
    assert digest([(b.name, b.parent.name if b.parent else None, list(map(list, b.matrix_local)))
                   for b in rig.data.bones]) == rig_before
    frozen = [i for i, p in enumerate(original) if p.z <= 1.59]
    assert all((face.matrix_world @ face.data.vertices[i].co - original[i]).length == 0 for i in frozen)
    # Store edit selections as metadata, not extra deform groups or runtime blendshapes.
    face['C26.HeadAnatomy'] = 'Hero_001 anatomy v2; topology/UV/weights preserved; neck band <=1.59m frozen'
    face['C26.Source'] = str(SOURCE.relative_to(ROOT))
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.data.objects:
        if obj.type in {'MESH', 'ARMATURE'}:
            obj.select_set(True)
    bpy.context.view_layer.objects.active = rig
    # Convenient head framing in the saved DCC file; no camera objects are created.
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.region_3d.view_location = (0, -.04, 1.704)
                area.spaces.active.region_3d.view_distance = .42
                area.spaces.active.overlay.show_overlays = False
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'C26_HeroHead_001.blend'))
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_C26_HeroHead_001.fbx'), use_selection=True,
        object_types={'MESH', 'ARMATURE'}, add_leaf_bones=False, bake_anim=False,
        apply_unit_scale=True, use_mesh_modifiers=False)
    report = {'source': str(SOURCE.relative_to(ROOT)), 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
              'hero': 'Player_001', 'changed_object': FACE, 'changed_vertices': len(changed),
              'max_displacement_mm': max_move * 1000, 'frozen_neck_vertices': len(frozen),
              'face_topology_uv_weights_material_slots_sha256': topology_before,
              'unchanged_mesh_contracts': untouched, 'unchanged_rig_sha256': rig_before,
              'face_vertices': len(face.data.vertices), 'face_triangles': len(face.data.polygons),
              'eye_scale': eye_scale, 'eye_recess_mm': round(eye_recess.y * 1000, 3),
              'eyes': {s: {'center_before_m': list(c), 'center_after_m': list(c + eye_recess),
                           'diameter_before_mm': r * 2000, 'diameter_after_mm': r * eye_scale * 2000}
                       for s, (c, r) in eyes.items()},
              'visual_acceptance': 'pending independent four-view inspection',
              'facial_animation': 'No shape keys in source. Existing neck/head weights preserved.'}
    (OUT / 'anatomy-change-report.json').write_text(json.dumps(report, indent=2))
    print('C26_HERO_HEAD', json.dumps(report))


if __name__ == '__main__':
    main()
