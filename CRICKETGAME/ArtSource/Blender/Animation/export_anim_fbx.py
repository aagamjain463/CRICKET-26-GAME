"""Export the authored CRICKET 26 actions as one FBX each, for import as AnimSequences.

One file per action on purpose: Unreal's FBX animation import binds a file to one skeleton
and takes the takes inside it, so a single file per clip keeps the import unambiguous and
lets a clip be re-exported without touching the others.

Run:
  Blender --background <authored.blend> --python export_anim_fbx.py -- --out <dir>
"""
import bpy
import os
import sys

argv = sys.argv
out_dir = '/tmp/c26anim'
if '--' in argv:
    rest = argv[argv.index('--') + 1:]
    if '--out' in rest:
        out_dir = rest[rest.index('--out') + 1]
os.makedirs(out_dir, exist_ok=True)

arm = bpy.data.objects['Armature']
arm.data.pose_position = 'POSE'

# This file keeps its objects in a collection that is not linked into the scene, so the
# armature cannot even be selected for export until it is linked.
master = bpy.context.scene.collection
if arm.name not in master.objects:
    master.objects.link(arm)

# Only the armature goes out: the clips are pure skeletal animation, and this file's
# body mesh is not in a state worth shipping.
for o in bpy.data.objects:
    o.select_set(False)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm

JOBS = {
    'A_C26_BattingDrive': (1, 36),
    'A_C26_BowlingPace': (1, 46),
}

for name, (f0, f1) in JOBS.items():
    act = bpy.data.actions.get(name)
    if act is None:
        print('C26_ANIMEXP missing action %s' % name)
        continue
    arm.animation_data_create()
    arm.animation_data.action = act
    try:
        if getattr(arm.animation_data, 'action_slot', None) is None and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
    except Exception:
        pass

    bpy.context.scene.frame_start = f0
    bpy.context.scene.frame_end = f1

    path = os.path.join(out_dir, name + '.fbx')
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        apply_unit_scale=True,
        global_scale=1.0,
        object_types={'ARMATURE'},
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        bake_space_transform=False,
        use_armature_deform_only=False,
        primary_bone_axis='Y',
        secondary_bone_axis='X',
    )
    print('C26_ANIMEXP wrote %s' % path)

print('C26_ANIMEXP_DONE')
