"""CRICKET 26 — authored cricket animation, built on the production 67-bone rig.

Why this exists
---------------
Every cricket action in the game is posed procedurally in C++ (`AC26Athlete::Animate`).
That produces correct *positions* but no authored *motion*: there is no animation asset
for a batting stroke or a bowling action anywhere in the project. This script authors
real keyframed animation on the exact rig the game already skins to
(`C26_KitBase_v002.blend` -> `Armature`, 67 bones, `mixamorig:*` names), so the clips
play on the existing skeleton with no retargeting.

Coordinate convention of this rig (measured, not assumed)
--------------------------------------------------------
    -Y = the character's FORWARD (toes point -Y in armature space)
    +X = the character's LEFT   (LeftArm tail is at +X)
    +Z = up
Rest pose is a T-pose: arms straight out along +/-X, legs straight down.
This was measured against the shipped rig (C26_KitBase_v002 -> the exported
FBX): the toe vector in armature space is (2.7, -21.4, 24.3), i.e. toes point
-Y. The first version of this file assumed +Y was forward, which placed every
IK target on the character's BACK (the drive went through the body). The keys
below are still written in the original authoring frame (forward = +Y); the
function repair_facing() transplants them onto the rig's true orientation at
solve time, so the numbers stay readable as authored.

Why the pose is specified with IK targets, not joint angles
-----------------------------------------------------------
The first version of this file set bone angles directly. Measured, it put the two hands
0.83 m apart -- the defining feature of a batting animation is that both hands are ON the
bat handle, and no amount of angle-guessing reaches that. So limbs are now driven by
where the hands and feet must BE, and a two-bone solver works out the joint angles. The
spine chain is still authored as rotations, because that is genuinely how a coach talks
about a batting stance.

How the FK solve works
----------------------
A pose bone's armature-space matrix is
    M_pose = M_parent_pose @ M_parent_rest^-1 @ M_bone_rest @ M_basis
so the basis we must write is
    M_basis = (M_parent_pose @ M_parent_rest^-1 @ M_bone_rest)^-1 @ M_pose
Walking the hierarchy in parent-first order means M_parent_pose is always already known,
so no depsgraph round-trips are needed and the result is deterministic.

Run:
  Blender --background ArtSource/Blender/Characters/C26_KitBase_v002.blend \
      --python ArtSource/Blender/Animation/c26_anim_author.py -- --out <dir>
"""
import bpy
import math
import os
import sys
from mathutils import Matrix, Quaternion, Vector

RAD = math.radians
AXES = {'x': Vector((1, 0, 0)), 'y': Vector((0, 1, 0)), 'z': Vector((0, 0, 1))}

HIPS = 'mixamorig:Hips'
BONE_Y = Vector((0.0, 1.0, 0.0))  # Mixamo bones run along their local +Y

# Limb chains driven by IK: (upper, lower, end, pole_direction)
ARM_L = ('mixamorig:LeftArm', 'mixamorig:LeftForeArm', 'mixamorig:LeftHand')
ARM_R = ('mixamorig:RightArm', 'mixamorig:RightForeArm', 'mixamorig:RightHand')
LEG_L = ('mixamorig:LeftUpLeg', 'mixamorig:LeftLeg', 'mixamorig:LeftFoot')
LEG_R = ('mixamorig:RightUpLeg', 'mixamorig:RightLeg', 'mixamorig:RightFoot')

ANIMATED = [
    HIPS, 'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2',
    'mixamorig:Neck', 'mixamorig:Head',
    'mixamorig:LeftShoulder', 'mixamorig:LeftArm', 'mixamorig:LeftForeArm', 'mixamorig:LeftHand',
    'mixamorig:RightShoulder', 'mixamorig:RightArm', 'mixamorig:RightForeArm', 'mixamorig:RightHand',
    'mixamorig:LeftUpLeg', 'mixamorig:LeftLeg', 'mixamorig:LeftFoot', 'mixamorig:LeftToeBase',
    'mixamorig:RightUpLeg', 'mixamorig:RightLeg', 'mixamorig:RightFoot', 'mixamorig:RightToeBase',
]


def R(*ops):
    """Compose armature-space rotations. Earlier ops are applied first."""
    out = Quaternion()
    for axis, deg in ops:
        out = Quaternion(AXES[axis], RAD(deg)) @ out
    return out


# The rig's true forward in armature space (toes point -Y; see the header).
RIG_FORWARD = Vector((0.0, -1.0, 0.0))


def repair_facing(spec, hips_loc, ik):
    """Transplant authored keys from the assumed frame (+Y forward) onto the
    rig's real frame (-Y forward). Called at solve time, so the keys stay
    readable as authored while the solve lands on the character's FRONT.

    What it does, and why each part is what it is:
      * spine spec quaternions conjugate by rotZ(180): the imagined forward is
        the actual backward, so x-rotations (leans) and y-rotations negate,
        while z-rotations (turn/chest twists) keep their body-relative meaning.
      * IK targets, shoulder-relative offsets and hips offsets rotate by
        (x, y, z) -> (-x, -y, z): the swing arc, the stride and the weight
        transfer move to the front; left/right sides swap exactly as a proper
        180-degree rotation swaps them, so a right-hander stays a right-hander.
      * IK poles keep x and z and negate y: the pole x was iterated against
        real renders of this rig (elbow OUTWARD), while its y was authored in
        the imagined frame. The chain defaults baked into Rig.apply are
        injected here as explicit, repaired poles so they can never fight this.
    """
    R180 = Quaternion(Vector((0.0, 0.0, 1.0)), 180.0)
    spec = {name: (R180 @ q @ R180.conjugated()) for name, q in spec.items()}
    if hips_loc is not None:
        hips_loc = Vector((-hips_loc[0], -hips_loc[1], hips_loc[2]))
    out = dict(ik) if ik else {}
    # apply()'s baked-in chain poles, injected explicitly so they go through
    # the same pole repair as authored poles (raw values; the loop below
    # repairs them). Arms: elbows back and slightly down. Legs: knees forward.
    defaults = {
        'left_hand_pole': Vector((0.0, -1.0, -0.35)),
        'right_hand_pole': Vector((0.0, -1.0, -0.35)),
        'left_foot_pole': Vector((0.0, 1.0, 0.0)),
        'right_foot_pole': Vector((0.0, 1.0, 0.0)),
    }
    for k, v in defaults.items():
        out.setdefault(k, v)
    for k, v in list(out.items()):
        if k.endswith('_pole'):
            out[k] = Vector((v[0], -v[1], v[2]))
        else:
            out[k] = Vector((-v[0], -v[1], v[2]))
    return spec, hips_loc, out


class Rig:
    def __init__(self, arm):
        self.arm = arm
        self.rest = {b.name: b.matrix_local.copy() for b in arm.data.bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}
        self.order = []
        self._walk()
        for name in self.rest:
            if name not in self.order:
                self.order.append(name)

    def _walk(self):
        def rec(b):
            self.order.append(b.name)
            for c in b.children:
                rec(c)
        for b in self.arm.data.bones:
            if b.parent is None:
                rec(b)

    def rest_dir(self, name):
        """The bone's rest direction in armature space."""
        return (self.rest[name].to_quaternion() @ BONE_Y).normalized()

    def rest_len(self, name):
        b = self.arm.data.bones[name]
        return (b.tail_local - b.head_local).length

    @staticmethod
    def two_bone(S, T, L1, L2, pole):
        """Elbow/knee position for a two-bone chain from S to T. Returns (E, reachable)."""
        d = T - S
        dist = d.length
        if dist < 1e-6:
            return S + Vector((0, 0, -L1)), False
        maxr = (L1 + L2) * 0.999
        minr = abs(L1 - L2) * 1.001
        clamped = min(max(dist, minr), maxr)
        dirv = d / dist
        a = (L1 * L1 - L2 * L2 + clamped * clamped) / (2.0 * clamped)
        h2 = L1 * L1 - a * a
        h = math.sqrt(h2) if h2 > 0 else 0.0
        n = pole - dirv * pole.dot(dirv)
        if n.length < 1e-6:
            n = Vector((0, 0, 1)) - dirv * dirv.z
        n.normalize()
        return S + dirv * a + n * h, dist <= maxr

    def apply(self, frame_index, spec, hips_loc, ik):
        """spec: {bone: delta Quaternion}; ik: {chain_key: armature-space target Vector}

        The IK solve for a chain writes BOTH the upper and the lower bone, but the loop
        then walks on to the lower bone and would overwrite it with its un-IK'd base
        matrix. Results are parked in ik_done and consumed when the loop reaches them.
        """
        target = {}
        ik_done = {}
        for name in self.order:
            rest_m = self.rest[name]
            p = self.parent[name]

            if name in ik_done:
                base, m_pose = ik_done[name]
                target[name] = m_pose
                self._write(name, base, m_pose)
                continue

            base = rest_m.copy() if p is None else target[p] @ self.rest[p].inverted() @ rest_m

            delta = spec.get(name)
            if delta is None:
                m_pose = base.copy()
            else:
                m_pose = (delta @ base.to_quaternion()).to_matrix().to_4x4()
                m_pose.translation = base.to_translation()

            if name == HIPS and hips_loc is not None:
                m_pose.translation = m_pose.translation + Vector(hips_loc)

            # --- IK override for limb chains -------------------------------------
            chain = None
            if name == ARM_L[0]:
                chain = ('left_hand', ARM_L, Vector((0.0, -1.0, -0.35)))
            elif name == ARM_R[0]:
                chain = ('right_hand', ARM_R, Vector((0.0, -1.0, -0.35)))
            elif name == LEG_L[0]:
                chain = ('left_foot', LEG_L, Vector((0.0, 1.0, 0.0)))
            elif name == LEG_R[0]:
                chain = ('right_foot', LEG_R, Vector((0.0, 1.0, 0.0)))

            if chain and (chain[0] in ik or chain[0] + '_rel' in ik):
                key, (upper, lower, end), pole = chain
                # A chain may carry its own pole. The elbow of a bowling arm has to swing from
                # behind, out to the side and then through as the arm comes over the top; a fixed
                # backward pole pins the elbow behind the body for the whole circle, which is what
                # turned an overhead action into a round-arm one.
                pole = Vector(ik.get(key + '_pole', pole))
                # A target may be given as an offset from the joint it hangs off instead of as an
                # absolute point. This matters for the bowling arm: the torso travels about half a
                # metre through the action, so an absolute target that reads "straight up" at the
                # start reads "up and behind" by release, and the arm ends up swinging backwards
                # over the head. Shoulder-relative keeps the arm's relationship to its own body.
                if key + '_rel' in ik:
                    T = base.to_translation() + Vector(ik[key + '_rel'])
                else:
                    T = Vector(ik[key])
                S = base.to_translation()
                L1 = self.rest_len(upper)
                L2 = self.rest_len(lower)
                E, _ok = self.two_bone(S, T, L1, L2, pole)

                # Aim the bone by rotating from where it CURRENTLY points (its inherited
                # direction, which already carries the whole spine chain's rotation) onto the
                # direction we want. Rotating from the rest direction instead silently bakes
                # in the spine twist and the limb lands somewhere else entirely.
                up_dir = (E - S)
                base_up_dir = (base.to_quaternion() @ BONE_Y).normalized()
                up_dir = up_dir.normalized() if up_dir.length > 1e-6 else base_up_dir
                q_up = base_up_dir.rotation_difference(up_dir)
                m_up = (q_up @ base.to_quaternion()).to_matrix().to_4x4()
                m_up.translation = S
                target[upper] = m_up
                self._write(upper, base, m_up)

                # Lower bone: aim from E to T, inheriting the upper bone's frame.
                base_low = m_up @ self.rest[upper].inverted() @ self.rest[lower]
                lo_dir = (T - E)
                base_lo_dir = (base_low.to_quaternion() @ BONE_Y).normalized()
                lo_dir = lo_dir.normalized() if lo_dir.length > 1e-6 else base_lo_dir
                q_lo = base_lo_dir.rotation_difference(lo_dir)
                m_lo = (q_lo @ base_low.to_quaternion()).to_matrix().to_4x4()
                m_lo.translation = base_low.to_translation()
                ik_done[lower] = (base_low, m_lo)
                continue

            target[name] = m_pose
            self._write(name, base, m_pose)

    def _write(self, name, base, m_pose):
        pb = self.arm.pose.bones[name]
        pb.rotation_mode = 'QUATERNION'
        if name in ANIMATED:
            basis = base.inverted() @ m_pose
            pb.rotation_quaternion = basis.to_quaternion()
            pb.location = basis.to_translation() if name == HIPS else Vector((0, 0, 0))
            pb.keyframe_insert('rotation_quaternion', frame=CURRENT_FRAME)
            if name == HIPS:
                pb.keyframe_insert('location', frame=CURRENT_FRAME)
        else:
            pb.rotation_quaternion = Quaternion()
            pb.location = Vector((0, 0, 0))


CURRENT_FRAME = 1


def action_fcurves(action):
    """Blender 4.4+ replaced Action.fcurves with layers -> strips -> channelbags."""
    try:
        return list(action.fcurves)
    except AttributeError:
        pass
    out = []
    for layer in action.layers:
        for strip in layer.strips:
            for cb in getattr(strip, 'channelbags', []):
                out.extend(cb.fcurves)
    return out


def build_action(rig, name, keys):
    """keys: list of (frame, spec, hips_loc, ik)."""
    global CURRENT_FRAME
    arm = rig.arm
    arm.animation_data_create()
    action = bpy.data.actions.new(name)
    arm.animation_data.action = action

    for f, spec, hips, ik in keys:
        spec, hips, ik = repair_facing(spec, hips, ik)
        CURRENT_FRAME = f
        rig.apply(f, spec, hips, ik)

    curves = action_fcurves(action)
    for fc in curves:
        for kp in fc.keyframe_points:
            kp.interpolation = 'BEZIER'
            kp.handle_left_type = 'AUTO_CLAMPED'
            kp.handle_right_type = 'AUTO_CLAMPED'
        fc.update()
    arm.animation_data.action = None
    return action, len(curves)


def spine(turn, lean, chest, neck, head):
    return {
        HIPS: R(('z', turn), ('x', lean)),
        'mixamorig:Spine': R(('z', chest * 0.40), ('x', lean * 0.30)),
        'mixamorig:Spine1': R(('z', chest * 0.35), ('x', lean * 0.25)),
        'mixamorig:Spine2': R(('z', chest * 0.25), ('x', lean * 0.20)),
        'mixamorig:Neck': R(('z', neck), ('x', -lean * 0.35)),
        'mixamorig:Head': R(('z', head), ('x', -lean * 0.50)),
        'mixamorig:LeftShoulder': R(('z', -4), ('y', 8)),
        'mixamorig:RightShoulder': R(('z', 4), ('y', -8)),
    }


def V(x, y, z):
    return Vector((x, y, z))


# ---------------------------------------------------------------------------------
# BATTING — a right-handed straight drive.
# Hands stay on the bat handle for the whole clip: the grip target moves, the two
# hands are always 4 cm apart on it, and the arms solve to reach it.
# ---------------------------------------------------------------------------------
def batting_keys():
    """Right-handed front-foot drive.

    LEAN SIGN: spine() applies R(('x', lean)) to the Hips in armature space. Rotating this
    rig's forward (+Y) about +X tips it toward +Z, so a POSITIVE lean tips the batsman
    BACKWARD. Every key in the first version used a positive lean, which is why the striker
    spent the whole stroke leaning away from the ball. Forward lean is negative here, which
    is also the sign the bowling action already used to fall over the brace leg.

    The other half of a drive is where the hands are at contact: over or just past the
    front foot, not behind it. The front foot is planted at y=0.46, so the grip goes to
    y=0.50 rather than the 0.30 the first version used, which left the blade behind the
    batsman's own front pad at the moment of contact.
    """
    keys = []

    def K(frame, turn, lean, chest, neck, head, grip, lfoot, rfoot, hips_z, hips_y=0.0,
          hand_gap=0.04):
        spec = spine(turn, lean, chest, neck, head)
        g = Vector(grip)
        # The grip runs across the handle: left hand above the right on a right-hander.
        lh = g + V(0, 0, hand_gap)
        rh = g - V(0, 0, hand_gap)
        ik = {
            'left_hand': lh,
            'right_hand': rh,
            'left_foot': Vector(lfoot),
            'right_foot': Vector(rfoot),
        }
        return (frame, spec, V(0.0, hips_y, hips_z), ik)

    # Stance: side-on, knees loaded, hands on the handle in front of the back hip.
    keys.append(K(1, 52, -6, -14, -10, -26, (0.08, 0.12, 0.88), (0.10, 0.26, 0.09),
                  (-0.14, 0.06, 0.09), -0.115))
    # Trigger: weight rocks back, hands lift to the top of the backlift.
    keys.append(K(7, 58, -3, -18, -10, -28, (0.14, -0.04, 1.08), (0.10, 0.24, 0.09),
                  (-0.15, 0.02, 0.09), -0.128, -0.03))
    # Top of backlift: hands high behind the back shoulder, chest coiled.
    keys.append(K(13, 64, 1, -22, -8, -26, (0.18, -0.20, 1.30), (0.11, 0.20, 0.09),
                  (-0.15, -0.02, 0.09), -0.132, -0.05))
    # Downswing: hands come down the line of the ball, front foot reaching out.
    keys.append(K(18, 44, -12, -4, -6, -18, (0.12, 0.14, 1.00), (0.13, 0.38, 0.10),
                  (-0.14, 0.06, 0.09), -0.125, 0.02))
    # CONTACT: hands out past the front foot, chest over the ball, leaning in over it.
    keys.append(K(23, 26, -20, 10, -2, -8, (0.08, 0.50, 0.94), (0.15, 0.46, 0.10),
                  (-0.12, 0.12, 0.09), -0.135, 0.10))
    # Follow-through: hands swing up past the front shoulder, weight fully forward.
    keys.append(K(29, 6, -17, 22, 2, 0, (0.00, 0.40, 1.30), (0.15, 0.48, 0.10),
                  (-0.11, 0.16, 0.09), -0.118, 0.13))
    # Recover.
    keys.append(K(36, 20, -9, 6, -4, -10, (0.06, 0.26, 1.00), (0.14, 0.40, 0.10),
                  (-0.12, 0.12, 0.09), -0.120, 0.08))
    return keys


# ---------------------------------------------------------------------------------
# BOWLING — right-arm fast bowler. The bowling hand is the IK target, so the arm
# genuinely rotates through the vertical and the release key is the top of the circle.
# ---------------------------------------------------------------------------------
def bowling_keys():
    """Right-arm fast bowler, OVERHEAD.

    The bowling arm is the IK target and it traces a vertical circle: chest -> down and
    back -> up behind the head -> over the top -> down past the opposite hip. Release is
    the frame where the hand is at its highest and furthest from the shoulder, which is
    what makes it an overhead action rather than a round-arm one.

    Measured against the rig, the right shoulder sits at about z=1.45 m and the whole arm
    reaches about 0.62 m, so a fully extended release puts the hand near z=2.06 m -- above
    the top of the head. The previous version released at z=1.74 m, only 0.36 m from the
    shoulder (57% extension), which is why it read as underarm.
    """
    keys = []

    def K(frame, turn, lean, chest, neck, head, rh_rel, lh_rel, lfoot, rfoot, hips_z,
          hips_y=0.0, rpole=(-1.0, -0.35, 0.0)):
        """Hands are offsets from their own shoulder; feet stay absolute, because the ground
        is what they are planted on and it does not move with the bowler's torso."""
        spec = spine(turn, lean, chest, neck, head)
        ik = {
            'right_hand_rel': Vector(rh_rel),
            'left_hand_rel': Vector(lh_rel),
            'left_foot': Vector(lfoot),
            'right_foot': Vector(rfoot),
            # The bowling elbow rides OUTWARD from the body all the way through the circle.
            'right_hand_pole': Vector(rpole),
        }
        return (frame, spec, V(0.0, hips_y, hips_z), ik)

    # Measured on this rig: the right shoulder sits at z=1.341 and the whole arm only reaches
    # 0.4745 m. So the highest a wrist can possibly get is about z=1.82, and an overhead release
    # has to be authored as "fully extended, almost straight up" -- there is no more arm to give.
    # At the mark: tall, square, ball in both hands at chest height. The right hand crosses to
    # the left (x -0.36 ~= shoulder width) so the two hands actually meet on the ball;
    # relative-to-own-shoulder targets at the same offset would hold them 34 cm apart.
    keys.append(K(1, 8, 4, 0, 0, 0, (-0.36, 0.14, -0.06), (-0.02, 0.14, -0.06),
                  (0.10, 0.10, 0.10), (-0.10, 0.08, 0.10), -0.06, 0.0,
                  (-1.0, -0.30, -0.20)))
    # Gather: weight sinks, hands drop together low in front.
    keys.append(K(8, 12, 12, -4, 2, -4, (-0.36, 0.16, -0.34), (-0.02, 0.16, -0.34),
                  (0.10, 0.24, 0.10), (-0.10, 0.30, 0.10), -0.19, 0.0,
                  (-1.0, -0.30, -0.30)))
    # Bound: airborne, the bowling arm swings back and down behind the hip.
    keys.append(K(14, 14, 8, -2, 2, -4, (-0.08, -0.18, -0.26), (0.06, 0.20, 0.10),
                  (0.11, 0.30, 0.24), (-0.09, 0.40, 0.22), -0.06, 0.10,
                  (-1.0, -0.30, -0.20)))
    # Back foot lands at the crease; the bowling arm is already up behind the head.
    keys.append(K(20, 16, 4, 0, 4, -2, (-0.12, -0.24, 0.30), (0.08, 0.28, 0.30),
                  (0.12, 0.52, 0.14), (-0.06, 0.44, 0.12), -0.10, 0.24,
                  (-1.0, -0.20, 0.25)))
    # Front foot braces. The arm is at the top of the circle, cocked behind the head.
    keys.append(K(26, 16, -4, 2, 6, 2, (-0.10, -0.14, 0.44), (0.06, 0.30, 0.34),
                  (0.11, 0.64, 0.12), (-0.05, 0.42, 0.12), -0.07, 0.30,
                  (-1.0, -0.05, 0.35)))
    # RELEASE: arm through the vertical -- fully extended and pointing straight up, just
    # forward of the shoulder. Hips are at their highest here; a bowler is tall at release.
    keys.append(K(31, 14, -16, 4, 6, 4, (-0.03, 0.08, 0.465), (0.02, 0.10, -0.10),
                  (0.10, 0.66, 0.12), (-0.05, 0.40, 0.12), -0.03, 0.33,
                  (-1.0, 0.15, 0.20)))
    # Follow-through: the bowling arm comes down past the opposite hip.
    keys.append(K(38, 12, -18, 2, 6, 6, (0.14, 0.22, -0.36), (-0.04, -0.10, -0.30),
                  (0.09, 0.64, 0.12), (-0.07, 0.38, 0.12), -0.12, 0.34,
                  (-1.0, 0.45, -0.25)))
    # Recover: straighten up, arms settle back toward the chest.
    keys.append(K(46, 8, 6, 0, 2, 2, (-0.02, 0.20, -0.20), (-0.02, 0.16, -0.10),
                  (0.09, 0.44, 0.11), (-0.09, 0.34, 0.11), -0.08, 0.20,
                  (-1.0, -0.20, -0.10)))
    return keys


# ---------------------------------------------------------------------------------
# THE SHOT LIBRARY. Every batting clip shares the drive's frame layout (36 frames,
# contact at 23) and every bowling clip shares the pace layout (46 frames, release
# at 31), because AC26Athlete pins the defining frame to the match's own timing via
# one pair of constants (C26BattingContactFrame / C26BowlingReleaseFrame).
#
# Selection, in AC26Athlete::SelectBattingClip / SelectBowlingClip:
#   Defence (Defending) | Pull (leg side, weight back) | Sweep (leg side, weight
#   forward/low) | Cut (off side, weight back) | Drive (everything else, incl. the
#   cover/on drives, which are the same swing aimed differently).
#   Bowling: OffBreak family -> off-spin action, LegBreak family -> leg-spin action,
#   everything else -> pace.
#
# All keys are in the ORIGINAL AUTHORING FRAME (forward +Y, right +X, left -X);
# repair_facing() transplants them onto the rig's true -Y facing at solve time.
# ---------------------------------------------------------------------------------

def _stance_key(K, turn=52, lean=-6, chest=-14, neck=-10, head=-26,
                grip=(0.08, 0.12, 0.88), lf=(0.10, 0.26, 0.09), rf=(-0.14, 0.06, 0.09),
                hips_z=-0.115):
    """The shared starting stance; every shot starts from the same shape so the
    entry blend from the ready pose is identical across the library."""
    return K(1, turn, lean, chest, neck, head, grip, lf, rf, hips_z)


def batting_pull_keys():
    """Right-handed PULL: short ball, weight rocked onto the back foot, horizontal
    swing through chest height, chest opening fully toward midwicket by contact."""
    keys = []
    K = _batting_key_builder()
    keys.append(_stance_key(K))
    # Rock back: weight onto the back foot, hands lift.
    keys.append(K(7, 58, -3, -18, -10, -28, (0.14, -0.02, 1.10), (0.10, 0.22, 0.09),
                  (-0.16, 0.00, 0.09), -0.12, -0.04))
    # Backlift high and slightly leg-side, coiled.
    keys.append(K(13, 68, 2, -22, -8, -24, (0.02, -0.16, 1.34), (0.11, 0.16, 0.09),
                  (-0.16, -0.02, 0.09), -0.125, -0.05))
    # Swivel begins: hips rotating open, hands coming down.
    keys.append(K(18, 60, -10, -8, -6, -16, (-0.02, 0.06, 1.18), (0.12, 0.20, 0.09),
                  (-0.15, 0.04, 0.09), -0.12, -0.02))
    # CONTACT: chest height, in front of the body, just leg-side of the line.
    keys.append(K(23, 95, -8, 12, -2, -6, (-0.04, 0.34, 1.16), (0.12, 0.24, 0.09),
                  (-0.16, 0.02, 0.09), -0.10, -0.03))
    # Follow-through: the bat swings around to the leg side and up.
    keys.append(K(29, 100, -6, 16, 2, 0, (-0.22, 0.18, 1.30), (0.12, 0.26, 0.09),
                  (-0.15, 0.06, 0.09), -0.105, -0.02))
    # Recover.
    keys.append(K(36, 30, -8, 6, -4, -10, (-0.02, 0.20, 1.05), (0.12, 0.24, 0.09),
                  (-0.14, 0.06, 0.09), -0.115, 0.02))
    return keys


def batting_cut_keys():
    """Right-handed CUT: short and wide, weight back, chest opened toward point, a
    late slash played with the ball beside the body, bat finishing out toward the
    off side."""
    keys = []
    K = _batting_key_builder()
    keys.append(_stance_key(K))
    keys.append(K(7, 40, -2, -22, -12, -30, (0.14, 0.02, 1.12), (0.10, 0.20, 0.09),
                  (-0.14, 0.02, 0.09), -0.12, -0.03))
    # Backlift high over the off shoulder; the chest has opened toward point.
    keys.append(K(13, 30, 4, -28, -10, -26, (0.20, -0.08, 1.34), (0.11, 0.14, 0.09),
                  (-0.15, -0.02, 0.09), -0.13, -0.05))
    keys.append(K(18, 22, -8, -14, -8, -18, (0.18, 0.04, 1.22), (0.12, 0.16, 0.09),
                  (-0.14, 0.02, 0.09), -0.12, -0.04))
    # CONTACT: late -- beside the hips, out toward off, high. The bottom hand is
    # near full extension (0.44 m of a 0.474 m reach), which is what a cut is.
    keys.append(K(23, 15, -6, -10, -2, -8, (0.26, 0.02, 1.18), (0.13, 0.18, 0.09),
                  (-0.14, 0.00, 0.09), -0.12, -0.04))
    # The slash finishes out toward point.
    keys.append(K(29, 10, -8, -16, 2, -4, (0.26, -0.02, 1.00), (0.13, 0.20, 0.09),
                  (-0.13, 0.04, 0.09), -0.115, -0.02))
    keys.append(K(36, 28, -8, 6, -4, -10, (0.10, 0.14, 1.00), (0.12, 0.22, 0.09),
                  (-0.14, 0.05, 0.09), -0.118, 0.0))
    return keys


def batting_sweep_keys():
    """Right-handed SWEEP: full ball on the legs, deep crouch onto a wide front-leg
    stride, the whole torso folding over the front knee, hands sweeping through LOW
    toward the leg side. The back leg folds under (deep knee bend) -- a crouched
    sweep rather than a true knee-on-turf kneel, which position targets cannot
    author directly."""
    keys = []
    K = _batting_key_builder()
    keys.append(_stance_key(K))
    # Pre-move: a small sink, hands barely move.
    keys.append(K(7, 56, -4, -16, -10, -26, (0.10, 0.04, 0.96), (0.10, 0.24, 0.09),
                  (-0.14, 0.04, 0.09), -0.16, 0.0))
    # Backlift as the body starts to descend.
    keys.append(K(13, 62, 0, -18, -8, -24, (0.12, -0.10, 1.18), (0.10, 0.22, 0.09),
                  (-0.14, 0.02, 0.09), -0.24, -0.02))
    # The descent: hips drop hard, front foot strides out across to the leg-side
    # line where the ball will pitch.
    keys.append(K(18, 58, -2, -10, -6, -18, (0.06, 0.10, 0.95), (-0.06, 0.34, 0.09),
                  (-0.16, 0.00, 0.09), -0.32, 0.02))
    # CONTACT: low, in front of the pad, torso folded over the front knee.
    keys.append(K(23, 62, -24, 14, -2, -10, (-0.02, 0.34, 0.58), (-0.08, 0.36, 0.09),
                  (-0.16, 0.02, 0.09), -0.40, 0.04))
    # Hands swing through low toward square leg.
    keys.append(K(29, 70, -16, 20, 2, -4, (-0.24, 0.28, 0.85), (-0.08, 0.38, 0.09),
                  (-0.15, 0.06, 0.09), -0.34, 0.04))
    # Rise back out of the crouch.
    keys.append(K(36, 46, -8, 4, -4, -12, (0.04, 0.20, 0.95), (0.06, 0.28, 0.09),
                  (-0.14, 0.06, 0.09), -0.18, 0.02))
    return keys


def batting_defence_keys():
    """Forward DEFENCE: a small press forward, bat vertical under the eyes, soft
    hands, head over the ball. The whole clip is deliberately boring -- no arc, the
    follow-through holds the bat out in front rather than swinging it."""
    keys = []
    K = _batting_key_builder()
    keys.append(_stance_key(K))
    keys.append(K(7, 52, -6, -12, -10, -26, (0.08, 0.12, 0.90), (0.10, 0.26, 0.09),
                  (-0.14, 0.06, 0.09), -0.115, 0.0))
    # A tiny tap of the bat back -- nothing like a backlift.
    keys.append(K(13, 50, -4, -10, -8, -22, (0.06, 0.02, 1.00), (0.10, 0.26, 0.09),
                  (-0.14, 0.05, 0.09), -0.118, 0.0))
    # Press onto the front foot.
    keys.append(K(18, 40, -12, -2, -6, -14, (0.06, 0.22, 0.92), (0.12, 0.36, 0.09),
                  (-0.13, 0.06, 0.09), -0.122, 0.04))
    # CONTACT: hands out in front, low, bat vertical, head over the ball.
    keys.append(K(23, 34, -14, 0, -4, -12, (0.06, 0.30, 0.88), (0.13, 0.40, 0.09),
                  (-0.12, 0.08, 0.09), -0.125, 0.05))
    # Absorb: the hands stay where they are; there is no swing to finish.
    keys.append(K(29, 30, -10, 2, 0, -8, (0.05, 0.28, 0.86), (0.13, 0.40, 0.09),
                  (-0.12, 0.08, 0.09), -0.122, 0.04))
    keys.append(K(36, 44, -7, 0, -6, -16, (0.07, 0.20, 0.90), (0.12, 0.34, 0.09),
                  (-0.13, 0.07, 0.09), -0.118, 0.02))
    return keys


def _batting_key_builder():
    """The batting K, identical to batting_keys()' local K."""
    def K(frame, turn, lean, chest, neck, head, grip, lfoot, rfoot, hips_z, hips_y=0.0,
          hand_gap=0.04):
        spec = spine(turn, lean, chest, neck, head)
        g = Vector(grip)
        lh = g + V(0, 0, hand_gap)
        rh = g - V(0, 0, hand_gap)
        ik = {
            'left_hand': lh,
            'right_hand': rh,
            'left_foot': Vector(lfoot),
            'right_foot': Vector(rfoot),
        }
        return (frame, spec, V(0.0, hips_y, hips_z), ik)
    return K


def bowling_offspin_keys():
    """Off-spin: a shorter, rounder action than pace. The arm circle rides lower and
    more across the body (pole shaping it side-on rather than over the top), the
    bound is smaller, and the release is deliberately lower than pace's -- an
    off-break comes out of the front of the hand at shoulder-to-head height."""
    keys = []
    K = _bowling_key_builder()
    # Mark: ball in both hands at chest height.
    keys.append(K(1, 8, 4, 0, 0, 0, (-0.36, 0.14, -0.06), (-0.02, 0.14, -0.06),
                  (0.10, 0.10, 0.10), (-0.10, 0.08, 0.10), -0.06, 0.0,
                  (-1.0, -0.30, -0.20)))
    # Gather: a shallow sink; spinners gather short.
    keys.append(K(8, 12, 10, -4, 2, -4, (-0.36, 0.16, -0.30), (-0.02, 0.16, -0.30),
                  (0.10, 0.22, 0.10), (-0.10, 0.28, 0.10), -0.15, 0.0,
                  (-1.0, -0.30, -0.30)))
    # Bound: small; the bowling arm starts back but LOW -- a round-arm circle.
    keys.append(K(14, 14, 6, -2, 2, -4, (-0.06, -0.16, -0.22), (0.05, 0.18, 0.08),
                  (0.11, 0.26, 0.20), (-0.09, 0.36, 0.18), -0.05, 0.06,
                  (-1.0, -0.35, 0.05)))
    # Back foot down; the arm comes up but stays angled across, not vertical.
    keys.append(K(20, 16, 4, 0, 4, -2, (-0.10, -0.20, 0.24), (0.07, 0.26, 0.28),
                  (0.12, 0.48, 0.13), (-0.06, 0.40, 0.12), -0.09, 0.18,
                  (-1.0, -0.25, 0.20)))
    # Front foot braces; arm cocked at the top, slightly across.
    keys.append(K(26, 16, -2, 2, 6, 2, (-0.05, 0.02, 0.34), (0.06, 0.30, 0.32),
                  (0.11, 0.62, 0.12), (-0.05, 0.40, 0.12), -0.07, 0.24,
                  (-1.0, -0.05, 0.30)))
    # RELEASE: full extension but LOW and across -- head height, not above it by
    # much. This is the visual signature of finger spin.
    keys.append(K(31, 14, -12, 4, 6, 4, (-0.06, 0.06, 0.30), (0.02, 0.10, -0.10),
                  (0.10, 0.64, 0.12), (-0.05, 0.38, 0.12), -0.04, 0.28,
                  (-1.0, 0.10, 0.18)))
    # Follow-through: the arm folds down past the hip.
    keys.append(K(38, 12, -14, 2, 6, 6, (0.12, 0.20, -0.32), (-0.04, -0.10, -0.28),
                  (0.09, 0.62, 0.12), (-0.07, 0.36, 0.12), -0.10, 0.30,
                  (-1.0, 0.45, -0.20)))
    keys.append(K(46, 8, 6, 0, 2, 2, (-0.02, 0.18, -0.16), (-0.02, 0.14, -0.10),
                  (0.09, 0.42, 0.11), (-0.09, 0.32, 0.11), -0.07, 0.20,
                  (-1.0, -0.15, -0.10)))
    return keys


def bowling_legspin_keys():
    """Leg-spin: the whippy wrist-spin action. A deeper coil at the gather, a
    springier bound, the bowling arm loaded HIGH behind the head, and a release as
    high as a quick's -- the flip side of the off-spinner's low one. The extra
    follow-through lean (the wrist snapping down over the braced leg) is the
    signature of a leg-break bowler."""
    keys = []
    K = _bowling_key_builder()
    keys.append(K(1, 8, 4, 0, 0, 0, (-0.36, 0.14, -0.06), (-0.02, 0.14, -0.06),
                  (0.10, 0.10, 0.10), (-0.10, 0.08, 0.10), -0.06, 0.0,
                  (-1.0, -0.30, -0.20)))
    # Gather: a deep coil, chest closed.
    keys.append(K(8, 18, 14, -6, 2, -4, (-0.36, 0.16, -0.34), (-0.02, 0.16, -0.34),
                  (0.10, 0.26, 0.10), (-0.10, 0.30, 0.10), -0.20, 0.0,
                  (-1.0, -0.35, -0.30)))
    # Bound: bigger than the seamer's; the bowling arm drops LOW behind, loading
    # the whip; the guide arm points high at the batter.
    keys.append(K(14, 16, 8, -2, 2, -4, (-0.10, -0.22, -0.28), (0.06, 0.24, 0.14),
                  (0.11, 0.32, 0.26), (-0.09, 0.42, 0.22), -0.04, 0.12,
                  (-1.0, -0.30, -0.10)))
    # Back foot down; the arm swings up from behind.
    keys.append(K(20, 16, 6, 0, 4, -2, (-0.14, -0.26, 0.26), (0.08, 0.30, 0.30),
                  (0.12, 0.52, 0.14), (-0.06, 0.44, 0.12), -0.10, 0.26,
                  (-1.0, -0.25, 0.28)))
    # Front foot braces; the arm is cocked HIGH behind the head, wrist loaded.
    keys.append(K(26, 16, -4, 2, 6, 2, (-0.10, -0.18, 0.42), (0.06, 0.30, 0.34),
                  (0.11, 0.64, 0.12), (-0.05, 0.42, 0.12), -0.08, 0.30,
                  (-1.0, -0.10, 0.35)))
    # RELEASE: fully extended, very high, just ahead of the shoulder line.
    keys.append(K(31, 14, -18, 4, 6, 4, (-0.02, 0.10, 0.46), (0.02, 0.10, -0.10),
                  (0.10, 0.66, 0.12), (-0.05, 0.40, 0.12), -0.03, 0.33,
                  (-1.0, 0.15, 0.20)))
    # The whip: the arm rips down across the body, torso committed far over the
    # braced leg (lean -18, the most of any action).
    keys.append(K(38, 12, -18, 2, 6, 6, (0.14, 0.24, -0.38), (-0.04, -0.10, -0.30),
                  (0.09, 0.64, 0.12), (-0.07, 0.38, 0.12), -0.11, 0.34,
                  (-1.0, 0.45, -0.25)))
    keys.append(K(46, 8, 6, 0, 2, 2, (-0.02, 0.20, -0.20), (-0.02, 0.16, -0.10),
                  (0.09, 0.44, 0.11), (-0.09, 0.34, 0.11), -0.08, 0.20,
                  (-1.0, -0.20, -0.10)))
    return keys


def _bowling_key_builder():
    """The bowling K, identical to bowling_keys()' local K."""
    def K(frame, turn, lean, chest, neck, head, rh_rel, lh_rel, lfoot, rfoot, hips_z,
          hips_y=0.0, rpole=(-1.0, -0.35, 0.0)):
        spec = spine(turn, lean, chest, neck, head)
        ik = {
            'right_hand_rel': Vector(rh_rel),
            'left_hand_rel': Vector(lh_rel),
            'left_foot': Vector(lfoot),
            'right_foot': Vector(rfoot),
            'right_hand_pole': Vector(rpole),
        }
        return (frame, spec, V(0.0, hips_y, hips_z), ik)
    return K


def main():
    argv = sys.argv
    out_dir = '/tmp/c26anim'
    if '--' in argv:
        rest = argv[argv.index('--') + 1:]
        if '--out' in rest:
            out_dir = rest[rest.index('--out') + 1]
    os.makedirs(out_dir, exist_ok=True)

    arm = bpy.data.objects.get('Armature')
    if arm is None:
        print('C26_ANIM FATAL no Armature object')
        return
    bpy.context.scene.render.fps = 24
    rig = Rig(arm)

    jobs = [
        ('A_C26_BattingDrive', batting_keys(), 1, 36),
        ('A_C26_BattingPull', batting_pull_keys(), 1, 36),
        ('A_C26_BattingCut', batting_cut_keys(), 1, 36),
        ('A_C26_BattingSweep', batting_sweep_keys(), 1, 36),
        ('A_C26_BattingDefence', batting_defence_keys(), 1, 36),
        ('A_C26_BowlingPace', bowling_keys(), 1, 46),
        ('A_C26_BowlingOffSpin', bowling_offspin_keys(), 1, 46),
        ('A_C26_BowlingLegSpin', bowling_legspin_keys(), 1, 46),
    ]
    made = []
    for name, keys, f0, f1 in jobs:
        act, ncurves = build_action(rig, name, keys)
        act.use_frame_range = True
        act.frame_start = f0
        act.frame_end = f1
        made.append((name, act, f0, f1))
        print('C26_ANIM action=%s frames=%d..%d fcurves=%d keys=%d'
              % (name, f0, f1, ncurves, len(keys)))

    arm.animation_data_create()
    for track in list(arm.animation_data.nla_tracks):
        arm.animation_data.nla_tracks.remove(track)
    for name, act, f0, f1 in made:
        tr = arm.animation_data.nla_tracks.new()
        tr.name = name
        tr.strips.new(name, int(f0), act)
        tr.mute = True

    blend_out = os.path.join(out_dir, 'C26_AuthoredAnimations.blend')
    bpy.ops.wm.save_as_mainfile(filepath=blend_out)
    print('C26_ANIM saved %s' % blend_out)
    print('C26_ANIM_DONE')


main()
