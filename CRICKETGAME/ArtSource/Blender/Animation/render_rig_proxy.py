"""Render the authored CRICKET 26 clips as a BONE PROXY stick figure.

The source .blend's body mesh is not usable for motion review: it has no leg geometry and
throws long spikes off unweighted vertices, so a render of it says nothing about whether
the animation is any good. This builds clean tapered boxes along each posed bone instead
and renders those, which is an unambiguous read of the motion itself.

Run:
  Blender --background <authored.blend> --python render_rig_proxy.py -- --out <dir>
"""
import bpy
import math
import os
import sys
import numpy as np
from mathutils import Vector, Matrix

argv = sys.argv
out_dir = '/tmp/c26anim'
if '--' in argv:
    rest = argv[argv.index('--') + 1:]
    if '--out' in rest:
        out_dir = rest[rest.index('--out') + 1]
os.makedirs(out_dir, exist_ok=True)

arm = bpy.data.objects['Armature']
arm.data.pose_position = 'POSE'

master = bpy.context.scene.collection
if arm.name not in master.objects:
    master.objects.link(arm)

scene = bpy.context.scene

# Hide every real mesh: we are judging motion, not geometry.
for o in bpy.data.objects:
    if o.type == 'MESH':
        o.hide_render = True

PROXY_NAME = 'C26BoneProxy'


def bone_frame(head, tail):
    d = (tail - head)
    L = d.length
    if L < 1e-6:
        return None, 0.0
    z = d / L
    ref = Vector((0, 0, 1)) if abs(z.z) < 0.9 else Vector((1, 0, 0))
    x = z.cross(ref).normalized()
    y = z.cross(x).normalized()
    return Matrix((x, y, z)).transposed(), L


def proxy_geometry():
    """Build box geometry for every posed bone at the current frame."""
    dg = bpy.context.evaluated_depsgraph_get()
    ae = arm.evaluated_get(dg)
    mw = arm.matrix_world
    verts = []
    faces = []
    for pb in ae.pose.bones:
        head = mw @ pb.head
        tail = mw @ pb.tail
        rot, L = bone_frame(head, tail)
        if rot is None:
            continue
        r = max(0.016, min(0.030, L * 0.16))
        base = len(verts)
        for t, s in ((0.0, r * 0.55), (0.22, r), (0.78, r), (1.0, r * 0.45)):
            for sx, sy in ((1, 1), (1, -1), (-1, -1), (-1, 1)):
                off = rot @ Vector((sx * s, sy * s, 0.0))
                verts.append(head + (tail - head) * t + off)
        for seg in range(3):
            a = base + seg * 4
            b = base + (seg + 1) * 4
            for i in range(4):
                j = (i + 1) % 4
                faces.append((a + i, a + j, b + j, b + i))
        faces.append((base + 0, base + 3, base + 2, base + 1))
        e = base + 12
        faces.append((e + 0, e + 1, e + 2, e + 3))
    return verts, faces


proxy = bpy.data.meshes.new(PROXY_NAME)
proxy_obj = bpy.data.objects.new(PROXY_NAME, proxy)
scene.collection.objects.link(proxy_obj)
mat = bpy.data.materials.new('C26ProxyMat')
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get('Principled BSDF')
if bsdf:
    bsdf.inputs['Base Color'].default_value = (0.86, 0.55, 0.34, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.62
proxy.materials.append(mat)

# Lighting / world
world = bpy.data.worlds.get('World') or bpy.data.worlds.new('World')
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get('Background')
if bg:
    bg.inputs[0].default_value = (0.55, 0.60, 0.68, 1.0)
    bg.inputs[1].default_value = 1.4
sun_data = bpy.data.lights.new('ProxySun', type='SUN')
sun_data.energy = 4.5
sun = bpy.data.objects.new('ProxySun', sun_data)
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(50), 0.0, math.radians(35))

scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 20
scene.cycles.use_denoising = False
CELL_W, CELL_H = 520, 640
scene.render.resolution_x = CELL_W
scene.render.resolution_y = CELL_H
scene.render.image_settings.file_format = 'PNG'

cam_data = bpy.data.cameras.new('ProxyCam')
cam_data.lens = 50
cam = bpy.data.objects.new('ProxyCam', cam_data)
scene.collection.objects.link(cam)
tgt = bpy.data.objects.new('ProxyTarget', None)
scene.collection.objects.link(tgt)
con = cam.constraints.new('TRACK_TO')
con.target = tgt
con.track_axis = 'TRACK_NEGATIVE_Z'
con.up_axis = 'UP_Y'
scene.camera = cam

JOBS = {
    'A_C26_BattingDrive': [1, 13, 23, 36],
    'A_C26_BowlingPace': [1, 14, 26, 38],
}


def use_action(act):
    arm.animation_data_create()
    arm.animation_data.action = act
    try:
        if getattr(arm.animation_data, 'action_slot', None) is None and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
    except Exception as e:
        print('C26_PROXY slot note: %s' % e)


for action_name, frames in JOBS.items():
    act = bpy.data.actions.get(action_name)
    if act is None:
        print('C26_PROXY missing %s' % action_name)
        continue
    use_action(act)

    cells = []
    for f in frames:
        scene.frame_set(f)
        verts, faces = proxy_geometry()
        proxy.clear_geometry()
        proxy.from_pydata(verts, [], faces)
        proxy.update()
        bpy.context.view_layer.update()

        # Frame on the posed proxy itself. The character faces +Y, so sit the camera
        # front-left of him: that is the angle a cricket stroke actually reads from.
        lo = Vector((min(v.x for v in verts), min(v.y for v in verts), min(v.z for v in verts)))
        hi = Vector((max(v.x for v in verts), max(v.y for v in verts), max(v.z for v in verts)))
        c = (lo + hi) * 0.5
        radius = max((hi - lo).length * 0.5, 0.4)
        fov = 2.0 * math.atan(18.0 / cam_data.lens)
        dist = radius / math.sin(fov * 0.5) * 1.18
        dirv = Vector((0.70, 0.66, 0.26)).normalized()
        cam.location = c + dirv * dist
        tgt.location = c

        p = os.path.join(out_dir, 'proxy_%s_%03d.png' % (action_name, f))
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(p, check_existing=False)
        px = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
        bpy.data.images.remove(img)
        cells.append(px)
        print('C26_PROXY rendered %s frame %d' % (action_name, f))

    n = len(cells)
    sheet = np.zeros((CELL_H, CELL_W * n, 4), dtype=np.float32)
    for i, cc in enumerate(cells):
        h = min(CELL_H, cc.shape[0])
        w = min(CELL_W, cc.shape[1])
        sheet[0:h, i * CELL_W:i * CELL_W + w, :] = cc[0:h, 0:w, :]
    out_img = bpy.data.images.new('proxysheet_' + action_name, width=CELL_W * n, height=CELL_H,
                                  alpha=True, float_buffer=False)
    out_img.pixels = sheet.reshape(-1)
    sp = os.path.join(out_dir, 'PROXY_%s.png' % action_name)
    out_img.filepath_raw = sp
    out_img.file_format = 'PNG'
    out_img.save()
    print('C26_PROXY wrote %s' % sp)

print('C26_PROXY_DONE')
