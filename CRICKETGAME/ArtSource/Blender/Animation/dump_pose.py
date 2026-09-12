"""Numeric read-out of the authored poses: world-space positions of the bones that
define whether a cricket action reads correctly. Numbers, not squinting at renders.

Run: Blender --background <authored.blend> --python dump_pose.py
"""
import bpy
from mathutils import Vector

arm = bpy.data.objects['Armature']
arm.data.pose_position = 'POSE'
master = bpy.context.scene.collection
if arm.name not in master.objects:
    master.objects.link(arm)

WATCH = ['mixamorig:Hips', 'mixamorig:Head', 'mixamorig:LeftHand', 'mixamorig:RightHand',
         'mixamorig:LeftFoot', 'mixamorig:RightFoot', 'mixamorig:LeftToeBase',
         'mixamorig:RightToeBase']

JOBS = {
    'A_C26_BattingDrive': [1, 7, 13, 18, 23, 29, 36],
    'A_C26_BowlingPace': [1, 8, 14, 20, 26, 31, 38, 46],
}


def use_action(act):
    arm.animation_data_create()
    arm.animation_data.action = act
    try:
        if getattr(arm.animation_data, 'action_slot', None) is None and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
    except Exception:
        pass


for name, frames in JOBS.items():
    act = bpy.data.actions.get(name)
    if act is None:
        continue
    use_action(act)
    print('\n=== %s ===' % name)
    print('%-5s %-26s %-26s %-26s %-26s' % ('frm', 'HIPS', 'HEAD', 'LEFTHAND', 'RIGHTHAND'))
    for f in frames:
        bpy.context.scene.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get()
        ae = arm.evaluated_get(dg)
        mw = arm.matrix_world

        def P(n):
            pb = ae.pose.bones.get(n)
            if pb is None:
                return None
            return mw @ pb.head

        hips = P('mixamorig:Hips')
        head = P('mixamorig:Head')
        lh = P('mixamorig:LeftHand')
        rh = P('mixamorig:RightHand')
        lf = P('mixamorig:LeftFoot')
        rf = P('mixamorig:RightFoot')

        def fmt(v):
            return '(%.2f,%.2f,%.2f)' % (v.x, v.y, v.z) if v else 'n/a'

        print('%-5d %-26s %-26s %-26s %-26s' % (f, fmt(hips), fmt(head), fmt(lh), fmt(rh)))
        print('      feet  L=%s R=%s   head_above_hips=%+.3f  stance_width=%.3f'
              % (fmt(lf), fmt(rf), (head.z - hips.z), abs(lf.x - rf.x)))
print('\nC26_POSEDUMP_DONE')
