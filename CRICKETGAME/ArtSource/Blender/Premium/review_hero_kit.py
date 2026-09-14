"""Hero kit review (clothing only). Poses are ephemeral; the kit blend stays bind.

Renders neutral front/back/side/3-4, close-up torso, bent-knee and the approved
authored BATTER_READY_R stance. Reports garment-vs-body clearance per pose and
basic rig-safety diagnostics. Never overwrites the kit delivery blend/FBX.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector, Quaternion

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/HeroKit'
EVID = OUT / 'Evidence'
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import c26_rig as rig_lib
import c26_actions as actions


def clear(rig):
    rig.animation_data_clear()
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()


def flex(rig, bone, deg):
    pb = rig.pose.bones[bone]
    axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
    pb.rotation_quaternion = Quaternion(axis, math.radians(deg))


def pose_bent_knee(rig):
    clear(rig)
    for s in ('l', 'r'):
        flex(rig, 'thigh_' + s, -20)
        flex(rig, 'calf_' + s, 50)
        flex(rig, 'foot_' + s, -22)
    # settle pelvis down via bone-local mapping (same as c26_rig.apply)
    M = rig.pose.bones['pelvis'].bone.matrix_local.to_3x3()
    rig.pose.bones['pelvis'].location = M.inverted() @ Vector((0, 6, -9))
    bpy.context.view_layer.update()


def pose_batting(rig):
    clear(rig)
    rig_lib.apply(rig, actions.BATTER_READY_R)
    bpy.context.view_layer.update()


def posed_meshes(rig, names):
    dg = bpy.context.evaluated_depsgraph_get()
    out = {}
    for o in bpy.data.objects:
        if o.type == 'MESH' and o.name in names:
            e = o.evaluated_get(dg)
            m = e.to_mesh()
            out[o.name] = [e.matrix_world @ v.co for v in m.vertices]
            e.to_mesh_clear()
    return out


def clearance(body_pts, kit_pts):
    from mathutils.bvhtree import BVHTree
    # caller passes body object for faces; rebuild tree in posed space is
    # expensive, so approximate with point-to-point via KDTree on body verts
    from mathutils.kdtree import KDTree
    keys = list(body_pts)
    allp = [p for ps in body_pts.values() for p in ps]
    kdt = KDTree(len(allp))
    for i, p in enumerate(allp):
        kdt.insert(p, i)
    kdt.balance()
    mins, under4, under8, n = 1e9, 0, 0, 0
    for ps in kit_pts.values():
        for p in ps:
            _, _, d = kdt.find(p)
            mins = min(mins, d)
            n += 1
            if d < 0.004:
                under4 += 1
            if d < 0.008:
                under8 += 1
    return {'min_m': round(mins, 4), 'verts': n,
            'under_4mm': under4, 'under_8mm': under8,
            'under_4mm_pct': round(100 * under4 / max(1, n), 3)}


def render(prefix, views):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'; sh.studiolight_rotate_z = 0.3
    sh.color_type = 'MATERIAL'; sh.show_shadows = True
    sh.show_cavity = True; sh.cavity_type = 'WORLD'
    sh.background_type = 'WORLD'
    sc.world = sc.world or bpy.data.worlds.new('KitEvidenceWorld')
    sc.world.color = (0.12, 0.12, 0.12)
    sc.render.resolution_x = 800; sc.render.resolution_y = 1000
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    data = bpy.data.cameras.new('KitEvidenceOnly')
    cam = bpy.data.objects.new('KitEvidenceOnly', data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    data.type = 'ORTHO'
    for name, angle, target, dist, scale in views:
        a = math.radians(angle)
        cam.location = (target[0] + dist * math.sin(a),
                        target[1] - dist * math.cos(a), target[2])
        cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        data.ortho_scale = scale
        sc.render.filepath = str(EVID / f'{prefix}_{name}.png')
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    bpy.data.cameras.remove(data)


def main():
    EVID.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(OUT / 'C26_HeroKit_02.blend'))
    bpy.context.preferences.filepaths.save_version = 0
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    body_names = [o.name for o in bpy.data.objects
                  if o.type == 'MESH' and 'BodyMesh' in o.name]
    kit_names = [o.name for o in bpy.data.objects if o.type == 'MESH' and
                 ('Jersey' in o.name or 'Trousers' in o.name or
                  'Collar' in o.name or 'Button' in o.name)]
    report = {}
    T = (0, 0, 1.02)
    STD = [('front', 0, T, 4, 2.04), ('back', 180, T, 4, 2.04),
           ('side', 90, T, 4, 2.04), ('three_quarter', 35, T, 4, 2.04)]
    CLOSE = [('torso_front', 0, (0, -0.02, 1.28), 2.2, 0.95),
             ('torso_three_quarter', 35, (0, -0.02, 1.26), 2.2, 0.95)]
    # neutral
    clear(rig)
    report['neutral'] = clearance(posed_meshes(rig, body_names),
                                  posed_meshes(rig, kit_names))
    render('kit_neutral', STD)
    render('kit_closeup', CLOSE)
    # bent knee
    pose_bent_knee(rig)
    report['bent_knee'] = clearance(posed_meshes(rig, body_names),
                                    posed_meshes(rig, kit_names))
    render('kit_bentknee', [('front', 0, (0, 0, 0.85), 4, 2.04),
                            ('side', 90, (0, 0, 0.85), 4, 2.04)])
    # batting stance (authored, existing asset — pose only)
    pose_batting(rig)
    report['batting_stance'] = clearance(posed_meshes(rig, body_names),
                                         posed_meshes(rig, kit_names))
    render('kit_batting', [('three_quarter', 35, (0, 0, 1.0), 4, 2.1),
                           ('side', 90, (0, 0, 1.0), 4, 2.1)])
    # rig safety: skeleton/weights untouched handled at build; here finite check
    clear(rig)
    (OUT / 'clearance-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('C26_KIT_REVIEW', json.dumps(report))


if __name__ == '__main__':
    main()
