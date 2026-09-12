"""Render a re-skinned hero body in poses that exercise the joints the game actually drives.

A bind-pose screenshot proves nothing about a skin -- the bind pose is the one pose where broken
weights and good weights look identical. This poses the shared rig into a crouch, an overhead
reach and a batting stance and renders each, so a collapsed shoulder, a leg that does not follow
its knee, or an arm welded to the ribs is visible before the asset costs an engine import and a
capture run.

Cycles, not Workbench or EEVEE: both of those render an empty frame under `--background` on macOS.

Run:
    /Applications/Blender.app/Contents/MacOS/Blender -b -noaudio \
        --python ArtSource/Blender/Characters/render_skin_check.py \
        -- --fbx <path.fbx> --out <dir>
"""
import math
import os
import sys

import bpy
import mathutils

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []


def arg(flag, default):
    return argv[argv.index(flag) + 1] if flag in argv else default


FBX = arg('--fbx', '')
OUT = arg('--out', '/tmp/c26skin')
os.makedirs(OUT, exist_ok=True)


def bare(name):
    return name.replace('mixamorig:', '').replace('mixamorig_', '')


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX)

arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
mesh = max((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: len(o.data.vertices))
arm.data.pose_position = 'POSE'
byname = {bare(b.name): b.name for b in arm.data.bones}

lo = min((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
hi = max((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
height = hi - lo
print('C26_SKIN mesh height %.3f  verts %d  groups %d' % (height, len(mesh.data.vertices), len(mesh.vertex_groups)))


def clear():
    for pb in arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    bpy.context.view_layer.update()


def aim(short, direction):
    """Point a bone along a world direction -- the same roll-independent aim the re-skin uses."""
    pb = arm.pose.bones.get(byname.get(short, ''))
    if not pb:
        return
    have = pb.tail - pb.head
    if have.length < 1e-6:
        return
    rot = have.normalized().rotation_difference(mathutils.Vector(direction).normalized())
    m = pb.matrix.copy()
    head = m.translation.copy()
    m = rot.to_matrix().to_4x4() @ m
    m.translation = head
    pb.matrix = m
    bpy.context.view_layer.update()


def side_sign(short):
    return 1.0 if arm.data.bones[byname[short]].head_local.x > 0 else -1.0


POSES = {}


def pose_rest():
    clear()


def pose_crouch():
    """Knees driven forward and out, arms hanging -- the stance every fielder holds."""
    clear()
    for s in ('Left', 'Right'):
        k = side_sign(s + 'Arm')
        aim(s + 'UpLeg', (k * 0.25, 0.55, -1.0))
        aim(s + 'Leg', (k * 0.15, -0.75, -1.0))
        aim(s + 'Foot', (k * 0.10, 1.0, -0.15))
        aim(s + 'Arm', (k * 0.30, 0.25, -1.0))
        aim(s + 'ForeArm', (k * 0.25, 0.95, -0.55))


def pose_overhead():
    """Both arms straight up. Collapses instantly if the deltoid took spine weights."""
    clear()
    for s in ('Left', 'Right'):
        k = side_sign(s + 'Arm')
        aim(s + 'Arm', (k * 0.22, 0.0, 1.0))
        aim(s + 'ForeArm', (k * 0.10, 0.15, 1.0))
        aim(s + 'Hand', (k * 0.05, 0.15, 1.0))


def pose_bat():
    """A side-on batting stance: front leg planted, hands together at hip height."""
    clear()
    for s in ('Left', 'Right'):
        k = side_sign(s + 'Arm')
        front = k > 0
        aim(s + 'UpLeg', (k * 0.30, 0.45 if front else -0.35, -1.0))
        aim(s + 'Leg', (k * 0.10, -0.30 if front else 0.25, -1.0))
        aim(s + 'Arm', (k * 0.45, 0.55, -0.85))
        aim(s + 'ForeArm', (k * 0.10, 0.9, -0.25))
    aim('Spine1', (0.0, 0.35, 1.0))


POSES = {'1_rest': pose_rest, '2_crouch': pose_crouch, '3_overhead': pose_overhead, '4_bat': pose_bat}

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = 480
scene.render.resolution_y = 720
scene.render.film_transparent = False

world = bpy.data.worlds.new('C26')
scene.world = world
world.use_nodes = True
world.node_tree.nodes['Background'].inputs[0].default_value = (0.05, 0.06, 0.08, 1.0)
world.node_tree.nodes['Background'].inputs[1].default_value = 1.0

key = bpy.data.objects.new('Key', bpy.data.lights.new('Key', 'SUN'))
key.data.energy = 5.0
key.rotation_euler = (math.radians(55), 0.0, math.radians(35))
scene.collection.objects.link(key)

cam_data = bpy.data.cameras.new('Cam')
cam_data.lens = 70
cam = bpy.data.objects.new('Cam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam

# A flat grey floor, so a body sunk into the ground reads as sunk rather than as floating.
bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, lo))

mid = mathutils.Vector((0.0, 0.0, lo + height * 0.52))


def look_from(angle):
    # Far enough back that a pose which throws geometry sideways still lands inside the frame --
    # a spike that leaves the camera is the exact artefact this render exists to show.
    dist = height * 4.0
    pos = mathutils.Vector((math.sin(angle) * dist, -math.cos(angle) * dist, mid.z + height * 0.10))
    cam.location = pos
    cam.rotation_euler = (mid - pos).to_track_quat('-Z', 'Y').to_euler()


for name, fn in sorted(POSES.items()):
    fn()
    for view, angle in (('front', 0.0), ('side', math.radians(80))):
        look_from(angle)
        scene.render.filepath = os.path.join(OUT, '%s_%s.png' % (name, view))
        bpy.ops.render.render(write_still=True)
        print('C26_SKIN wrote', scene.render.filepath)
