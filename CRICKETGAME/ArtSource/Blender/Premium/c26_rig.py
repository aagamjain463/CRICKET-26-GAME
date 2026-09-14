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


def make_from_zx(z, x):
    z = z.normalized()
    x = (x - z * x.dot(z)).normalized()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed().to_4x4()


# Canonical bat socket relative transforms computed at authentic stance.
# Used to orient the primary batting hand so the attached bat mesh aligns exactly
# with the authored bat position and blade plane across all frames.
BAT_OFFSET_L = Matrix([
    (-0.222148, -0.578748, -0.784666, -3.531000),
    (-0.771659, -0.387561, 0.504320, 2.269439),
    (-0.595980, 0.717529, -0.360501, -1.622257),
    (0.000000, 0.000000, 0.000000, 1.000000),
])

BAT_OFFSET_R = Matrix([
    (-0.222148, 0.578748, 0.784666, 3.531000),
    (0.771659, -0.387561, 0.504320, 2.269439),
    (0.595980, 0.717529, -0.360501, -1.622257),
    (0.000000, 0.000000, 0.000000, 1.000000),
])


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
        # Every pb.matrix assignment leaks a little float scale. Never resetting it let limb
        # lengths drift across a long authoring session, so later clips got longer or shorter arms.
        pb.scale = Vector((1, 1, 1))
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
        elif key in ('shaft', 'face'):
            out[key] = (value[0], -value[1], value[2])
        elif isinstance(value, tuple) and value and value[0] in ('R', 'OFF'):
            out[key] = value          # already expressed relative to the body
        else:
            out[key] = (value[0], -value[1], value[2])
    return out


def _reach_assist(rig, solved, arm, limit=42.0):
    """The bat is placed where the ball is, so both hands are fixed. If a shoulder cannot reach its
    hand, turn and bend the upper torso towards the bat the way a batter's chest follows the hands,
    instead of tearing the bottom hand off the handle. The head keeps its world orientation, so the
    eyes stay on the ball while the chest works underneath them.

    The search minimises reach deficit plus a small cost on the turn itself and is warm-started from
    the previous frame, so the assist fades in and out smoothly instead of snapping on."""
    targets = [solved.get('hand_l'), solved.get('hand_r')]
    if None in targets:
        return
    pivot = rig.pose.bones['spine_02']
    h = pivot.head.copy()
    shoulders = [rig.pose.bones['upperarm_l'].head - h, rig.pose.bones['upperarm_r'].head - h]

    def cost(q):
        over = sum(max(0.0, ((h + q @ s) - t).length - arm * 0.955) ** 2 for s, t in zip(shoulders, targets))
        return over + 0.004 * math.degrees(q.angle) ** 2

    warm = getattr(_reach_assist, 'warm', None)
    total = Quaternion()
    best = cost(total)
    if warm is not None and cost(warm) < best:
        total, best = warm.copy(), cost(warm)
    step = 2.0 * D
    axes = [Vector(a) for a in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
    for _ in range(120):
        pick = None
        for axis in axes:
            for sign in (1.0, -1.0):
                q = Quaternion(axis, sign * step) @ total
                if math.degrees(q.angle) > limit:
                    continue
                c = cost(q)
                if c < best - 1e-5:
                    pick, best = q, c
        if pick is None:
            if step < 0.25 * D:
                break
            step *= 0.5
            continue
        total = pick
    _reach_assist.warm = total.copy()
    if math.degrees(total.angle) < 0.05:
        return
    head = rig.pose.bones['head']
    head_rot = head.matrix.to_quaternion()
    # Spread the turn through the lumbar and thoracic joints so it bends the ribcage, not one hinge.
    names = ('spine_02', 'spine_03', 'spine_04', 'spine_05')
    part = Quaternion().slerp(total, 1.0 / len(names))
    for name in names:
        pb = rig.pose.bones[name]
        update()
        c = pb.head.copy()
        pb.matrix = Matrix.Translation(c) @ part.to_matrix().to_4x4() @ Matrix.Translation(-c) @ pb.matrix
    update()
    head.matrix = Matrix.LocRotScale(head.head.copy(), head_rot, Vector((1, 1, 1)))
    update()


def apply(rig, spec):
    """Resolve one pose specification onto the rig.

    Keys: pelvis (rx,ry,rz,dx,dy,dz) | spine | chest | neck | head | clav_l/r |
    hand_l/hand_r (IK target) | elbow_l/elbow_r (pole) | wrist_l/wrist_r (rx,ry,rz after IK) |
    foot_l/foot_r (IK target) | knee_l/knee_r (pole) | ankle_l/ankle_r | grip_l/grip_r |
    shaft (bat handle vector) | face (bat blade hitting face normal)
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

    # If a batting shaft is specified, calculate the dependent bottom-hand target along the handle
    if 'shaft' in spec:
        sh = Vector(spec['shaft']).normalized()
        grip_dist = 8.5
        if 'hand_l' in spec and 'hand_r' not in spec:
            hl = _resolve(rig, spec, 'hand_l', 'upperarm_l', arm, solved)
            if hl is not None:
                solved['hand_r'] = hl - sh * grip_dist
        elif 'hand_r' in spec and 'hand_l' not in spec:
            hr = _resolve(rig, spec, 'hand_r', 'upperarm_r', arm, solved)
            if hr is not None:
                solved['hand_l'] = hr - sh * grip_dist

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

    if 'shaft' in spec:
        _reach_assist(rig, solved, arm)

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

    # Orient the bat-holding hand so the bat precisely follows the authored shaft and face vectors
    if 'shaft' in spec and 'face' in spec:
        sh = Vector(spec['shaft']).normalized()
        fc = Vector(spec['face']).normalized()
        bat_side = 'r' if ('hand_r' in spec and 'hand_l' not in spec) else 'l'
        top_pos = solved.get(f'hand_{bat_side}')
        if top_pos is not None:
            bat_m = Matrix.Translation(Vector(top_pos) + sh * 4.5) @ make_from_zx(sh, fc)
            hand_bone = rig.pose.bones[f'hand_{bat_side}']
            off = BAT_OFFSET_R if bat_side == 'r' else BAT_OFFSET_L
            # Rotation only: the wrist stays where the arm put it, so a short reach can move the
            # bat a little but can never pull the hand off the end of the forearm.
            hand_bone.matrix = Matrix.LocRotScale(hand_bone.head.copy(),
                                                  (bat_m @ off.inverted()).to_quaternion(), Vector((1, 1, 1)))
            update()

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
        elif key in ('shaft', 'face'):
            out[k] = (-value[0], value[1], value[2])
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


def _monotone(xs, ys, x):
    """Fritsch-Carlson monotone cubic: C1-smooth through every key, and never overshoots a key.
    Overshoot is what pushes a planted shoe through the floor or a hand past the handle."""
    n = len(xs)
    if n == 1 or x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    s = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [s[0]] + [0.0 if s[i - 1] * s[i] <= 0 else (s[i - 1] + s[i]) * 0.5 for i in range(1, n - 1)] + [s[-1]]
    m[0] = m[-1] = 0.0  # every stroke starts and ends at rest
    for i in range(n - 1):
        if s[i] == 0.0:
            m[i] = m[i + 1] = 0.0
            continue
        a, b = m[i] / s[i], m[i + 1] / s[i]
        if a * a + b * b > 9.0:
            t = 3.0 / math.sqrt(a * a + b * b)
            m[i], m[i + 1] = t * a * s[i], t * b * s[i]
    i = max(k for k in range(n - 1) if xs[k] <= x)
    t = (x - xs[i]) / h[i]
    t2, t3 = t * t, t * t * t
    return ((2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h[i] * m[i]
            + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * h[i] * m[i + 1])


def spec_at(keys, frame):
    """Technique-space interpolation: the stance, IK targets, bat shaft and face are blended, then the
    pose is solved. Blending solved bone rotations instead lets the bottom hand drift off the handle
    and flips the bat between keys, which is exactly what a camera catches."""
    frames = [float(f) for f, _ in keys]
    out = {}
    names = []
    for _, spec in keys:
        names.extend(k for k in spec if k not in names)
    for key in names:
        present = [(f, s[key]) for f, s in zip(frames, (s for _, s in keys)) if key in s]
        values = [v for _, v in present]
        xs = [f for f, _ in present]
        v0 = values[0]
        numeric = isinstance(v0, tuple) and all(isinstance(e, (int, float)) for e in v0)
        tagged = (isinstance(v0, tuple) and v0 and isinstance(v0[0], str)
                  and all(isinstance(v, tuple) and v[:1] == v0[:1] and len(v) == len(v0)
                          and (v0[0] != 'OFF' or v[1] == v0[1]) for v in values))
        if numeric and all(isinstance(v, tuple) and len(v) == len(v0) for v in values):
            out[key] = tuple(_monotone(xs, [v[i] for v in values], frame) for i in range(len(v0)))
            if key in ('shaft', 'face'):
                vec = Vector(out[key]).normalized()
                out[key] = (vec.x, vec.y, vec.z)
        elif tagged:
            head = 2 if v0[0] == 'OFF' else 1
            out[key] = v0[:head] + tuple(_monotone(xs, [v[i] for v in values], frame)
                                         for i in range(head, len(v0)))
        else:  # grips and mixed forms step on the key at or before this frame
            out[key] = next((v for f, v in reversed(present) if f <= frame), v0)
    return out


def shift_to_actor(spec, travel):
    """World-authored pose -> actor space. The match moves the athlete's root itself (run-up, bowling
    follow-through), so a clip authored with feet planted in the world and the gameplay root travel
    subtracted plays back with no foot skating once the root is added back in game."""
    tx, ty = travel
    out = {}
    for key, value in spec.items():
        if key == 'pelvis' and len(value) > 3:
            out[key] = (value[0], value[1], value[2], value[3] - tx, value[4] - ty, value[5])
        elif (key.startswith(('foot_', 'knee_', 'elbow_', 'hand_')) and isinstance(value, tuple)
              and len(value) == 3 and all(isinstance(c, (int, float)) for c in value)):
            out[key] = (value[0] - tx, value[1] - ty, value[2])
        else:
            out[key] = value
    return out


def absolute_limbs(rig, keys):
    """spec_at can only interpolate a limb target that keeps one form across keys; a limb that switches
    between a reach fraction R() and an absolute position steps on the key, which teleports the hand.
    Resolve those R() targets to the position the solver puts them at on their own key."""
    limbs = ('hand_l', 'hand_r', 'foot_l', 'foot_r')
    forms = {k: {s[k][0] if isinstance(s[k][0], str) else 'P' for _, s in keys if k in s} for k in limbs}
    mixed = [k for k in limbs if len(forms[k]) > 1 and 'OFF' not in forms[k]]
    if not mixed:
        return keys
    out = []
    for frame, spec in keys:
        spec = dict(spec)
        if any(isinstance(spec.get(k), tuple) and spec[k][:1] == ('R',) for k in mixed):
            apply(rig, spec)
            for k in mixed:
                if spec.get(k, ())[:1] == ('R',) and k in apply.last_targets:
                    t = apply.last_targets[k]
                    spec[k] = (t.x, -t.y, t.z)        # rig frame -> authoring frame
        out.append((frame, spec))
    return out


def bake(rig, name, keys, loop=False, dense=False, travel=None, resolve_mixed=False):
    """Write one action. `keys` is [(frame, spec), ...]; the pose at each frame is resolved and
    stamped onto every controlled bone so interpolation never drifts through an unkeyed joint.
    `dense` resolves the interpolated technique on every frame (batting: two hands, one bat)."""
    if rig.animation_data is None:
        rig.animation_data_create()
    bones = key_bones(rig)
    if loop and keys[0][1] != keys[-1][1]:
        keys = list(keys) + [(keys[-1][0] + (keys[1][0] - keys[0][0]), keys[0][1])]
    if dense:
        if resolve_mixed:
            rig.animation_data.action = None
            keys = absolute_limbs(rig, keys)
        keys = [(f, spec_at(keys, f)) for f in range(int(keys[0][0]), int(keys[-1][0]) + 1)]
        if travel is not None:
            keys = [(f, shift_to_actor(spec, travel(f))) for f, spec in keys]
    # Solve every pose with no action bound. With an action attached, a depsgraph update can
    # re-evaluate the previous clip over the pose being solved and leak it into this one.
    rig.animation_data.action = None
    _reach_assist.warm = None
    solved, previous = [], {}
    for frame, spec in keys:
        apply(rig, spec)
        pose = {}
        for n in bones:
            q = rig.pose.bones[n].rotation_quaternion.copy()
            # q and -q are the same rotation; keep neighbours in one hemisphere so no sampler
            # ever spins a joint the long way round between two frames.
            if n in previous and previous[n].dot(q) < 0:
                q.negate()
            previous[n] = q
            pose[n] = (q, rig.pose.bones[n].location.copy())
        solved.append((frame, pose))
    # Any bone the solver translated must be keyed, or playback keeps a stale offset from the last
    # pose that happened to be solved.
    moved = {'pelvis'} | {n for _, pose in solved for n in bones if pose[n][1].length > 1e-4}
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    if hasattr(action, 'slots'):  # Blender 4.4+ slotted actions
        slot = action.slots.new(id_type='OBJECT', name=rig.name)
        rig.animation_data.action_slot = slot
    for frame, pose in solved:
        for n in bones:
            pb = rig.pose.bones[n]
            pb.rotation_quaternion, pb.location = pose[n]
            pb.keyframe_insert('rotation_quaternion', frame=frame, group=n)
            if n in moved:
                pb.keyframe_insert('location', frame=frame, group=n)
    for fc in _fcurves(action):
        for kp in fc.keyframe_points:
            kp.interpolation = 'LINEAR' if dense else 'BEZIER'
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
