"""Reproducible geometry-only review; no game camera/material/animation changes.

Outputs neutral review .blend, four matched before/after PNG pairs and structural
deformation diagnostics. Bind-pose delivery .blend/FBX are never overwritten.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/HeroSilhouette'
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from c26_rig import aim, clear


def neutral(rig):
    rig.animation_data_clear()
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    # Pure FK rotations; lengths/reference transforms never change. A slight
    # arm clearance keeps the outline readable without a presentation T-pose.
    for side, sign in [('l', 1), ('r', -1)]:
        upper = rig.pose.bones['upperarm_' + side]
        elbow = rig.pose.bones['lowerarm_' + side]
        wrist = rig.pose.bones['hand_' + side]
        l1 = (elbow.head - upper.head).length
        l2 = (wrist.head - elbow.head).length
        aim(rig, upper.name, elbow.name,
            upper.head + Vector((sign * .15, -.015, -.9886)).normalized() * l1)
        aim(rig, elbow.name, wrist.name,
            elbow.head + Vector((sign * .12, -.10, -.9877)).normalized() * l2)
    bpy.context.view_layer.update()


def setup_view(angle=0):
    rotation = Quaternion((0, 0, 1), math.radians(angle)) @ Quaternion((1, 0, 0), math.pi / 2)
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                v = area.spaces.active
                v.region_3d.view_rotation = rotation
                v.region_3d.view_location = Vector((0, 0, .91))
                v.region_3d.view_distance = 2.55
                v.region_3d.view_perspective = 'ORTHO'
                v.overlay.show_overlays = False
                v.shading.type = 'SOLID'
                v.shading.color_type = 'SINGLE'
                v.shading.single_color = (.55, .55, .55)


def posed_points(meshes):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    result = {}
    for obj in meshes:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        result[obj.name] = [evaluated.matrix_world @ v.co for v in mesh.vertices]
        evaluated.to_mesh_clear()
    return result


def deformation_checks(rig, meshes):
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    rest = posed_points(meshes)
    results = []
    for label in ('neutral', 'elbow_90', 'knee_65', 'shoulder_75'):
        neutral(rig)
        if label != 'neutral':
            bone, degrees = {'elbow_90': ('lowerarm_l', 90),
                             'knee_65': ('calf_l', 65),
                             'shoulder_75': ('upperarm_l', 75)}[label]
            pb = rig.pose.bones[bone]
            axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
            pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(axis, math.radians(degrees))
        bpy.context.view_layer.update()
        points = posed_points(meshes)
        finite = all(math.isfinite(c) for ps in points.values() for p in ps for c in p)
        ratios = []
        for obj in meshes:
            a, b = rest[obj.name], points[obj.name]
            for edge in obj.data.edges:
                i, j = edge.vertices
                length = (a[i] - a[j]).length
                if length > .0005:
                    ratios.append((b[i] - b[j]).length / length)
        ratios.sort()
        assert finite, 'Nonfinite posed geometry'
        results.append({'pose': label, 'finite': finite,
            'edge_stretch_p99': round(ratios[int(len(ratios) * .99)], 4),
            'edge_stretch_max': round(max(ratios), 4),
            'note': 'Numerical diagnostic, not a self-intersection or animation-quality certification'})
    neutral(rig)
    return results


def render_views(prefix):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.studiolight_rotate_z = .3
    shading.color_type = 'SINGLE'
    shading.single_color = (.55, .55, .55)
    shading.show_shadows = True
    shading.show_cavity = True
    shading.cavity_type = 'WORLD'
    shading.curvature_ridge_factor = 1.1
    shading.curvature_valley_factor = .8
    shading.background_type = 'WORLD'
    scene.world = scene.world or bpy.data.worlds.new('SilhouetteEvidenceWorld')
    scene.world.color = (.12, .12, .12)
    scene.render.resolution_x = 800
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    # Ephemeral DCC inspection camera. Saved neutral file contains no camera.
    data = bpy.data.cameras.new('SilhouetteEvidenceOnly')
    camera = bpy.data.objects.new('SilhouetteEvidenceOnly', data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    data.type = 'ORTHO'
    data.ortho_scale = 2.04
    for name, angle in [('front', 0), ('side', 90), ('back', 180), ('three_quarter', 35)]:
        a = math.radians(angle)
        camera.location = (4 * math.sin(a), -4 * math.cos(a), .91)
        camera.rotation_euler = (Vector((0, 0, .91)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(OUT / 'Evidence' / f'{prefix}_{name}.png')
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(data)


def main():
    (OUT / 'Evidence').mkdir(parents=True, exist_ok=True)
    report = {}
    for label, path in [
            ('before', ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend'),
            ('hero', OUT / 'C26_HeroMale_Silhouette_01.blend')]:
        bpy.ops.wm.open_mainfile(filepath=str(path))
        bpy.context.preferences.filepaths.save_version = 0
        rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        report[label] = deformation_checks(rig, meshes)
        setup_view()
        if label == 'hero':
            bpy.context.scene['C26_Inspection'] = 'NEUTRAL REVIEW ONLY. Numpad 1/3/Ctrl-1; Numpad 6 for 3/4. Bind/FBX in sibling files.'
            bpy.ops.object.select_all(action='DESELECT')
            rig.select_set(True)
            bpy.context.view_layer.objects.active = rig
            bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'C26_HeroMale_Silhouette_01_Neutral.blend'))
        render_views(label)
    (OUT / 'deformation-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('C26_SILHOUETTE_REVIEW', json.dumps(report))


if __name__ == '__main__':
    main()
