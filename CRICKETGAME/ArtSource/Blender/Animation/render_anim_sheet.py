"""Render every key pose of the authored CRICKET 26 clips into one contact sheet per action,
so a human (or an agent reading pixels) can actually judge the motion instead of trusting it.

Run:
  Blender --background <authored.blend> --python render_anim_sheet.py -- --out <dir>
"""
import bpy
import os
import sys
import math
import numpy as np
from mathutils import Vector

argv = sys.argv
out_dir = '/tmp/c26anim'
if '--' in argv:
    rest = argv[argv.index('--') + 1:]
    if '--out' in rest:
        out_dir = rest[rest.index('--out') + 1]
os.makedirs(out_dir, exist_ok=True)

arm = bpy.data.objects['Armature']
arm.data.pose_position = 'POSE'

# This file keeps its character objects in a collection that is NOT linked into the scene
# (view_layer.objects is empty on open), so nothing renders until they are linked. Do that
# first, or every frame comes back as bare background.
master = bpy.context.scene.collection
for o in bpy.data.objects:
    if o.name in ('Camera', 'Light', 'Cube', 'Icosphere'):
        continue
    if o.name not in master.objects:
        try:
            master.objects.link(o)
        except Exception as e:
            print('C26_SHEET link note for %s: %s' % (o.name, e))

# Drop the file's default clutter so it can't photobomb the sheet.
for name in ('Cube', 'Icosphere', 'Camera', 'Light'):
    o = bpy.data.objects.get(name)
    if o:
        o.hide_render = True

scene = bpy.context.scene


def world_bounds():
    """Union world-space AABB of every renderable mesh, evaluated (posed) at the
    current frame. This file's meshes carry very different object scales under
    parent-inverse matrices, so the only trustworthy way to frame them is to
    measure where they actually land."""
    dg = bpy.context.evaluated_depsgraph_get()
    lo = [1e18] * 3
    hi = [-1e18] * 3
    for o in bpy.data.objects:
        if o.type != 'MESH' or o.hide_render:
            continue
        ob = o.evaluated_get(dg)
        try:
            me = ob.to_mesh()
        except Exception:
            continue
        mw = ob.matrix_world
        for v in me.vertices:
            w = mw @ v.co
            for i in range(3):
                lo[i] = min(lo[i], w[i])
                hi[i] = max(hi[i], w[i])
        ob.to_mesh_clear()
    return Vector(lo), Vector(hi)


lo, hi = world_bounds()
centre = (lo + hi) * 0.5
size = (hi - lo)
print('C26_SHEET world_bounds lo=%s hi=%s size=%s'
      % (tuple(round(v, 3) for v in lo), tuple(round(v, 3) for v in hi),
         tuple(round(v, 3) for v in size)))

cam_data = bpy.data.cameras.new('C26SheetCam')
cam_data.lens = 50
cam = bpy.data.objects.new('C26SheetCam', cam_data)
scene.collection.objects.link(cam)

# Sit back along +X/+Y at eye height and pull back far enough for the tallest axis.
reach = max(size.z, size.x, size.y) * 2.15
cam.location = centre + Vector((reach * 0.72, reach * 0.68, size.z * 0.16))

tgt = bpy.data.objects.new('C26SheetTarget', None)
scene.collection.objects.link(tgt)
tgt.location = centre
con = cam.constraints.new('TRACK_TO')
con.target = tgt
con.track_axis = 'TRACK_NEGATIVE_Z'
con.up_axis = 'UP_Y'
scene.camera = cam

scene.render.engine = 'CYCLES'
# Workbench/EEVEE need a GPU context, which `--background` does not give us on macOS
# (renders come back empty). Cycles is the only engine that reliably renders headless here.
scene.cycles.device = 'CPU'
scene.cycles.samples = 24
scene.cycles.use_denoising = False
scene.cycles.max_bounces = 4
scene.render.film_transparent = False

world = bpy.data.worlds.get('World') or bpy.data.worlds.new('World')
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get('Background')
if bg:
    bg.inputs[0].default_value = (0.62, 0.66, 0.72, 1.0)
    bg.inputs[1].default_value = 1.5

sun_data = bpy.data.lights.new('C26SheetSun', type='SUN')
sun_data.energy = 4.0
sun = bpy.data.objects.new('C26SheetSun', sun_data)
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(52), 0.0, math.radians(38))

CELL_W, CELL_H = 380, 520
scene.render.resolution_x = CELL_W
scene.render.resolution_y = CELL_H
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'

JOBS = {
    'A_C26_BattingDrive': [1, 7, 13, 18, 23, 29, 36],
    'A_C26_BowlingPace': [1, 8, 14, 20, 26, 31, 38, 46],
}


def use_action(act):
    arm.animation_data_create()
    arm.animation_data.action = act
    # 4.4+ slotted actions: bind the slot explicitly or the rig evaluates at rest.
    try:
        if getattr(arm.animation_data, 'action_slot', None) is None and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
    except Exception as e:
        print('C26_SHEET slot bind note: %s' % e)


def render_cell(path, frame):
    scene.frame_set(frame)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path, check_existing=False)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
    bpy.data.images.remove(img)
    return px


for action_name, frames in JOBS.items():
    act = bpy.data.actions.get(action_name)
    if act is None:
        print('C26_SHEET missing action %s' % action_name)
        continue
    use_action(act)

    cells = []
    for f in frames:
        p = os.path.join(out_dir, 'cell_%s_%03d.png' % (action_name, f))
        cells.append(render_cell(p, f))
        print('C26_SHEET rendered %s frame %d' % (action_name, f))

    # Contact sheet: cells left-to-right, earliest pose first.
    n = len(cells)
    sheet = np.zeros((CELL_H, CELL_W * n, 4), dtype=np.float32)
    for i, c in enumerate(cells):
        h = min(CELL_H, c.shape[0])
        w = min(CELL_W, c.shape[1])
        sheet[0:h, i * CELL_W:i * CELL_W + w, :] = c[0:h, 0:w, :]

    out_img = bpy.data.images.new('sheet_' + action_name, width=CELL_W * n, height=CELL_H,
                                  alpha=True, float_buffer=False)
    out_img.pixels = sheet.reshape(-1)
    sheet_path = os.path.join(out_dir, 'SHEET_%s.png' % action_name)
    out_img.filepath_raw = sheet_path
    out_img.file_format = 'PNG'
    out_img.save()
    print('C26_SHEET wrote %s' % sheet_path)

print('C26_SHEET_DONE')
