"""Authoring core for CRICKET 26 cricket actions.

Poses are authored directly on the imported full-body candidate armature, so the clips land on
SK_C26_FullBody_Candidate_Skeleton with no retarget step. That is deliberate: the existing
RTG_C26_MatchToFullBody retargets of the batting/bowling sources come out lying on the ground
because those sources carry a different root frame from A_Run. Authoring on the target rig removes
the whole class of bug rather than tuning around it.

Technique is specified in world measurements (where the front foot plants, where the hands meet the
ball) and resolved by a two-bone solver, because that is how cricket coaching describes a stroke and
because guessed euler angles are what produce stiff mannequin poses.

Authoring space: +X is the athlete's LEFT, +Y is FORWARD, +Z is UP.
The rig itself faces -Y (its toes sit at negative Y relative to the ankles), so `to_rig` below
reflects every authored value onto the rig frame at one single point. Authoring stays readable -
"front foot 30cm forward" is +30 - and nothing downstream has to remember the flip.
Units are CENTIMETRES - the armature object carries a 0.01 scale to metres, so every bone matrix and
every IK target below is in cm (ankle ~8.7, pelvis ~96.8, head ~163.2).
"""
import bpy, math
from mathutils import Vector, Quaternion, Matrix, Euler

D = math.pi / 180.0
FPS = 30

FINGERS = ('thumb', 'index', 'middle', 'ring', 'pinky')
SPINE = ('spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05')


def update():
    bpy.context.view_layer.update()


def load(path, keep_mesh=True):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    if not keep_mesh:
        for o in [o for o in bpy.data.objects if o.type == 'MESH']:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.context.scene.render.fps = FPS
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='POSE')
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    return rig


def rest_len(rig, a, b):
    """Distance between two bone heads in the rest pose - limb segment lengths never change."""
    return (rig.data.bones[b].matrix_local.translation - rig.data.bones[a].matrix_local.translation).length


def clear(rig):
    for pb in rig.pose.bones:
        pb.rotation_quaternion = Quaternion((1, 0, 0, 0))
        pb.location = Vector()
    update()


# ---------------------------------------------------------------- primitive posing

def world_rot(pb, x=0.0, y=0.0, z=0.0):
    """Rotate a bone about armature-space axes, expressed in its own rest frame.

    Child bones inherit the parent's rotation, so a chain behaves like ordinary FK: rotating the
    shoulder carries the forearm and hand with it.
    """
    if not (x or y or z):
        return
    M = pb.bone.matrix_local.to_3x3()
    R = Euler((x * D, y * D, z * D), 'XYZ').to_matrix()
    pb.rotation_quaternion = (M.inverted() @ R @ M).to_quaternion()


def local_rot(pb, x=0.0, y=0.0, z=0.0):
    """Rotate about the bone's own axes. Y runs along the bone, so finger curl is about local X."""
    pb.rotation_quaternion = Euler((x * D, y * D, z * D), 'XYZ').to_quaternion()


def aim(rig, bone, child, target):
    """Rotate `bone` about its head until `child`'s head sits on `target`.

    This rig comes from FBX, so a bone's tail points wherever the exporter left it - upperarm_l's
    tail runs forward, nowhere near the elbow. Limb direction is therefore head-to-child-head, and
    aiming the tail (the usual Blender idiom) would swing every joint into nonsense.
    """
    update()
    pb = rig.pose.bones[bone]
    head = pb.head.copy()
    cur = rig.pose.bones[child].head - head
    des = Vector(target) - head
    if cur.length < 1e-4 or des.length < 1e-4:
        return
    q = cur.rotation_difference(des)
    pb.matrix = (Matrix.Translation(head) @ q.to_matrix().to_4x4()
                 @ Matrix.Translation(-head) @ pb.matrix)
    update()


def two_bone_ik(rig, upper, lower, end, target, pole):
    """Analytic two-bone solve. `pole` steers the joint - knees track forward, elbows track back."""
    update()
    L1 = rest_len(rig, upper, lower)
    L2 = rest_len(rig, lower, end)
    A = rig.pose.bones[upper].head.copy()
    target = Vector(target)
    d = target - A
    dist = min(d.length, (L1 + L2) * 0.999)
    if dist < 1e-3:
        return
    dn = d.normalized()
    # Law of cosines gives the angle to lift the upper segment off the root-to-target line.
    cos_a = max(-1.0, min(1.0, (L1 * L1 + dist * dist - L2 * L2) / (2 * L1 * dist)))
    axis = dn.cross(Vector(pole) - A)
    if axis.length < 1e-3:
        axis = dn.cross(Vector((0, 1, 0)))
    axis.normalize()
    mid = A + (Quaternion(axis, math.acos(cos_a)) @ dn) * L1
    aim(rig, upper, lower, mid)
    aim(rig, lower, end, A + dn * dist)


def grip(rig, side, style):
    """Hand shapes. Every bowling variation gets its own finger and wrist configuration - a
    leg break, googly, top spinner and flipper are different deliveries because the hand is
    different, so reusing one rigid hand across them would be a lie the camera can see."""
    curl, spread, thumb = {
        'open':      ((14, 18, 16), (0, 0, 0, 0, 0), (10, 6, 4)),
        'flat':      ((4, 4, 4), (-2, -1, 1, 2, 0), (4, 2, 2)),
        'bat':       ((62, 74, 58), (-3, -1, 2, 4, 0), (30, 34, 22)),
        'ball':      ((44, 52, 38), (-4, -2, 3, 5, 0), (34, 30, 18)),
        'keeper':    ((22, 26, 20), (-12, -5, 5, 12, 0), (26, 16, 10)),
        # Pace: index and middle stay long either side of the seam, the last two fingers tuck away.
        'seam':      ((10, 12, 10), (-3, -2, 0, 0, 0), (26, 20, 12)),
        'seam_out':  ((10, 12, 10), (-3, -2, 0, 0, 0), (26, 20, 12)),
        'seam_in':   ((10, 12, 10), (-3, -2, 0, 0, 0), (26, 20, 12)),
        'cutter':    ((30, 38, 26), (-14, -7, 4, 6, 0), (30, 24, 14)),
        # Finger spin: index and middle spread wide and bite hard across the seam.
        'offspin':   ((52, 66, 44), (-20, -9, 6, 9, 0), (24, 18, 10)),
        'doosra':    ((48, 60, 40), (-24, -12, 7, 11, 0), (20, 14, 8)),
        # Wrist spin: the third finger does the work, so it curls further than the rest.
        'legbreak':  ((40, 58, 36), (-10, -4, 4, 7, 0), (22, 16, 10)),
        'googly':    ((44, 62, 38), (-12, -5, 5, 8, 0), (20, 15, 9)),
        'topspin':   ((46, 60, 40), (-8, -3, 3, 5, 0), (24, 18, 11)),
        # Flipper is squeezed out from between thumb and the first two fingers.
        'flipper':   ((34, 46, 30), (-6, -3, 2, 3, 0), (52, 44, 26)),
    }[style]
    ring_bias = {'legbreak': 22, 'googly': 26, 'topspin': 14, 'flipper': -10}.get(style, 0)
    for fi, finger in enumerate(FINGERS):
        for seg in (1, 2, 3):
            pb = rig.pose.bones.get(f'{finger}_0{seg}_{side}')
            if not pb:
                continue
            if finger == 'thumb':
                local_rot(pb, -thumb[seg - 1], 0, 0)
                continue
            amount = curl[seg - 1] + (ring_bias if finger == 'ring' else 0)
            local_rot(pb, -amount, 0, spread[fi] if seg == 1 else 0)


# ---------------------------------------------------------------- pose specification

DEFAULT_KNEE_POLE = Vector((0, -120.0, 55.0))
DEFAULT_ELBOW_POLE = Vector((0, 90.0, 105.0))


_ROT_KEYS = ('spine', 'chest', 'neck', 'head')


def R(frac, dx, dy, dz):
    """A limb target as a fraction of that limb's actual reach, in a direction from its own root.

    Authoring in absolute centimetres silently assumes a body size; this athlete's arm is 55cm and
    an absolute target written for a taller player just clamps, which is what turns a ready stance
    into a mannequin with its arms hanging down. Reach fractions stay correct across the body
    presets Phase 28 asks for."""
    return ('R', frac, dx, dy, dz)


def OFF(other, dx, dy, dz):
    """A target placed relative to another resolved target - two hands on one bat handle."""
    return ('OFF', other, dx, dy, dz)


def _resolve(rig, spec, key, root, limb, solved):
    value = spec.get(key)
    if value is None:
        return None
    if isinstance(value, tuple) and value and value[0] == 'R':
        _, frac, dx, dy, dz = value
        d = Vector((dx, -dy, dz))
        d.normalize()
        out = rig.pose.bones[root].head + d * (limb * frac)
    elif isinstance(value, tuple) and value and value[0] == 'OFF':
        _, other, dx, dy, dz = value
        base = solved.get(other)
        if base is None:
            return None
        out = Vector(base) + Vector((dx, -dy, dz))
    else:
        out = Vector(value)
    solved[key] = out
    return out


def to_rig(spec):
    """Authoring notation -> rig frame.

    The rig faces -Y, which reads badly in source ("front foot 30cm forward" would be -30), so
    positions are authored with forward positive and flipped here. Rotations are already written as
    rig-space eulers and pass through untouched; their agreed meaning is:
        +rx leans the body FORWARD, +ry leans it to the athlete's LEFT, +rz turns it LEFT.
    A fully side-on right-handed batter is therefore about rz = -90.
    """
    out = {}
    for key, value in spec.items():
        if key.startswith('grip_'):
            out[key] = value
        elif key == 'pelvis':
            out[key] = tuple(value[:3]) + ((value[3], -value[4], value[5]) if len(value) > 3 else ())
        elif key in _ROT_KEYS or key.startswith(('clav_', 'wrist_', 'ankle_')):
            out[key] = tuple(value)
        elif isinstance(value, tuple) and value and value[0] in ('R', 'OFF'):
            out[key] = value          # already expressed relative to the body
        else:
            out[key] = (value[0], -value[1], value[2])
    return out


def apply(rig, spec):
    """Resolve one pose specification onto the rig.

    Keys: pelvis (rx,ry,rz,dx,dy,dz) | spine | chest | neck | head | clav_l/r |
    hand_l/hand_r (IK target) | elbow_l/elbow_r (pole) | wrist_l/wrist_r (rx,ry,rz after IK) |
    foot_l/foot_r (IK target) | knee_l/knee_r (pole) | ankle_l/ankle_r | grip_l/grip_r
    """
    spec = to_rig(spec)
    clear(rig)
    pelvis = rig.pose.bones['pelvis']
    p = spec.get('pelvis', (0, 0, 0, 0, 0, 0))
    world_rot(pelvis, p[0], p[1], p[2])
    if len(p) > 3:
        # Bone-local translation, so the weight shift travels with the hips rather than the world.
        M = pelvis.bone.matrix_local.to_3x3()
        pelvis.location = M.inverted() @ Vector((p[3], p[4], p[5]))
    update()

    # Spine curvature is distributed, never dumped on one joint - that is what makes a snapped
    # torso read as a mannequin hinging at the waist.
    sx, sy, sz = spec.get('spine', (0, 0, 0))
    cx, cy, cz = spec.get('chest', (0, 0, 0))
    weights = (0.16, 0.20, 0.22, 0.22, 0.20)
    chest_w = (0.0, 0.0, 0.10, 0.38, 0.52)
    for name, w, cw in zip(SPINE, weights, chest_w):
        world_rot(rig.pose.bones[name], sx * w + cx * cw, sy * w + cy * cw, sz * w + cz * cw)
    nx, ny, nz = spec.get('neck', (0, 0, 0))
    world_rot(rig.pose.bones['neck_01'], nx * 0.5, ny * 0.5, nz * 0.5)
    if 'neck_02' in rig.pose.bones:
        world_rot(rig.pose.bones['neck_02'], nx * 0.5, ny * 0.5, nz * 0.5)
    hx, hy, hz = spec.get('head', (0, 0, 0))
    world_rot(rig.pose.bones['head'], hx, hy, hz)
    update()

    for side in ('l', 'r'):
        c = spec.get(f'clav_{side}')
        if c:
            world_rot(rig.pose.bones[f'clavicle_{side}'], *c)
    update()

    arm = rest_len(rig, 'upperarm_l', 'lowerarm_l') + rest_len(rig, 'lowerarm_l', 'hand_l')
    leg = rest_len(rig, 'thigh_l', 'calf_l') + rest_len(rig, 'calf_l', 'foot_l')
    solved = {}

    # A stride only opens up if the hips come down with it. Lowering the pelvis until both feet are
    # reachable produces the vertical oscillation that separates running from gliding, and it means
    # stride length can be authored as technique rather than trimmed to fit the leg.
    for _ in range(4):
        deficit = 0.0
        for side in ('l', 'r'):
            t = _resolve(rig, spec, f'foot_{side}', f'thigh_{side}', leg, solved)
            if t is not None:
                deficit = max(deficit, (rig.pose.bones[f'thigh_{side}'].head - t).length - leg * 0.985)
        if deficit <= 0.05:
            break
        M = pelvis.bone.matrix_local.to_3x3()
        pelvis.location += M.inverted() @ Vector((0, 0, -deficit))
        update()

    # Mirroring swaps the OFF dependency: the left wrist then depends on the
    # right. A fixed left-then-right pass silently omitted the left arm entirely.
    for _ in range(2):
        for side in ('l', 'r'):
            key = f'hand_{side}'
            if key not in solved:
                _resolve(rig, spec, key, f'upperarm_{side}', arm, solved)
    for key in ('hand_l', 'hand_r'):
        if key in spec and key not in solved:
            raise ValueError('Unresolved hand target dependency: ' + key)

    # A two-handed grip is one rigid object. In a closed batting stance the back shoulder sits a
    # third of a metre behind the front one, so a grip placed under the front shoulder is simply
    # out of the back arm's range. Slide the whole grip - both hands together - until both arms can
    # hold it, rather than letting one arm stretch off the handle.
    if any(isinstance(spec.get(k), tuple) and spec[k] and spec[k][0] == 'OFF'
           for k in ('hand_l', 'hand_r')):
        for _ in range(4):
            worst, shift = 0.0, None
            for side in ('l', 'r'):
                t = solved.get(f'hand_{side}')
                if t is None:
                    continue
                root = rig.pose.bones[f'upperarm_{side}'].head
                over = (t - root).length - arm * 0.97
                if over > worst:
                    worst, shift = over, (root - t).normalized() * over
            if shift is None or worst <= 0.05:
                break
            for side in ('l', 'r'):
                if solved.get(f'hand_{side}') is not None:
                    solved[f'hand_{side}'] = solved[f'hand_{side}'] + shift

    for side in ('l', 'r'):
        target = solved.get(f'hand_{side}')
        if target is not None:
            pole = spec.get(f'elbow_{side}')
            if pole is None:
                pole = rig.pose.bones[f'upperarm_{side}'].head + DEFAULT_ELBOW_POLE
            two_bone_ik(rig, f'upperarm_{side}', f'lowerarm_{side}', f'hand_{side}', target, pole)
        w = spec.get(f'wrist_{side}')
        if w:
            world_rot(rig.pose.bones[f'hand_{side}'], *w)
        g = spec.get(f'grip_{side}')
        if g:
            grip(rig, side, g)

    apply.last_targets = solved
    for side in ('l', 'r'):
        target = solved.get(f'foot_{side}')
        if target is not None:
            pole = spec.get(f'knee_{side}')
            if pole is None:
                pole = rig.pose.bones[f'thigh_{side}'].head + DEFAULT_KNEE_POLE
            two_bone_ik(rig, f'thigh_{side}', f'calf_{side}', f'foot_{side}', target, pole)
        # A planted shoe must not inherit the bent calf's pitch. Preserve the
        # authored ankle position, orient to the canonical ground frame, then
        # apply any deliberate heel/toe roll in armature space.
        foot = rig.pose.bones[f'foot_{side}']
        rotation = foot.bone.matrix_local.to_quaternion()
        a = spec.get(f'ankle_{side}')
        if a:
            rotation = Euler(tuple(value * D for value in a), 'XYZ').to_quaternion() @ rotation
        foot.matrix = Matrix.LocRotScale(foot.head.copy(), rotation, Vector((1, 1, 1)))
    update()


# ---------------------------------------------------------------- mirroring

def _swap(name):
    if name.endswith('_l'):
        return name[:-2] + '_r'
    if name.endswith('_r'):
        return name[:-2] + '_l'
    return name


def mirror(spec):
    """Reflect a pose through the sagittal plane.

    Valid for cricket because the stroke and the bowling action genuinely reverse for a left-hander;
    the bat and ball swap hands via the profile's LeftHandedSocket rather than by bending the wrist
    backwards. Reflection maps a rotation (rx,ry,rz) to (rx,-ry,-rz) and negates X positions.
    """
    out = {}
    for key, value in spec.items():
        k = _swap(key) if (key[-2:] in ('_l', '_r')) else key
        if key.startswith('grip_'):
            out[k] = value
        elif isinstance(value, tuple) and value and value[0] == 'R':
            out[k] = ('R', value[1], -value[2], value[3], value[4])
        elif isinstance(value, tuple) and value and value[0] == 'OFF':
            out[k] = ('OFF', _swap(value[1]), -value[2], value[3], value[4])
        elif key == 'pelvis':
            out[k] = (value[0], -value[1], -value[2]) + ((-value[3], value[4], value[5]) if len(value) > 3 else ())
        elif key in ('spine', 'chest', 'neck', 'head') or key.startswith(('clav_', 'wrist_', 'ankle_')):
            out[k] = (value[0], -value[1], -value[2])
        else:  # IK targets and poles are positions
            out[k] = (-value[0], value[1], value[2])
    return out


# ---------------------------------------------------------------- clip baking

def key_bones(rig):
    names = [b.name for b in rig.data.bones
             if not any(t in b.name for t in ('_twist', 'corrective', '_scl', '_bck', '_fwd',
                                              '_in_', '_out_', 'kneeBack', '_knee_', 'latissimus',
                                              'bicep', 'tricep', 'scap', 'pec', 'toe_'))]
    return [n for n in names if n in rig.pose.bones]


def _fcurves(action):
    """Blender 4.4+ moved f-curves out of Action.fcurves and into layer/strip channel bags."""
    if hasattr(action, 'layers') and action.layers:
        for layer in action.layers:
            for strip in layer.strips:
                for bag in getattr(strip, 'channelbags', []):
                    yield from bag.fcurves
    elif hasattr(action, 'fcurves'):
        yield from action.fcurves


def bake(rig, name, keys, loop=False):
    """Write one action. `keys` is [(frame, spec), ...]; the pose at each frame is resolved and
    stamped onto every controlled bone so interpolation never drifts through an unkeyed joint."""
    if rig.animation_data is None:
        rig.animation_data_create()
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    if hasattr(action, 'slots'):  # Blender 4.4+ slotted actions
        slot = action.slots.new(id_type='OBJECT', name=rig.name)
        rig.animation_data.action_slot = slot
    bones = key_bones(rig)
    if loop and keys[0][1] != keys[-1][1]:
        keys = list(keys) + [(keys[-1][0] + (keys[1][0] - keys[0][0]), keys[0][1])]
    for frame, spec in keys:
        apply(rig, spec)
        for n in bones:
            pb = rig.pose.bones[n]
            pb.keyframe_insert('rotation_quaternion', frame=frame, group=n)
            if n == 'pelvis':
                pb.keyframe_insert('location', frame=frame, group=n)
    for fc in _fcurves(action):
        for kp in fc.keyframe_points:
            kp.interpolation = 'BEZIER'
            kp.handle_left_type = kp.handle_right_type = 'AUTO_CLAMPED'
    bpy.context.scene.frame_start = int(keys[0][0])
    bpy.context.scene.frame_end = int(keys[-1][0])
    return action


def export(rig, action, path):
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.animation_data.action = action
    if hasattr(action, 'slots') and action.slots:
        rig.animation_data.action_slot = action.slots[0]
    start, end = action.frame_range
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = int(start), int(end)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    for o in bpy.data.objects:
        if o.type == 'MESH':
            o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(
        filepath=str(path), use_selection=True, object_types={'MESH', 'ARMATURE'},
        add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
        bake_anim_step=1.0, apply_unit_scale=True, use_mesh_modifiers=False)
    bpy.ops.object.mode_set(mode='POSE')
