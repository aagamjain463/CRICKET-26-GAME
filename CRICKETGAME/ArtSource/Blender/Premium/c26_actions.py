"""The CRICKET 26 cricket action library.

Every clip here is authored, not generated at runtime: poses are placed by cricket technique
(where the front foot strides, where the hands meet the ball, how high the arm is at release) and
the two-bone solver in c26_rig resolves the limbs. Timing is shaped so each action reads as
preparation -> force production -> contact/release -> follow-through -> recovery.

Right-handed is authored; left-handed comes from the sagittal mirror, which is valid for cricket
because a left-hander's stroke and action genuinely reverse. The bat and ball change hands through
the profile's LeftHandedSocket, not by bending a wrist backwards.

Convention: +Y is down the pitch (towards the bowler for a batter, towards the batter for a bowler),
+X is the athlete's LEFT, +Z is up, centimetres. Ankle rests at 8.7, shoulders at 145, head at 163.
Full arm reach is 55.1cm; full leg 87.6cm.
"""
from c26_rig import mirror, R, OFF

GROUND = 8.7
SHOULDER_Z = 145.0


def _bat_hands(top, shaft=(0.06, 0.20, 0.98), face=(0.10, 0.98, -0.10)):
    """Two hands on one handle. The top hand is placed by reach or position; the bottom hand is
    offset down the handle from it, so the grip stays intact no matter where the stroke carries
    the hands."""
    if isinstance(top, tuple) and len(top) > 0 and top[0] == 'R':
        return {'hand_l': top, 'hand_r': OFF('hand_l', -5.5, -2.5, -8.5), 'grip_l': 'bat', 'grip_r': 'bat'}
    return {'hand_l': top, 'shaft': shaft, 'face': face, 'grip_l': 'bat', 'grip_r': 'bat'}


# ================================================================= idles and ready poses

FIELDER_READY = {
    'pelvis': (10, 0, 0, 0, 0, -13), 'spine': (16, 0, 0), 'chest': (6, 0, 0), 'neck': (-16, 0, 0),
    'foot_l': (20, 1, GROUND), 'foot_r': (-20, -1, GROUND),
    'hand_l': R(.74, .34, .78, -.53), 'hand_r': R(.74, -.34, .78, -.53),
    'elbow_l': (52, -14, 96), 'elbow_r': (-52, -14, 96),
    'grip_l': 'flat', 'grip_r': 'flat',
}
FIELDER_READY_SHIFT = dict(FIELDER_READY, **{
    'pelvis': (12, 0, -4, 2, 0, -16), 'spine': (18, 0, 3), 'neck': (-18, 0, -3),
    'foot_l': (21, 3, GROUND), 'foot_r': (-19, -3, GROUND),
    'hand_l': R(.78, .38, .76, -.53), 'hand_r': R(.70, -.30, .78, -.55),
})
FIELDER_KNEES = dict(FIELDER_READY, **{  # hands-on-knees breather between deliveries
    'pelvis': (26, 0, 0, 0, 0, -22), 'spine': (24, 0, 0), 'neck': (-30, 0, 0),
    'hand_l': R(.86, .30, .52, -.80), 'hand_r': R(.86, -.30, .52, -.80),
})

KEEPER_READY = {
    'pelvis': (20, 0, 0, 0, 2, -37), 'spine': (24, 0, 0), 'chest': (8, 0, 0), 'neck': (-26, 0, 0),
    'foot_l': (23, 2, GROUND), 'foot_r': (-23, -2, GROUND),
    'hand_l': R(.88, .14, .66, -.74), 'hand_r': R(.88, -.14, .66, -.74),
    'elbow_l': (46, 2, 52), 'elbow_r': (-46, 2, 52),
    'grip_l': 'keeper', 'grip_r': 'keeper',
}
KEEPER_RISE = dict(KEEPER_READY, **{
    'pelvis': (14, 0, 0, 0, 2, -24), 'spine': (18, 0, 0), 'neck': (-20, 0, 0),
    'hand_l': R(.82, .15, .74, -.65), 'hand_r': R(.82, -.15, .74, -.65),
})

UMPIRE_READY = {
    'pelvis': (2, 0, 0, 0, 0, -2), 'spine': (3, 0, 0), 'neck': (-4, 0, 0),
    'foot_l': (12, 0, GROUND), 'foot_r': (-12, 0, GROUND),
    'hand_l': R(.46, .13, -.46, -.88), 'hand_r': R(.46, -.13, -.46, -.88),  # clasped behind the back
    'elbow_l': (40, -34, 118), 'elbow_r': (-40, -34, 118),
    'grip_l': 'open', 'grip_r': 'open',
}

BOWLER_READY = {
    'pelvis': (5, 0, -6, 0, 0, -5), 'spine': (7, 0, 0), 'neck': (-8, 0, 0),
    'foot_l': (11, 4, GROUND), 'foot_r': (-11, -5, GROUND),
    'hand_l': R(.50, .16, .82, -.55), 'hand_r': R(.50, -.10, .84, -.53),  # ball cupped at the chest
    'elbow_l': (34, -18, 104), 'elbow_r': (-34, -18, 104),
    'grip_l': 'open', 'grip_r': 'seam',
}

# Right-handed stance. Hips ~58 deg closed and shoulders a further 22, so the LEFT shoulder points at
# the bowler; neck and head turn back square so both eyes are level on the ball. Toes point towards
# point (back foot parallel to the crease, front foot a touch open) and every knee tracks its toes -
# a knee aimed down the pitch over a side-on foot is what makes a stance read as a mannequin.
# LEFT hand is the top hand; the bat toe rests just outside the back toe.
import math as _math


def _foot(side, x, y, z=GROUND, toe=-70.0, knee_out=62.0):
    """Planted or travelling foot, its toe direction (rig rz, negative = towards point for a
    right-hander) and a knee pole that keeps the knee over the toes."""
    t = _math.radians(toe)
    dx, dy = _math.sin(t), _math.cos(t)
    return {f'foot_{side}': (x, y, z), f'ankle_{side}': (0, 0, toe),
            f'knee_{side}': (x + dx * 90.0, y + dy * 90.0, knee_out)}


_SWEET = 62.0 - 4.5   # hand_l target to the middle of the blade, measured down the handle


def _unit(v):
    n = _math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


def _bat_at(sweet, shaft, face):
    """Place the bat by where the middle of the blade is - that is where the ball is. `shaft` points
    from the blade up to the handle, `face` is the hitting-face normal. The LEFT (top) hand is solved
    from it and the RIGHT (bottom) hand is derived 8.5cm down the same handle, so the grip can never
    come apart and a right-hander can never end up with the hands swapped."""
    s = _unit(shaft)
    hand = tuple(p + c * _SWEET for p, c in zip(sweet, s))
    return {'hand_l': hand, 'shaft': s, 'face': _unit(face), 'grip_l': 'bat', 'grip_r': 'bat'}


def _bat_held(hand, shaft, face):
    return {'hand_l': hand, 'shaft': _unit(shaft), 'face': _unit(face), 'grip_l': 'bat', 'grip_r': 'bat'}


BATTER_READY_R = {
    'pelvis': (8, 0, -58, 0, -3, -10), 'spine': (16, 0, -10), 'chest': (5, 2, -12),
    'neck': (-12, 0, 34), 'head': (-8, 0, 34),
    **_foot('l', 3, 15, toe=-62), **_foot('r', -1, -21, toe=-86),
    'elbow_l': (-34, 58, 112), 'elbow_r': (-8, -46, 92),
    **_bat_held((-27, 2, 92), (0.16, 0.30, 0.94), (0.12, 0.99, -0.05)),
}
BATTER_READY_TAP = dict(BATTER_READY_R, **{   # bat tap: the stance breathes instead of freezing
    'pelvis': (9, 0, -58, 0, -3, -11), 'spine': (17, 0, -10),
    **_bat_held((-27, 2, 89), (0.16, 0.30, 0.94), (0.12, 0.99, -0.05)),
})

# ================================================================= locomotion

def _locomotion(reach, lift, drop, lean, fwd, back, bat=None, cycle=18):
    """One full gait cycle. The second half is the sagittal mirror of the first, which guarantees a
    symmetric cycle and halves the technique that has to be specified by hand.

    reach  - how far the foot travels fore/aft of the hips
    lift   - swing-leg foot clearance
    drop   - pelvis dip at midstance (the vertical oscillation that stops a run looking like a glide)
    lean   - forward torso lean
    fwd/back - (reach fraction, dx, dy, dz) arm-swing targets, given for the LEFT arm
    """
    def leg(front, rear, rise, arm_fwd_left):
        # arm_fwd_left=True means the LEFT arm is forward, so the RIGHT leg is forward.
        lhand, rhand = (fwd, back) if arm_fwd_left else (back, fwd)
        spec = {
            'pelvis': (lean, 0, -4 if arm_fwd_left else 4, 0, 0, rise),
            'spine': (lean * 0.8, 0, 6 if arm_fwd_left else -6),
            'chest': (lean * 0.4, 0, 8 if arm_fwd_left else -8),
            'neck': (-lean * 0.9, 0, 0),
            'foot_l': front, 'foot_r': rear,
            'knee_l': (front[0], 120, 58), 'knee_r': (rear[0], 120, 58),
            'elbow_l': (46, -58, 112), 'elbow_r': (-46, -58, 112),
        }
        if bat:
            spec.update(_bat_hands(*bat))
            spec['elbow_l'] = (40, -20, 100)
            spec['elbow_r'] = (-24, -30, 96)
        else:
            spec.update({'hand_l': R(lhand[0], lhand[1], lhand[2], lhand[3]),
                         'hand_r': R(rhand[0], -rhand[1], rhand[2], rhand[3]),
                         'grip_l': 'open', 'grip_r': 'open'})
        return spec

    x = 9.0
    contact = leg((x, reach, GROUND + 1), (-x, -reach * 0.80, GROUND + 5.5), -drop * 0.35, False)
    low = leg((x, reach * 0.34, GROUND), (-x, -reach * 0.62, GROUND + lift * 0.55), -drop, False)
    push = leg((x, -reach * 0.24, GROUND + 1.5), (-x, -reach * 0.10, GROUND + lift), -drop * 0.1, True)
    fly = leg((x, -reach * 0.72, GROUND + lift * 0.7), (-x, reach * 0.62, GROUND + lift * 0.45),
              drop * 0.55, True)
    h = cycle // 2
    keys = [(0, contact), (h // 4, low), (h // 2, push), (3 * h // 4, fly),
            (h, mirror(contact)), (h + h // 4, mirror(low)), (h + h // 2, mirror(push)),
            (h + 3 * h // 4, mirror(fly)), (cycle, contact)]
    return keys


RUN = _locomotion(reach=32, lift=26, drop=5.5, lean=13,
                  fwd=(.62, .22, .74, -.64), back=(.70, .32, -.54, -.78), cycle=16)
SPRINT = _locomotion(reach=36, lift=30, drop=6.5, lean=19,
                     fwd=(.60, .18, .80, -.58), back=(.74, .34, -.58, -.72), cycle=14)
WALK = _locomotion(reach=24, lift=12, drop=3.0, lean=4,
                   fwd=(.74, .26, .44, -.86), back=(.78, .30, -.38, -.88), cycle=30)
JOG = _locomotion(reach=28, lift=18, drop=4.5, lean=9,
                  fwd=(.68, .24, .60, -.76), back=(.74, .32, -.46, -.84), cycle=20)
# A batter runs carrying the bat, so the arms cannot swing freely - that is what makes running
# between the wickets read differently from a fielder's sprint.
BATTER_RUN_R = _locomotion(reach=30, lift=24, drop=5.0, lean=15,
                           fwd=(.62, .22, .74, -.64), back=(.70, .32, -.54, -.78),
                           bat=((-12, 14, 98), (0.08, 0.35, 0.93), (0.10, 0.98, -0.10)), cycle=16)


def _still(pose):
    return dict(pose)


# start: from ready, drive the first two strides out
START = [
    (0, FIELDER_READY),
    (4, dict(FIELDER_READY, **{'pelvis': (24, 0, 0, 0, 3, -18), 'spine': (26, 0, 0),
                               'foot_l': (18, -14, GROUND), 'foot_r': (-18, 4, GROUND),
                               'hand_l': R(0.96, 0.04, 0.54, -0.84), 'hand_r': R(0.96, -0.08, -0.20, -0.98)})),
    (10, RUN[2][1]), (18, RUN[4][1]), (26, RUN[0][1]),
]
STOP = [
    (0, RUN[0][1]),
    (7, dict(RUN[2][1], **{'pelvis': (-14, 0, 0, 0, -4, -20), 'spine': (-6, 0, 0),
                           'foot_l': (13, 34, GROUND), 'foot_r': (-15, -18, GROUND)})),
    (15, dict(FIELDER_READY, **{'pelvis': (6, 0, 0, 0, -2, -22), 'spine': (10, 0, 0)})),
    (24, FIELDER_READY),
]


def _turn(sign):
    """Plant the outside foot, lean into the turn, cross over. `sign` +1 turns to the athlete's
    left (+X), -1 to the right."""
    return [
        (0, RUN[0][1]),
        (6, dict(RUN[1][1], **{'pelvis': (12, 9 * sign, -26 * sign, 0, 0, -9),
                               'spine': (12, 7 * sign, 16 * sign), 'neck': (-12, 0, 24 * sign),
                               'foot_l': (9 + 7 * sign, 30, GROUND), 'foot_r': (-9 + 5 * sign, -22, GROUND + 7)})),
        (14, dict(RUN[3][1], **{'pelvis': (12, 6 * sign, -40 * sign, 0, 0, -5),
                                'spine': (12, 4 * sign, 20 * sign), 'neck': (-12, 0, 20 * sign)})),
        (22, RUN[4][1]), (30, RUN[0][1]),
    ]


# ================================================================= batting
#
# Round 4 library. Every stroke is authored as real technique in the batter's own frame - where the
# feet go, where the middle of the bat meets the ball, which way the face points - and baked densely
# (c26_rig.spec_at), so the pose is re-solved on every frame instead of blending bone rotations.
#
# RIGHT-HANDED INVARIANTS (enforced by batting_lab.py before anything is exported):
#   * LEFT shoulder, LEFT hip and LEFT foot lead down the pitch; the left foot is always the front foot
#   * LEFT hand is the top hand (`_bat_at`/`_bat_held` solve it), RIGHT hand sits 8.5cm below it
#   * off side is -X, leg side is +X; a stroke `direction` of 0 is straight, positive is off side
#   * the bat never passes through the torso, head or thighs on any frame
# Left-handers are the sagittal mirror of these clips, never a separately bent set of arms.
#
# Phases on the 30fps clock: stance 0 -> trigger 6 -> backlift 11 -> stride/load 17 -> plant 22 ->
# downswing 26 -> contact -> extension -> finish -> hold -> recovery -> stance.

G = GROUND


def _stroke(name, beats, contact, direction, footwork, length, face_tolerance=38):
    """Beats are cumulative: each one only states what changes, so a stroke reads like coaching
    notes and a joint nobody mentioned keeps doing what it was doing."""
    keys, pose = [(0, BATTER_READY_R)], dict(BATTER_READY_R)
    for frame, delta in beats:
        pose = dict(pose, **delta)
        keys.append((frame, pose))
    keys.append((length, BATTER_READY_R))
    return {'name': name, 'event': 'BatContact', 'contact': contact, 'keys': keys, 'dense': True,
            'direction': direction, 'footwork': footwork, 'face_tolerance': face_tolerance}


def _trigger(back=(-7, -25), hands=(-28, -2, 100)):
    """Small back-and-across press: the back foot moves first, the hands start up outside the back
    knee so the blade clears the pad."""
    mid = ((back[0] - 1) * 0.5, (back[1] - 21) * 0.5)
    return [
        (3, {**_foot('r', mid[0], mid[1], G + 3.5, toe=-87)}),
        (6, {'pelvis': (8, 0, -60, -2, -6, -11), **_foot('r', back[0], back[1], toe=-88),
             **_bat_held(hands, (0.30, 0.60, 0.74), (0.10, 0.76, -0.64))}),
    ]


def _backlift(hands=(-20, -12, 118), shaft=(0.12, 0.62, -0.78), pelvis=(7, 0, -62, -2, -7, -11)):
    """Hands up over the back hip, blade up towards the slips, face open to point."""
    return (11, {'pelvis': pelvis, 'spine': (13, 0, -12), 'chest': (4, -4, -14),
                 'elbow_l': (-40, 40, 150), 'elbow_r': (-10, -60, 120),
                 **_bat_held(hands, shaft, (-0.70, -0.45, -0.55))})


def _front_load(foot, toe, pelvis_stride, pelvis_plant, hands_top, hands_plant, plant_shaft, plant_face,
                head_down=4):
    """Front-foot load: the head leads, the front foot travels in the air and lands on the line of the
    ball, the weight follows it and the downswing starts only once the foot is down."""
    mid = (foot[0] * 0.55 + 3 * 0.45, foot[1] * 0.62 + 15 * 0.38)
    return [
        (17, {**_foot('l', mid[0], mid[1], G + 9, toe=(toe - 62) * 0.5), 'pelvis': pelvis_stride,
              'spine': (17, 0, -10), 'neck': (-14, 0, 32), 'head': (-2, 0, 32),
              **_bat_held(hands_top, (0.12, 0.62, -0.78), (-0.70, -0.45, -0.55))}),
        (22, {**_foot('l', foot[0], foot[1], toe=toe), 'pelvis': pelvis_plant,
              'spine': (19, 0, -8), 'chest': (7, 0, -9), 'head': (head_down, 0, 30),
              **_bat_held(hands_plant, plant_shaft, plant_face)}),
    ]


def _back_load(back, front, back_toe=-88, front_toe=-62, hands_top=(-26, -10, 124)):
    """Back-foot load: the back foot goes back and across first, the front foot follows it back,
    the body stays tall so the ball can be played under the eyes. The blade stays up over off
    stump, well away from the back of the head."""
    bmid = ((back[0] - 7) * 0.5, (back[1] - 25) * 0.5)
    fmid = ((front[0] + 3) * 0.5, (front[1] + 15) * 0.5)
    return [
        (14, {**_foot('r', bmid[0], bmid[1], G + 7, toe=back_toe), 'pelvis': (6, 0, -60, -3, -14, -9),
              'spine': (10, 0, -12), 'neck': (-8, 0, 34), 'head': (-10, 0, 34),
              **_bat_held(hands_top, (0.35, 0.30, -0.89), (-0.80, -0.35, -0.45))}),
        (18, {**_foot('r', back[0], back[1], toe=back_toe), 'pelvis': (5, 0, -62, -4, -20, -8)}),
        (22, {**_foot('l', fmid[0], fmid[1], G + 7, toe=front_toe)}),
    ]


def _recover(front_from, front_toe, back_from, back_toe, length, over=True):
    """Walk back into the stance: the bat comes down in front of the body (never back over the head),
    then the front foot, then the back foot - never both feet off the ground at once. `over` is for
    finishes with the blade up behind the shoulder; blocks and glances already have it down."""
    py = (front_from[1] + back_from[1]) * 0.5
    if over:
        # Relax the bat down the leg side: the blade drops beside the body, never over the helmet.
        carry = [_bat_held((6, py + 24, 118), (-0.90, 0.35, -0.25), (0.30, 0.90, -0.30)),
                 _bat_held((min(-30, front_from[0] - 20), py * 0.5 + 26, 98), (0.05, -0.20, 0.98), (0.05, 0.98, 0.20))]
    else:
        carry = [_bat_held((-30, py + 18, 96), (0.05, 0.20, 0.98), (0.0, 0.98, -0.20)),
                 _bat_held((-29, (py + 6) * 0.5 + 2, 94), (0.12, 0.28, 0.95), (0.08, 0.95, -0.28))]
    return [
        (length - 17, {'pelvis': (10, 0, -50, -2, py, -13), 'spine': (15, 0, -6), 'chest': (5, 0, -8),
                       'neck': (-12, 0, 30), 'head': (-6, 0, 32),
                       'elbow_l': (-34, 58, 130), 'elbow_r': (-20, -20, 110), **carry[0]}),
        (length - 13, {**_foot('l', (front_from[0] + 3) * 0.5, (front_from[1] + 15) * 0.5, G + 7,
                               toe=(front_toe - 62) * 0.5),
                       'pelvis': (9, 0, -54, -1, (py - 3) * 0.5, -12), **carry[1]}),
        (length - 9, {**_foot('l', 3, 15, toe=-62), 'spine': (16, 0, -9), 'chest': (5, 1, -11),
                      'neck': (-12, 0, 33), 'head': (-8, 0, 34),
                      'elbow_l': (-34, 58, 116), 'elbow_r': (-8, -46, 96)}),
        (length - 5, {**_foot('r', (back_from[0] - 1) * 0.5, (back_from[1] - 21) * 0.5, G + 3.5,
                              toe=(back_toe - 86) * 0.5)}),
    ]


# ---------------------------------------------------------------- front foot

STRAIGHT_DRIVE = _stroke('STRAIGHTDRIVE', [
    *_trigger(), _backlift(),
    *_front_load((-4, 50), -22, (11, 0, -58, -3, 6, -14), (15, 0, -52, -4, 17, -17),
                 (-22, -8, 121), (-24, 4, 112), (0.05, 0.90, -0.43), (-0.50, 0.20, -0.84)),
    (26, {'pelvis': (15, 0, -48, -4, 21, -18),
          **_bat_held((-28, 34, 96), (0.05, 0.62, 0.78), (-0.10, 0.78, -0.62))}),
    # Head over the front knee, full face presented straight back past the bowler, the bat coming
    # down just outside the front pad.
    (30, {'pelvis': (15, 0, -44, -4, 23, -19), 'spine': (20, 4, -2), 'chest': (8, 6, 0),
          'neck': (-16, 0, 24), 'head': (6, 0, 26), 'elbow_l': (-30, 80, 150), 'elbow_r': (-20, 10, 80),
          **_bat_at((-22, 62, 22), (0.0, 0.25, 0.97), (0.0, 1.0, -0.2))}),
    (34, {'pelvis': (14, 0, -40, -4, 24, -18), 'spine': (18, 4, 2), 'neck': (-14, 0, 20), 'head': (2, 0, 22),
          **_bat_held((-20, 82, 100), (0.0, -0.30, 0.95), (0.0, 0.95, 0.30))}),
    (42, {'pelvis': (12, 0, -32, -3, 20, -15), 'spine': (12, 0, 6), 'chest': (4, 0, 6),
          'neck': (-10, 0, 12), 'head': (-4, 0, 14), 'elbow_l': (-10, 70, 170), 'elbow_r': (-30, 40, 150),
          **_foot('r', -7, -23, G + 3, toe=-72),
          **_bat_held((-6, 50, 148), (-0.65, 0.45, -0.62), (0.55, 0.80, 0.25))}),
    (48, {'pelvis': (11, 0, -34, -3, 17, -14), **_foot('r', -7, -23, toe=-76),
          **_bat_held((-6, 46, 144), (-0.65, 0.45, -0.62), (0.55, 0.80, 0.25))}),
    *_recover((-4, 50), -22, (-7, -23), -76, 70),
], contact=30, direction=0, footwork='front', length=70)

COVER_DRIVE = _stroke('COVERDRIVE', [
    *_trigger(), _backlift(),
    # Stride goes towards the pitch of the ball outside off, not merely forward.
    *_front_load((-20, 44), -36, (11, 0, -60, -7, 5, -14), (16, 0, -58, -11, 15, -18),
                 (-22, -8, 122), (-30, 2, 113), (0.10, 0.92, -0.38), (-0.60, 0.15, -0.78)),
    (26, {'pelvis': (17, 0, -56, -12, 19, -19), 'spine': (21, -4, -10),
          **_bat_held((-38, 30, 98), (0.12, 0.60, 0.79), (-0.45, 0.70, -0.55))}),
    # High front elbow, shoulders still side-on, face opened towards cover.
    (30, {'pelvis': (17, 0, -52, -12, 21, -20), 'spine': (22, -2, -6), 'chest': (9, 2, -4),
          'neck': (-16, 0, 28), 'head': (8, 0, 28), 'elbow_l': (-40, 70, 160), 'elbow_r': (-30, 10, 80),
          **_bat_at((-44, 56, 20), (-0.18, 0.22, 0.96), (-0.66, 0.74, -0.10))}),
    (34, {'pelvis': (16, 0, -44, -12, 22, -19), 'spine': (20, -2, -2), 'head': (4, 0, 24),
          **_bat_held((-50, 70, 100), (-0.40, -0.22, 0.89), (-0.60, 0.70, 0.35))}),
    (42, {'pelvis': (13, 0, -22, -8, 18, -16), 'spine': (12, 0, 10), 'chest': (4, 0, 8),
          'neck': (-10, 0, 6), 'head': (-4, 0, 8), 'elbow_l': (-20, 70, 175), 'elbow_r': (-40, 30, 150),
          **_foot('r', -8, -23, G + 3, toe=-66),
          **_bat_held((-10, 56, 146), (-0.75, 0.45, -0.49), (0.55, 0.80, 0.20))}),
    (48, {'pelvis': (12, 0, -26, -8, 16, -15), **_foot('r', -8, -23, toe=-72),
          **_bat_held((-10, 54, 142), (-0.75, 0.45, -0.49), (0.55, 0.80, 0.20))}),
    *_recover((-20, 44), -36, (-8, -23), -72, 70),
], contact=30, direction=40, footwork='front', length=70)

ON_DRIVE = _stroke('ONDRIVE', [
    *_trigger(), _backlift(),
    *_front_load((6, 46), -12, (11, 0, -54, -1, 6, -14), (15, 0, -46, 1, 16, -17),
                 (-20, -8, 121), (-22, 4, 112), (0.02, 0.90, -0.43), (-0.30, 0.30, -0.90)),
    # The front hip opens early so the bat can come through around the front pad.
    (26, {'pelvis': (16, 0, -38, 1, 20, -18), 'spine': (19, 2, 0),
          **_bat_held((-26, 32, 96), (0.22, 0.60, 0.77), (0.20, 0.78, -0.60))}),
    (30, {'pelvis': (16, 0, -30, 1, 22, -19), 'spine': (20, 4, 6), 'chest': (8, 4, 4),
          'neck': (-16, 0, 18), 'head': (6, 0, 20), 'elbow_l': (-10, 80, 150), 'elbow_r': (-20, 10, 80),
          **_bat_at((-14, 64, 24), (0.12, 0.22, 0.97), (0.42, 0.90, -0.10))}),
    (34, {'pelvis': (15, 0, -18, 1, 22, -18), 'spine': (18, 2, 10), 'head': (2, 0, 14),
          **_bat_held((-2, 82, 100), (0.25, -0.30, 0.92), (0.45, 0.85, 0.30))}),
    (38, {'pelvis': (13, 0, -10, 2, 20, -16), 'spine': (14, 0, 14),
          **_bat_held((8, 62, 124), (-0.90, -0.30, 0.0), (0.10, 0.20, 0.97))}),
    (42, {'pelvis': (12, 0, -4, 2, 18, -15), 'spine': (11, 0, 16), 'chest': (4, 0, 10),
          'neck': (-10, 0, -2), 'head': (-4, 0, 0), 'elbow_l': (10, 70, 170), 'elbow_r': (-30, 40, 150),
          **_foot('r', -5, -21, G + 5, toe=-50),
          **_bat_held((14, 40, 148), (-0.75, 0.35, -0.56), (0.55, 0.80, 0.20))}),
    (48, {'pelvis': (11, 0, -10, 2, 16, -14), **_foot('r', -5, -21, toe=-60),
          **_bat_held((12, 38, 144), (-0.75, 0.35, -0.56), (0.55, 0.80, 0.20))}),
    (52, {'pelvis': (11, 0, -26, 1, 14, -14), 'spine': (13, 0, 6), 'chest': (4, 0, 0),
          'neck': (-10, 0, 14), 'head': (-4, 0, 16),
          **_bat_held((-2, 64, 130), (-0.20, -0.30, -0.93), (0.20, -0.93, 0.30))}),
    *_recover((6, 46), -12, (-5, -21), -60, 72),
], contact=30, direction=-25, footwork='front', length=72)

FORWARD_DEFENCE = _stroke('FRONTFOOTDEFENCE', [
    *_trigger(), _backlift(hands=(-22, -7, 108), shaft=(0.10, 0.75, -0.65)),
    *_front_load((-6, 42), -22, (12, 0, -58, -3, 4, -15), (17, 0, -54, -4, 16, -19),
                 (-22, -5, 110), (-26, 6, 104), (0.05, 0.95, -0.30), (-0.30, 0.40, -0.86), head_down=8),
    (26, {'pelvis': (18, 0, -52, -4, 19, -21), 'spine': (22, 2, -6),
          **_bat_held((-28, 30, 88), (0.0, 0.60, 0.80), (-0.05, 0.80, -0.60))}),
    # Bat and pad together, handle ahead of the blade so the ball goes down, soft bottom hand.
    (30, {'pelvis': (19, 0, -50, -4, 21, -22), 'spine': (22, 4, -4), 'chest': (10, 6, -2),
          'neck': (-18, 0, 26), 'head': (12, 0, 28), 'elbow_l': (-30, 70, 150), 'elbow_r': (-30, 0, 70),
          **_bat_at((-26, 56, 20), (-0.05, 0.38, 0.92), (0.0, 0.92, -0.38))}),
    (35, {**_bat_held((-29, 74, 74), (-0.05, 0.34, 0.94), (0.0, 0.94, -0.34))}),   # give with the ball
    (44, {'pelvis': (18, 0, -51, -4, 20, -21),
          **_bat_held((-29, 73, 75), (-0.05, 0.34, 0.94), (0.0, 0.94, -0.34))}),
    *_recover((-6, 42), -22, (-7, -25), -88, 64, over=False),
], contact=30, direction=0, footwork='front', length=64)

LOFTED_DRIVE = _stroke('LOFTEDDRIVE', [
    *_trigger(hands=(-28, -4, 104)),
    _backlift(hands=(-20, -16, 128), shaft=(0.10, 0.45, -0.89), pelvis=(6, 0, -63, -2, -8, -10)),
    # Bigger stride, the hands wait longer and the whole body drives up through the ball.
    *_front_load((-12, 56), -26, (11, 0, -60, -4, 8, -14), (15, 0, -54, -6, 20, -18),
                 (-20, -12, 131), (-26, 0, 124), (0.10, 0.95, -0.30), (-0.55, 0.20, -0.81)),
    (26, {'pelvis': (15, 0, -48, -6, 24, -18), 'spine': (19, 0, -4),
          **_bat_held((-30, 34, 104), (0.0, 0.75, 0.66), (-0.20, 0.66, -0.72))}),
    # Hands level with the ball rather than ahead of it: the bat meets it on the up, face open.
    (30, {'pelvis': (14, 0, -42, -6, 27, -18), 'spine': (18, 2, 0), 'chest': (6, 4, 0),
          'neck': (-14, 0, 24), 'head': (6, 0, 24), 'elbow_l': (-30, 80, 160), 'elbow_r': (-30, 20, 90),
          **_bat_at((-28, 68, 30), (-0.08, 0.02, 1.0), (-0.25, 0.92, 0.30))}),
    (34, {'pelvis': (12, 0, -32, -6, 27, -16), 'spine': (14, 0, 4), 'head': (2, 0, 20),
          'elbow_l': (-20, 90, 180), 'elbow_r': (-30, 50, 150),
          **_bat_held((-24, 84, 130), (-0.12, -0.55, 0.83), (-0.10, 0.83, 0.55))}),
    (44, {'pelvis': (8, 0, -18, -5, 22, -12), 'spine': (4, 0, 12), 'chest': (-2, 0, 8),
          'neck': (-8, 0, 2), 'head': (-10, 0, 4), 'elbow_l': (-10, 60, 200), 'elbow_r': (-40, 30, 190),
          **_foot('r', -9, -19, G + 7, toe=-58),
          **_bat_held((6, 46, 162), (-0.72, 0.50, -0.48), (0.50, 0.60, 0.62))}),
    (50, {'pelvis': (9, 0, -22, -5, 20, -13), **_foot('r', -9, -17, toe=-62),
          **_bat_held((6, 44, 156), (-0.72, 0.50, -0.48), (0.50, 0.60, 0.62))}),
    *_recover((-12, 56), -26, (-9, -17), -62, 74),
], contact=30, direction=15, footwork='front', length=74)

# ---------------------------------------------------------------- back foot

BACK_FOOT_DEFENCE = _stroke('BACKFOOTDEFENCE', [
    *_trigger(), _backlift(hands=(-22, -10, 116)),
    *_back_load((-12, -40), (-3, -6)),
    (25, {**_foot('l', -3, -6, toe=-60), 'pelvis': (5, 0, -62, -4, -22, -7), 'spine': (8, 0, -12),
          'neck': (-8, 0, 32), 'head': (0, 0, 32),
          **_bat_held((-28, 2, 130), (0.0, 0.85, 0.52), (-0.10, 0.52, -0.85))}),
    # Tall, on the toes of the back foot, dead bat under the eyes with the top elbow high.
    (29, {'pelvis': (6, 0, -62, -4, -22, -6), 'spine': (10, 0, -12), 'chest': (4, 0, -12),
          'neck': (-8, 0, 30), 'head': (10, 0, 30), 'elbow_l': (-30, 50, 185), 'elbow_r': (-30, -10, 110),
          **_bat_at((-24, 4, 80), (-0.05, 0.30, 0.95), (0.0, 0.95, -0.30))}),
    (34, {**_bat_held((-26, 18, 131), (-0.05, 0.26, 0.96), (0.0, 0.96, -0.26))}),
    (42, {'pelvis': (6, 0, -62, -4, -21, -6),
          **_bat_held((-26, 17, 130), (-0.05, 0.26, 0.96), (0.0, 0.96, -0.26))}),
    (48, {**_foot('l', 0, 5, G + 7, toe=-62), 'pelvis': (7, 0, -60, -2, -12, -9),
          **_bat_held((-28, 6, 104), (0.16, 0.40, 0.90), (0.10, 0.91, -0.40))}),
    (53, {**_foot('l', 3, 15, toe=-62)}),
    (57, {**_foot('r', -6, -30, G + 5, toe=-87)}),
], contact=29, direction=0, footwork='back', length=62)

BACK_FOOT_PUNCH = _stroke('BACKFOOTPUNCH', [
    *_trigger(), _backlift(hands=(-23, -12, 120)),
    *_back_load((-20, -36), (-8, -4), hands_top=(-26, -12, 126)),
    (25, {**_foot('l', -8, -4, toe=-58), 'pelvis': (6, 0, -64, -8, -18, -6), 'spine': (9, -3, -12),
          'neck': (-8, 0, 32), 'head': (2, 0, 32),
          **_bat_held((-32, 0, 132), (0.05, 0.85, 0.52), (-0.50, 0.40, -0.77))}),
    # Up on the back toes, punched through the line outside off: short, firm, checked.
    (29, {'pelvis': (8, 0, -60, -10, -16, -5), 'spine': (12, -5, -10), 'chest': (5, -4, -10),
          'neck': (-10, 0, 30), 'head': (10, 0, 28), 'elbow_l': (-50, 50, 175), 'elbow_r': (-40, 0, 100),
          **_bat_at((-38, 8, 84), (-0.10, 0.25, 0.96), (-0.75, 0.65, -0.10))}),
    (33, {'pelvis': (8, 0, -54, -10, -14, -5),
          **_bat_held((-46, 26, 124), (-0.20, -0.45, 0.87), (-0.60, 0.70, 0.40))}),
    (38, {'spine': (10, -3, -6),
          **_bat_held((-44, 28, 130), (-0.12, -0.62, -0.78), (-0.50, 0.75, -0.45))}),
    (45, {**_bat_held((-43, 27, 129), (-0.12, -0.62, -0.78), (-0.50, 0.75, -0.45))}),
    (50, {**_bat_held((-38, 18, 120), (-0.05, -0.95, 0.30), (-0.50, 0.25, 0.83))}),
    (54, {**_foot('l', -3, 6, G + 7, toe=-62), 'pelvis': (7, 0, -60, -4, -10, -9), 'spine': (15, 0, -9),
          'chest': (5, 0, -11), 'neck': (-10, 0, 32), 'head': (-6, 0, 33),
          'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98),
          **_bat_held((-32, 8, 106), (-0.05, -0.50, 0.86), (0.0, 0.86, 0.50))}),
    (58, {**_foot('l', 3, 15, toe=-62)}),
    (62, {**_foot('r', -10, -28, G + 5, toe=-87)}),
], contact=29, direction=50, footwork='back', length=67)

SQUARE_CUT = _stroke('SQUARECUT', [
    *_trigger(), _backlift(hands=(-22, -12, 128), shaft=(0.25, 0.35, -0.90)),
    # Back foot goes across towards the ball, the body makes room, the bat comes from high.
    *_back_load((-26, -30), (-10, 2), back_toe=-92, hands_top=(-26, -10, 134)),
    (25, {**_foot('l', -10, 2, toe=-55), 'pelvis': (8, -4, -68, -10, -14, -8), 'spine': (12, -6, -14),
          'chest': (5, -5, -14), 'neck': (-10, 0, 34), 'head': (4, 0, 34),
          'elbow_l': (-50, 30, 190), 'elbow_r': (-30, -30, 150),
          **_bat_held((-32, -4, 140), (0.20, -0.40, -0.90), (-0.90, 0.30, -0.30))}),
    # Arms extend out to the width of the ball; the blade runs flat towards the bowler and the face
    # slices down towards point.
    (29, {'pelvis': (10, -8, -72, -12, -12, -10), 'spine': (16, -10, -16), 'chest': (7, -6, -14),
          'neck': (-12, 0, 36), 'head': (10, 0, 34), 'elbow_l': (-70, 30, 140), 'elbow_r': (-50, -10, 110),
          **_bat_at((-50, 40, 86), (-0.12, -0.93, 0.35), (-0.95, 0.25, -0.20))}),
    (33, {'pelvis': (10, -6, -64, -12, -11, -10), 'spine': (14, -6, -8),
          **_bat_held((-48, 10, 100), (0.10, -0.70, 0.70), (-0.95, 0.10, 0.20))}),
    # The blade keeps travelling down and round, finishing low on the leg side of the front knee.
    (40, {'pelvis': (8, -2, -56, -10, -11, -9), 'spine': (12, -2, -6), 'chest': (5, 0, -6),
          'neck': (-10, 0, 26), 'head': (4, 0, 28), 'elbow_l': (-40, 40, 150), 'elbow_r': (-40, 10, 120),
          **_bat_held((-36, 16, 104), (-0.30, -0.50, 0.81), (0.20, -0.85, -0.45))}),
    (47, {**_bat_held((-35, 15, 106), (-0.30, -0.50, 0.81), (0.20, -0.85, -0.45))}),
    (55, {**_foot('l', -3, 8, G + 7, toe=-62), 'pelvis': (7, 0, -60, -5, -9, -9), 'spine': (15, 0, -9),
          'chest': (5, 0, -11), 'neck': (-10, 0, 32), 'head': (-6, 0, 33),
          'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98),
          **_bat_held((-34, 8, 100), (0.10, 0.20, 0.97), (0.05, 0.97, -0.20))}),
    (59, {**_foot('l', 3, 15, toe=-62)}),
    (63, {**_foot('r', -13, -26, G + 5, toe=-88)}),
], contact=29, direction=75, footwork='back', length=68)

PULL = _stroke('PULL', [
    *_trigger(), _backlift(hands=(-26, -14, 130), shaft=(0.30, 0.25, -0.92)),
    *_back_load((-16, -32), (8, -2), front_toe=-30, hands_top=(-24, -14, 136)),
    # Hips and shoulders open hard to face the bowler; the bat comes down from high across the line.
    (25, {**_foot('l', 12, -2, toe=-10), 'pelvis': (6, 0, -34, -2, -16, -10), 'spine': (10, 0, 2),
          'chest': (4, 0, 0), 'neck': (-10, 0, 18), 'head': (0, 0, 20),
          'elbow_l': (-20, 20, 190), 'elbow_r': (-40, -40, 140),
          **_bat_held((-24, -2, 132), (0.80, 0.30, -0.52), (-0.20, 0.90, 0.40))}),
    (28, {'pelvis': (6, 0, -8, 0, -15, -12), 'spine': (10, 0, 14), 'chest': (4, 0, 10),
          'neck': (-10, 0, -2), 'head': (6, 0, 0), 'elbow_l': (10, 40, 150), 'elbow_r': (-10, 20, 120),
          **_foot('r', -16, -32, toe=-60),
          **_bat_at((-12, 60, 110), (0.42, -0.90, 0.05), (0.90, 0.42, -0.10))}),
    (32, {'pelvis': (6, 0, 10, 2, -14, -12), 'spine': (8, 0, 20), 'chest': (2, 0, 12),
          'neck': (-8, 0, -18), 'head': (0, 0, -16), **_foot('r', -16, -32, toe=-40),
          **_bat_held((20, 16, 118), (-0.40, -0.90, -0.10), (0.30, -0.10, -0.95))}),
    (38, {'pelvis': (5, 0, 16, 2, -14, -11), 'spine': (6, 0, 22), 'elbow_l': (30, 10, 160),
          **_bat_held((26, -2, 132), (-0.60, 0.70, -0.38), (-0.60, -0.70, -0.38))}),
    (44, {**_bat_held((18, -8, 146), (-0.30, 0.70, -0.65), (-0.60, -0.55, 0.58)),
          'pelvis': (5, 0, 10, 1, -14, -11), 'spine': (8, 0, 16), 'neck': (-8, 0, -10), 'head': (-4, 0, -8)}),
    (49, {**_bat_held((6, 12, 118), (-0.90, 0.35, -0.25), (0.30, 0.90, -0.30)),
          'pelvis': (6, 0, -12, 0, -12, -11), 'spine': (10, 0, 6), 'neck': (-10, 0, 10), 'head': (-4, 0, 12)}),
    (53, {**_foot('l', 5, 6, G + 7, toe=-40), 'pelvis': (7, 0, -40, -1, -10, -10),
          'spine': (12, 0, -4), 'chest': (4, 0, -8), 'neck': (-10, 0, 26), 'head': (-6, 0, 28),
          **_foot('r', -16, -32, toe=-80), 'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98),
          **_bat_held((-30, 18, 98), (0.05, -0.20, 0.98), (0.05, 0.98, 0.20))}),
    (57, {**_foot('l', 3, 15, toe=-62)}),
    (61, {**_foot('r', -8, -26, G + 5, toe=-86)}),
], contact=28, direction=-65, footwork='back', length=67)

HOOK = _stroke('HOOK', [
    *_trigger(), _backlift(hands=(-26, -14, 134), shaft=(0.30, 0.20, -0.93)),
    *_back_load((-14, -36), (14, -8), front_toe=-20, hands_top=(-24, -14, 142)),
    # Ball at the head: the head moves inside the line, the bat whips over the top and rolls down.
    (25, {**_foot('l', 16, -8, toe=0), 'pelvis': (3, -4, -40, -4, -18, -7), 'spine': (5, -4, 0),
          'chest': (1, -3, -2), 'neck': (-12, 0, 20), 'head': (-10, 0, 22),
          'elbow_l': (-10, 10, 210), 'elbow_r': (-40, -40, 170),
          **_bat_held((-22, -8, 148), (0.85, 0.30, -0.43), (-0.20, 0.90, 0.40))}),
    (26, {'pelvis': (2, -5, -30, -4, -18, -6), 'spine': (4, -5, 5), 'chest': (0, -4, 3),
          **_bat_held((-14, -2, 152), (0.45, -0.60, -0.66), (0.55, 0.75, -0.30))}),
    (29, {'pelvis': (2, -8, -4, -5, -18, -6), 'spine': (4, -8, 18), 'chest': (0, -6, 12),
          'neck': (-14, 0, -8), 'head': (-12, 0, -6), 'elbow_l': (10, 30, 190), 'elbow_r': (-10, 20, 160),
          **_foot('r', -14, -36, toe=-50),
          **_bat_at((10, 54, 150), (-0.30, -0.93, -0.10), (0.90, -0.30, -0.30))}),
    (32, {'pelvis': (2, -6, 20, -3, -17, -6), 'spine': (4, -4, 24), 'chest': (0, 0, 14),
          'neck': (-12, 0, -26), 'head': (-10, 0, -20), **_foot('r', -14, -36, toe=-24),
          **_bat_held((26, 8, 146), (-0.50, -0.80, 0.33), (0.20, -0.50, -0.84))}),
    (38, {'pelvis': (3, -2, 26, -2, -16, -7), 'spine': (4, 0, 26), 'elbow_l': (40, 0, 180),
          **_bat_held((28, -10, 150), (-0.50, 0.75, -0.43), (-0.40, -0.70, -0.58))}),
    (44, {**_bat_held((20, -12, 156), (-0.30, 0.72, -0.62), (-0.60, -0.55, 0.58)),
          'pelvis': (4, 0, 16, -2, -16, -8), 'spine': (6, 0, 18)}),
    (49, {**_bat_held((6, 10, 120), (-0.90, 0.35, -0.25), (0.30, 0.90, -0.30)),
          'pelvis': (6, 0, -12, -1, -14, -9), 'spine': (10, 0, 6), 'neck': (-10, 0, 10), 'head': (-4, 0, 12)}),
    (53, {**_foot('l', 7, 4, G + 7, toe=-36), 'pelvis': (7, 0, -40, -2, -12, -10),
          'spine': (12, 0, -4), 'chest': (4, 0, -8), 'neck': (-10, 0, 26), 'head': (-6, 0, 28),
          **_foot('r', -14, -36, toe=-80), 'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98),
          **_bat_held((-30, 16, 98), (0.05, -0.20, 0.98), (0.05, 0.98, 0.20))}),
    (57, {**_foot('l', 3, 15, toe=-62)}),
    (61, {**_foot('r', -7, -28, G + 5, toe=-86)}),
], contact=29, direction=-105, footwork='back', length=67, face_tolerance=45)

# ---------------------------------------------------------------- spin and aggression

SWEEP = _stroke('SWEEP', [
    *_trigger(hands=(-28, -4, 96)), _backlift(hands=(-24, -8, 112), shaft=(0.15, 0.70, -0.70)),
    # Long stride down the line, back knee goes to the turf, head down over the front knee.
    (17, {**_foot('l', -4, 36, G + 9, toe=-50), 'pelvis': (12, 0, -58, -3, 8, -22),
          'spine': (16, 0, -10), 'neck': (-14, 0, 32), 'head': (0, 0, 32)}),
    (22, {**_foot('l', -10, 54, toe=-40), **_foot('r', -8, -24, G + 2, toe=-88, knee_out=4),
          'pelvis': (18, 0, -52, -5, 14, -44), 'spine': (18, 0, -8), 'chest': (6, 0, -8),
          'head': (8, 0, 30), 'elbow_l': (-40, 60, 130), 'elbow_r': (-40, -10, 80),
          **_bat_held((-32, 18, 88), (0.10, 0.95, -0.30), (-0.70, 0.20, -0.70))}),
    (26, {'pelvis': (19, 0, -44, -5, 16, -48), 'spine': (18, 0, -4),
          **_bat_held((-34, 36, 74), (0.60, 0.20, 0.77), (-0.10, 0.95, -0.30))}),
    # Bat flat, well out in front of the front pad, face rolling towards square leg.
    (30, {'pelvis': (19, 0, -34, -5, 17, -50), 'spine': (18, 2, 4), 'chest': (6, 2, 2),
          'neck': (-16, 0, 20), 'head': (14, 0, 22), 'elbow_l': (-20, 80, 110), 'elbow_r': (-20, 30, 60),
          **_bat_at((-30, 90, 26), (0.10, -0.70, 0.70), (0.95, 0.20, -0.25))}),
    (34, {'pelvis': (18, 0, -22, -5, 17, -49), 'spine': (17, 0, 12), 'neck': (-14, 0, 10),
          **_bat_held((6, 52, 66), (-0.30, -0.95, 0.10), (0.30, -0.10, -0.95))}),
    (40, {'pelvis': (17, 0, -16, -5, 16, -47), 'spine': (16, 0, 16),
          **_bat_held((20, 34, 86), (-0.70, 0.50, -0.50), (0.10, -0.75, -0.65))}),
    (44, {**_bat_held((18, 32, 88), (-0.70, 0.50, -0.50), (0.10, -0.75, -0.65))}),
    (52, {'pelvis': (12, 0, -44, -3, 8, -24), 'spine': (16, 0, -6), 'chest': (6, 0, -10),
          'neck': (-12, 0, 30), 'head': (-4, 0, 32), **_foot('r', -8, -24, toe=-88),
          'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98)}),
    *_recover((-10, 54), -40, (-8, -24), -88, 72),
], contact=30, direction=-85, footwork='front', length=72, face_tolerance=45)

SLOG_SWEEP = _stroke('SLOGSWEEP', [
    *_trigger(hands=(-28, -6, 104)), _backlift(hands=(-24, -14, 132), shaft=(0.25, 0.35, -0.90)),
    (17, {**_foot('l', -6, 36, G + 9, toe=-50), 'pelvis': (12, 0, -60, -3, 7, -22),
          'spine': (16, 0, -10), 'neck': (-14, 0, 32), 'head': (0, 0, 32),
          **_bat_held((-24, -14, 138), (0.25, 0.35, -0.90), (-0.70, -0.45, -0.55))}),
    (22, {**_foot('l', -14, 52, toe=-45), **_foot('r', -8, -24, G + 2, toe=-88, knee_out=4),
          'pelvis': (18, 0, -56, -6, 13, -42), 'spine': (18, 0, -10), 'chest': (6, 0, -10),
          'head': (8, 0, 30), 'elbow_l': (-30, 30, 190), 'elbow_r': (-40, -40, 150),
          **_bat_held((-24, -6, 130), (0.60, 0.20, -0.77), (-0.70, 0.40, -0.60))}),
    (26, {'pelvis': (19, 0, -46, -6, 15, -46), 'spine': (18, 0, -2),
          **_bat_held((-42, 36, 100), (0.70, -0.30, 0.65), (-0.40, 0.70, 0.60))}),
    # Down on the back knee, big arc from high, face open and up to clear mid-wicket.
    (30, {'pelvis': (19, 0, -30, -6, 16, -48), 'spine': (18, 2, 8), 'chest': (6, 2, 6),
          'neck': (-16, 0, 14), 'head': (12, 0, 16), 'elbow_l': (0, 70, 130), 'elbow_r': (-20, 40, 80),
          **_bat_at((-24, 94, 34), (0.10, -0.75, 0.65), (0.85, 0.35, 0.40))}),
    (34, {'pelvis': (18, 0, -12, -5, 16, -47), 'spine': (17, 0, 18), 'neck': (-14, 0, 2),
          **_bat_held((14, 50, 98), (-0.40, -0.85, -0.35), (0.40, -0.50, 0.77))}),
    (42, {'pelvis': (15, 0, 0, -4, 14, -44), 'spine': (13, 0, 24), 'chest': (4, 0, 12),
          'neck': (-10, 0, -10), 'head': (-6, 0, -8), 'elbow_l': (30, 30, 190), 'elbow_r': (-10, 20, 170),
          **_bat_held((30, 34, 122), (-0.82, 0.40, -0.40), (0.20, -0.75, -0.63))}),
    (46, {**_bat_held((28, 32, 120), (-0.82, 0.40, -0.40), (0.20, -0.75, -0.63))}),
    (50, {'pelvis': (15, 0, -20, -4, 12, -38), 'spine': (15, 0, 10), 'chest': (5, 0, 2),
          'neck': (-12, 0, 10), 'head': (-4, 0, 12),
          }),
    (54, {'pelvis': (12, 0, -44, -3, 8, -24), 'spine': (16, 0, -6), 'chest': (6, 0, -10),
          'neck': (-12, 0, 30), 'head': (-4, 0, 32), **_foot('r', -8, -24, toe=-88),
          'elbow_l': (-34, 58, 118), 'elbow_r': (-8, -46, 98)}),
    *_recover((-14, 52), -45, (-8, -24), -88, 74),
], contact=30, direction=-60, footwork='front', length=74, face_tolerance=45)

# Clearing the front leg: the front foot opens to the leg side, the base stays wide and braced and
# the bat swings in a full arc over long-on. Shared by LOFTED STRAIGHT DRIVE and LEG-SIDE PICKUP.
LOFTED_STRAIGHT = _stroke('LOFTEDSTRAIGHT', [
    *_trigger(hands=(-28, -6, 108)),
    _backlift(hands=(-22, -18, 136), shaft=(0.20, 0.25, -0.95), pelvis=(6, 0, -64, -2, -8, -10)),
    (17, {**_foot('l', 8, 30, G + 10, toe=-30), 'pelvis': (10, 0, -56, 0, 4, -14),
          'spine': (15, 0, -10), 'neck': (-14, 0, 32), 'head': (-2, 0, 32),
          **_bat_held((-22, -18, 140), (0.20, 0.25, -0.95), (-0.70, -0.45, -0.55))}),
    (22, {**_foot('l', 14, 40, toe=-10), 'pelvis': (12, 0, -44, 3, 12, -18), 'spine': (16, 0, -6),
          'chest': (5, 0, -6), 'head': (4, 0, 28), 'elbow_l': (-20, 30, 200), 'elbow_r': (-40, -40, 160),
          **_bat_held((-24, -8, 134), (0.20, 0.85, -0.49), (-0.70, 0.20, -0.68))}),
    (26, {'pelvis': (13, 0, -30, 3, 15, -19), 'spine': (17, 0, 2),
          **_bat_held((-30, 26, 110), (0.15, 0.70, 0.70), (0.10, 0.70, -0.70))}),
    (30, {'pelvis': (12, 0, -14, 4, 16, -19), 'spine': (15, 2, 10), 'chest': (5, 2, 8),
          'neck': (-14, 0, 8), 'head': (6, 0, 10), 'elbow_l': (0, 80, 160), 'elbow_r': (-20, 30, 100),
          **_foot('r', -7, -25, toe=-70),
          **_bat_at((-18, 54, 36), (0.10, 0.05, 0.99), (0.35, 0.90, 0.25))}),
    (34, {'pelvis': (10, 0, 2, 4, 16, -17), 'spine': (10, 0, 18), 'head': (0, 0, 2),
          'elbow_l': (10, 90, 190), 'elbow_r': (-20, 60, 160),
          **_bat_held((0, 74, 130), (-0.20, -0.60, 0.77), (0.60, 0.60, 0.53))}),
    (44, {'pelvis': (6, 0, 18, 4, 14, -14), 'spine': (4, 0, 24), 'chest': (-2, 0, 14),
          'neck': (-8, 0, -16), 'head': (-8, 0, -14), 'elbow_l': (40, 40, 200), 'elbow_r': (0, 40, 200),
          **_foot('r', -6, -22, G + 6, toe=-40),
          **_bat_held((22, 32, 164), (-0.75, 0.35, -0.56), (0.35, -0.90, 0.25))}),
    (50, {'pelvis': (7, 0, 12, 4, 13, -14), **_foot('r', -6, -20, toe=-50),
          **_bat_held((20, 30, 158), (-0.75, 0.35, -0.56), (0.35, -0.90, 0.25))}),
    *_recover((14, 40), -10, (-6, -20), -50, 76),
], contact=30, direction=-20, footwork='front', length=76)

# Wristy deflection: short press, hands stay inside the line, the face rolls towards fine leg.
LEG_GLANCE = _stroke('LEGGLANCE', [
    *_trigger(), _backlift(hands=(-22, -8, 110), shaft=(0.15, 0.72, -0.68)),
    (17, {**_foot('l', 5, 28, G + 7, toe=-40), 'pelvis': (10, 0, -54, 1, 6, -13),
          'spine': (15, 0, -10), 'neck': (-12, 0, 32), 'head': (0, 0, 32)}),
    (22, {**_foot('l', 7, 40, toe=-30), 'pelvis': (14, 0, -46, 2, 16, -16),
          **_bat_held((-26, 8, 104), (0.25, 0.90, -0.36), (-0.30, 0.40, -0.86))}),
    (26, {'pelvis': (15, 0, -40, 2, 20, -17), 'spine': (18, 3, -2),
          **_bat_held((-26, 44, 94), (0.12, 0.45, 0.88), (0.10, 0.89, -0.45))}),
    (30, {'pelvis': (15, 0, -32, 2, 22, -18), 'spine': (19, 4, 2), 'chest': (7, 6, 2),
          'neck': (-14, 0, 18), 'head': (8, 0, 18), 'elbow_l': (-10, 70, 130), 'elbow_r': (-10, 20, 80),
          **_bat_at((4, 64, 26), (0.20, 0.20, 0.96), (0.87, -0.48, 0.0))}),
    (35, {'pelvis': (15, 0, -28, 2, 22, -18), 'spine': (18, 2, 6),
          **_bat_held((12, 70, 86), (0.45, 0.20, 0.87), (0.80, -0.55, -0.20))}),
    (44, {'pelvis': (14, 0, -34, 2, 20, -17), 'spine': (18, 2, 0),
          **_bat_held((12, 68, 90), (0.45, 0.20, 0.87), (0.80, -0.55, -0.20))}),
    *_recover((7, 40), -30, (-7, -25), -88, 68, over=False),
], contact=30, direction=-120, footwork='front', length=68, face_tolerance=45)

SHOTS = [STRAIGHT_DRIVE, COVER_DRIVE, ON_DRIVE, FORWARD_DEFENCE, LOFTED_DRIVE,
         BACK_FOOT_DEFENCE, BACK_FOOT_PUNCH, SQUARE_CUT, PULL, HOOK,
         SWEEP, SLOG_SWEEP, LOFTED_STRAIGHT, LEG_GLANCE]


# ================================================================= bowling

def _delivery(name, approach, gather, brace, coil, release, follow, recover,
              release_frame=28, length=54):
    return {'name': name, 'event': 'BallRelease', 'contact': release_frame,
            'keys': [(0, approach), (10, gather), (18, brace), (24, coil), (release_frame, release),
                     (36, follow), (46, recover), (length, approach)]}


def _pace_release(grip_style, wrist, extend=0.99, arm=(.16, .34, .93)):
    """Release pose for a right-arm pace bowler. The bowling arm is vertical and fully extended,
    the leading arm has been pulled down hard into the left side, the front leg is braced straight
    and the head is still. Only the wrist and grip differ between seam positions - the body action
    of an outswinger and an inswinger is the same, and faking otherwise would be worse than honest.
    """
    return {
        'pelvis': (10, 0, 34, 0, 12, -10), 'spine': (16, -8, 26), 'chest': (10, -10, 18),
        'neck': (-14, 0, -20), 'head': (-8, 0, -16),
        'foot_l': (4, 38, GROUND), 'foot_r': (-14, -30, GROUND + 22),
        'knee_l': (4, 150, 46), 'knee_r': (-20, 40, 40),
        'hand_r': R(extend, *arm), 'elbow_r': (-34, -46, 176),
        'hand_l': R(0.96, 0.05, -0.19, -0.98), 'elbow_l': (44, -18, 118),
        'wrist_r': wrist, 'grip_r': grip_style, 'grip_l': 'open',
    }


_PACE_APPROACH = {
    'pelvis': (14, 0, 8, 0, 0, -6), 'spine': (14, 0, -6), 'neck': (-14, 0, 0),
    'foot_l': (9, 58, GROUND + 2), 'foot_r': (-9, -48, GROUND + 8),
    'hand_r': R(0.75, 0.07, 0.59, -0.80), 'hand_l': R(0.74, -0.03, 0.70, -0.72),
    'elbow_r': (-44, -30, 104), 'elbow_l': (44, -30, 108),
    'grip_r': 'seam', 'grip_l': 'open',
}
# The bound: both feet leave the ground, the body turns side-on and the leading arm starts to rise.
_PACE_GATHER = {
    'pelvis': (6, 0, 46, 0, 4, 6), 'spine': (8, 0, 30), 'chest': (4, 0, 22),
    'neck': (-10, 0, -32), 'head': (-4, 0, -22),
    'foot_l': (10, 22, GROUND + 30), 'foot_r': (-12, -34, GROUND + 20),
    'knee_l': (12, 90, 70), 'knee_r': (-16, 40, 44),
    'hand_r': R(0.95, -0.02, -0.34, -0.94), 'hand_l': R(0.39, -0.24, 0.94, 0.23),
    'elbow_r': (-46, -44, 112), 'elbow_l': (40, -8, 140),
    'grip_r': 'seam', 'grip_l': 'open',
}
# Back foot lands parallel to the crease, fully side-on, leading arm at its highest.
_PACE_BRACE = {
    'pelvis': (4, 0, 72, 0, 6, -4), 'spine': (6, -4, 44), 'chest': (2, -6, 30),
    'neck': (-10, 0, -54), 'head': (-4, 0, -32),
    'foot_l': (8, 20, GROUND + 26), 'foot_r': (-16, -26, GROUND),
    'knee_l': (10, 84, 64), 'knee_r': (-22, 30, 40),
    'hand_r': R(0.96, -0.10, -0.46, -0.88), 'hand_l': R(0.63, -0.26, 0.36, 0.90),
    'elbow_r': (-48, -52, 108), 'elbow_l': (30, 2, 156),
    'grip_r': 'seam', 'grip_l': 'open',
}
# Delivery stride: front foot slams down, hips start to open, bowling arm at the bottom of the circle.
_PACE_COIL = {
    'pelvis': (8, 0, 58, 0, 10, -12), 'spine': (10, -8, 40), 'chest': (6, -10, 28),
    'neck': (-12, 0, -42), 'head': (-6, 0, -28),
    'foot_l': (4, 38, GROUND), 'foot_r': (-16, -28, GROUND + 6),
    'knee_l': (4, 140, 44), 'knee_r': (-20, 30, 42),
    'hand_r': R(0.61, -0.33, -0.83, -0.45), 'hand_l': R(0.24, -0.23, 0.02, 0.97),
    'elbow_r': (-50, -50, 140), 'elbow_l': (36, -6, 150),
    'grip_r': 'seam', 'grip_l': 'open',
}
# Follow-through: the arm continues down across the body, the torso flexes over the braced front leg.
_PACE_FOLLOW = {
    'pelvis': (26, 0, 16, 0, 16, -16), 'spine': (32, -6, 6), 'chest': (18, -6, 2),
    'neck': (-26, 0, -6), 'head': (-14, 0, -6),
    'foot_l': (4, 38, GROUND), 'foot_r': (-10, -6, GROUND + 34),
    'knee_l': (4, 150, 42), 'knee_r': (-14, 70, 60),
    'hand_r': R(0.96, 0.46, 0.09, -0.88), 'hand_l': R(0.89, 0.14, -0.52, -0.84),
    'elbow_r': (-18, -18, 120), 'elbow_l': (46, -26, 122),
    'grip_r': 'open', 'grip_l': 'open',
}
_PACE_RECOVER = {
    'pelvis': (16, 0, 4, 0, 8, -10), 'spine': (18, 0, 0), 'neck': (-16, 0, 0),
    'foot_l': (7, 20, GROUND), 'foot_r': (-9, -22, GROUND + 12),
    'hand_r': R(0.87, 0.02, 0.34, -0.94), 'hand_l': R(0.82, 0.02, 0.41, -0.91),
    'elbow_r': (-44, -30, 100), 'elbow_l': (44, -30, 102),
    'grip_r': 'open', 'grip_l': 'open',
}

FAST_BOWL = _delivery('FastBowl', _PACE_APPROACH, _PACE_GATHER, _PACE_BRACE, _PACE_COIL,
                      _pace_release('seam', (0, 0, 0)), _PACE_FOLLOW, _PACE_RECOVER)
# Swing: identical body action, seam presented differently by the wrist. Ball physics do the rest.
OUTSWING = _delivery('FastBowlOutswing', _PACE_APPROACH, _PACE_GATHER, _PACE_BRACE, _PACE_COIL,
                     _pace_release('seam_out', (0, -22, -12)), _PACE_FOLLOW, _PACE_RECOVER)
INSWING = _delivery('FastBowlInswing', _PACE_APPROACH, _PACE_GATHER, _PACE_BRACE, _PACE_COIL,
                    _pace_release('seam_in', (0, 24, 14)), _PACE_FOLLOW, _PACE_RECOVER)
# Cutter: the fingers drag down one side at release, so the wrist turns late and the arm slows.
CUTTER = _delivery('FastBowlCutter', _PACE_APPROACH, _PACE_GATHER, _PACE_BRACE, _PACE_COIL,
                   _pace_release('cutter', (0, -34, -20), extend=.96, arm=(.18, .40, .90)), _PACE_FOLLOW, _PACE_RECOVER)


def _spin_release(grip_style, wrist, extend, arm, chest_on):
    """Spinners deliver from a lower, more chest-on position than a quick, and the shoulders keep
    rotating through the release rather than bracing against it."""
    return {
        'pelvis': (10, 0, 34 - chest_on, 0, 8, -12), 'spine': (14, -6, 22 - chest_on),
        'chest': (8, -8, 14 - chest_on),
        'neck': (-14, 0, -14), 'head': (-8, 0, -10),
        'foot_l': (5, 30, GROUND), 'foot_r': (-13, -24, GROUND + 16),
        'knee_l': (5, 140, 46), 'knee_r': (-18, 40, 44),
        'hand_r': R(extend, *arm), 'elbow_r': (-38, -40, 168),
        'hand_l': R(0.96, 0.09, -0.14, -0.99), 'elbow_l': (46, -16, 120),
        'wrist_r': wrist, 'grip_r': grip_style, 'grip_l': 'open',
    }


def _spin_set(prefix, grip_style, wrist, extend, arm, chest_on):
    approach = dict(_PACE_APPROACH, **{
        'foot_l': (9, 34, GROUND + 2), 'foot_r': (-9, -28, GROUND + 6),   # short, bouncy approach
        'pelvis': (10, 0, 6, 0, 0, -5), 'grip_r': grip_style,
    })
    gather = dict(_PACE_GATHER, **{
        'foot_l': (10, 16, GROUND + 20), 'foot_r': (-11, -22, GROUND + 12),
        'pelvis': (6, 0, 34, 0, 3, 4), 'spine': (8, 0, 22), 'hand_l': R(0.35, -0.26, 0.95, -0.16),
        'grip_r': grip_style,
    })
    brace = dict(_PACE_BRACE, **{
        'pelvis': (4, 0, 52, 0, 4, -4), 'spine': (6, -4, 32),
        'foot_l': (8, 16, GROUND + 18), 'foot_r': (-14, -20, GROUND),
        'hand_l': R(0.45, -0.33, 0.42, 0.85), 'grip_r': grip_style,
    })
    coil = dict(_PACE_COIL, **{
        'pelvis': (8, 0, 44, 0, 7, -12), 'spine': (10, -6, 30),
        'foot_l': (5, 30, GROUND), 'foot_r': (-14, -22, GROUND + 5),
        'hand_r': R(0.64, -0.20, -0.61, -0.77), 'grip_r': grip_style,
    })
    follow = dict(_PACE_FOLLOW, **{
        'pelvis': (22, 0, 8, 0, 12, -14), 'spine': (26, -4, 2),
        'foot_l': (5, 30, GROUND), 'foot_r': (-8, -2, GROUND + 26),
        'hand_r': R(0.96, 0.47, 0.07, -0.88),
    })
    recover = dict(_PACE_RECOVER, **{'foot_l': (7, 16, GROUND), 'foot_r': (-9, -16, GROUND + 8)})
    return _delivery(prefix, approach, gather, brace, coil,
                     _spin_release(grip_style, wrist, extend, arm, chest_on),
                     follow, recover, release_frame=28, length=52)


# Finger spin: arm comes over at about shoulder-plus, wrist snaps across the seam left-to-right.
OFF_SPIN = _spin_set('OffSpin', 'offspin', (0, -40, -26), .97, (.18, .46, .87), 12)
DOOSRA = _spin_set('OffSpinDoosra', 'doosra', (0, 46, 30), .96, (.20, .50, .84), 16)
# Wrist spin. These four are the reason the grip table exists: same approach and body action, four
# genuinely different wrists and hands at the moment of release.
LEG_BREAK = _spin_set('LegSpin', 'legbreak', (0, 58, -34), .98, (.16, .40, .90), 6)
GOOGLY = _spin_set('LegSpinGoogly', 'googly', (0, 112, -46), .97, (.14, .44, .89), 10)
TOP_SPINNER = _spin_set('LegSpinTopSpinner', 'topspin', (0, 84, -8), .99, (.14, .30, .94), 4)
FLIPPER = _spin_set('LegSpinFlipper', 'flipper', (0, 18, -62), .95, (.20, .54, .82), 2)

DELIVERIES = [FAST_BOWL, OUTSWING, INSWING, CUTTER, OFF_SPIN, DOOSRA,
              LEG_BREAK, GOOGLY, TOP_SPINNER, FLIPPER]


# ================================================================= fielding

PICKUP = {
    'name': 'Pickup', 'event': 'Pickup', 'contact': 20,
    'keys': [
        (0, RUN[0][1]),
        (8, dict(FIELDER_READY, **{'pelvis': (34, 0, 0, 0, 4, -26), 'spine': (34, 0, 0),
                                   'neck': (-34, 0, 0),
                                   'foot_l': (16, 18, GROUND), 'foot_r': (-16, -16, GROUND),
                                   'hand_l': R(.86, .22, .62, -.75), 'hand_r': R(.88, -.16, .64, -.75)})),
        # Both hands go down beside the ball, weight over a bent front knee - not a bend at the waist.
        (20, {'pelvis': (48, 0, -6, 0, 8, -38), 'spine': (40, 0, 0), 'chest': (12, 0, 0),
              'neck': (-42, 0, 0), 'head': (-16, 0, 0),
              'foot_l': (15, 26, GROUND), 'foot_r': (-17, -20, GROUND + 4),
              'knee_l': (15, 130, 40), 'knee_r': (-20, 60, 44),
              'hand_l': R(.94, .10, .52, -.85), 'hand_r': R(.94, -.10, .52, -.85),
              'elbow_l': (40, 24, 60), 'elbow_r': (-40, 24, 60),
              'grip_l': 'flat', 'grip_r': 'ball'}),
        (30, {'pelvis': (30, 0, -4, 0, 6, -26), 'spine': (28, 0, 4), 'neck': (-28, 0, 0),
              'foot_l': (15, 24, GROUND), 'foot_r': (-17, -18, GROUND),
              'hand_l': R(.80, .20, .68, -.70), 'hand_r': R(.82, -.16, .66, -.72),
              'grip_l': 'open', 'grip_r': 'ball'}),
        (42, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'})),
    ],
}

THROW = {
    'name': 'Throw', 'event': 'ThrowRelease', 'contact': 26,
    'keys': [
        (0, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'})),
        # Gather and transfer weight onto the back foot, front arm points at the target.
        (10, {'pelvis': (4, 0, 40, 0, -4, -12), 'spine': (6, -4, 28), 'chest': (2, -6, 20),
              'neck': (-10, 0, -30), 'head': (-4, 0, -20),
              'foot_l': (10, 16, GROUND + 10), 'foot_r': (-14, -20, GROUND),
              'knee_l': (12, 80, 60), 'knee_r': (-18, 40, 42),
              'hand_r': R(0.57, -0.35, -0.76, -0.55), 'hand_l': R(0.60, -0.15, 0.98, -0.15),
              'elbow_r': (-52, -46, 130), 'elbow_l': (36, 10, 130),
              'grip_r': 'ball', 'grip_l': 'flat'}),
        # Arm cocked, elbow leads, front foot plants.
        (18, {'pelvis': (6, 0, 56, 0, 4, -14), 'spine': (8, -6, 38), 'chest': (4, -8, 26),
              'neck': (-10, 0, -38), 'head': (-4, 0, -24),
              'foot_l': (6, 32, GROUND), 'foot_r': (-14, -22, GROUND + 3),
              'knee_l': (6, 130, 46), 'knee_r': (-18, 34, 42),
              'hand_r': R(0.65, -0.42, -0.78, 0.47), 'hand_l': R(0.48, -0.04, 1.00, 0.03),
              'elbow_r': (-54, -44, 150), 'elbow_l': (40, 4, 134),
              'grip_r': 'ball', 'grip_l': 'flat'}),
        (26, {'pelvis': (10, 0, 20, 0, 10, -12), 'spine': (14, -6, 14), 'chest': (8, -8, 8),
              'neck': (-14, 0, -12), 'head': (-8, 0, -8),
              'foot_l': (6, 32, GROUND), 'foot_r': (-14, -26, GROUND + 14),
              'knee_l': (6, 145, 44), 'knee_r': (-18, 40, 44),
              'hand_r': R(0.96, 0.12, 0.56, 0.82), 'hand_l': R(0.83, 0.11, -0.08, -0.99),
              'elbow_r': (-38, -34, 172), 'elbow_l': (46, -14, 122),
              'grip_r': 'open', 'grip_l': 'open'}),
        (36, dict(_PACE_FOLLOW, **{'grip_r': 'open'})),
        (48, FIELDER_READY),
    ],
}

CATCH = {
    'name': 'Catch', 'event': 'Catch', 'contact': 18,
    'keys': [
        (0, FIELDER_READY),
        (8, dict(FIELDER_READY, **{'pelvis': (4, 0, 0, 0, 2, -8), 'spine': (2, 0, 0),
                                   'neck': (-20, 0, 0), 'head': (-10, 0, 0),
                                   'hand_l': R(.62, .28, .82, -.50), 'hand_r': R(.62, -.28, .82, -.50),
                                   'elbow_l': (44, -6, 116), 'elbow_r': (-44, -6, 116),
                                   'grip_l': 'open', 'grip_r': 'open'})),
        # Hands meet in front of the eyes, fingers up, elbows soft.
        (18, {'pelvis': (6, 0, 0, 0, 4, -6), 'spine': (4, 0, 0), 'neck': (-22, 0, 0),
              'head': (-12, 0, 0),
              'foot_l': (17, 6, GROUND), 'foot_r': (-17, -4, GROUND),
              'hand_l': R(.56, .14, .92, -.36), 'hand_r': R(.56, -.14, .92, -.36),
              'elbow_l': (42, 0, 118), 'elbow_r': (-42, 0, 118),
              'grip_l': 'ball', 'grip_r': 'ball'}),
        # Give with the ball - the hands ride back into the chest rather than stopping dead.
        (26, {'pelvis': (10, 0, 0, 0, 0, -12), 'spine': (10, 0, 0), 'neck': (-18, 0, 0),
              'foot_l': (17, 4, GROUND), 'foot_r': (-17, -2, GROUND),
              'hand_l': R(.34, .16, .82, -.55), 'hand_r': R(.34, -.16, .82, -.55),
              'grip_l': 'ball', 'grip_r': 'ball'}),
        (40, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'})),
    ],
}

KEEPER_RECEIVE = {
    'name': 'KeeperReceive', 'event': 'Catch', 'contact': 16,
    'keys': [
        (0, KEEPER_READY),
        (8, KEEPER_RISE),
        (16, dict(KEEPER_RISE, **{'pelvis': (16, 0, 0, 0, 4, -26), 'neck': (-22, 0, 0),
                                  'hand_l': R(.84, .12, .74, -.66), 'hand_r': R(.84, -.12, .74, -.66),
                                  'elbow_l': (44, 6, 68), 'elbow_r': (-44, 6, 68),
                                  'grip_l': 'keeper', 'grip_r': 'keeper'})),
        (24, dict(KEEPER_RISE, **{'pelvis': (18, 0, 0, 0, 0, -30),
                                  'hand_l': R(.66, .14, .62, -.77), 'hand_r': R(.66, -.14, .62, -.77),
                                  'grip_l': 'keeper', 'grip_r': 'keeper'})),
        (36, KEEPER_READY),
    ],
}


# ================================================================= round 5: bowling, fielding, keeping
#
# Same architecture as the Round 4 batting library: technique authored as world measurements,
# cumulative beats, dense per-frame solve, and one right-handed source per action whose left-handed
# twin is the FULL sagittal mirror (root, feet, hips, torso, shoulders, arm, wrist, release).
#
# Orientation convention (verified on the batter in Round 4): +rz turns the body LEFT. A right-arm
# bowler or thrower is side-on with the LEFT shoulder pointing at the target, i.e. pelvis/chest
# rz around -80..-90. The legacy clips above turned +rz, which points the bowling shoulder at the
# batter - the Round 4 handedness bug in bowling form. action_lab.py rejects that.
#
# Bowling clips are authored in WORLD space along the run-up and the match's own root travel
# (C26MatchGameMode RunUp curve and post-release drift) is subtracted at bake, so planted feet stay
# planted when the game adds the root motion back.

_RELEASE = 19            # 0.633s: the match starts the action 0.62s before release, so ~1:1 playback


def _run_travel(start_y, post_speed=330.0):
    """Root travel (x, y) of the bowler actor since the clip began, in clip frames.

    Mirrors C26MatchGameMode: y = lerp(start, -995, T^2 (2-T)) over the 3.25s run-up, the action
    begins 0.62s before release, then the root drifts on at post_speed decaying to zero over 0.63s."""
    length = -995.0 - start_y
    t0 = (3.25 - 0.62) / 3.25

    def trav(t):
        return t * t * (2.0 - t)

    def fn(frame):
        a = frame * 0.62 / _RELEASE if frame <= _RELEASE else 0.62 + (frame - _RELEASE) / 30.0
        if a <= 0.62:
            return (0.0, length * (trav(t0 + a / 3.25) - trav(t0)))
        t = min(a - 0.62, 0.63)
        return (0.0, length * (1.0 - trav(t0)) + post_speed * (t - t * t / (2 * 0.63)))
    return fn


# C26MatchGameMode runs EVERY bowler from -2700 to -995 with the same 330 cm/s drift after release,
# so every action shares one root travel. Pace and spin differ in the body, not in the root.
RUN_START = -2700.0


def _world(pose, dy):
    """A pose placed dy down the pitch in world space (feet, poles, hands and pelvis travel)."""
    return shift_pose(pose, -dy)


def shift_pose(pose, ty):
    out = {}
    for key, value in pose.items():
        if key == 'pelvis':
            v = tuple(value) + (0, 0, 0)[len(value) - 3:] if len(value) < 6 else value
            out[key] = (v[0], v[1], v[2], v[3], v[4] - ty, v[5])
        elif (key.startswith(('foot_', 'knee_', 'elbow_', 'hand_')) and isinstance(value, tuple)
              and len(value) == 3 and all(isinstance(c, (int, float)) for c in value)):
            out[key] = (value[0], value[1] - ty, value[2])
        else:
            out[key] = value
    return out


def _action(name, beats, event, event_frame, length, start, end, travel=None, **meta):
    keys, pose = [(0, start)], dict(start)
    for frame, delta in beats:
        pose = dict(pose, **delta)
        keys.append((frame, pose))
    keys.append((length, end))
    return dict(meta, name=name, event=event, contact=event_frame, keys=keys, dense=True, travel=travel)


_BALL_R = {'grip_r': 'seam', 'grip_l': 'open'}

# ---------------------------------------------------------------- bowling (right arm)


def _pace(name, run, side_on, bound, stride, upright, arm_back, follow, travel):
    """A pace action. The body parameters are what separate a fast bowler from a fast-medium one:
    how side-on the back-foot contact is, how high the bound goes, how long the delivery stride is,
    how braced and upright the release is, and how far the follow-through carries."""
    bfc = 228.0 * run
    ffc = bfc + stride
    rel = ffc + 2.0
    end_y = travel(52)[1]
    # Frame 0 is the last run-up stride: left foot down under the body, ball held at the chest.
    return _action(name, [
        # Take-off into the bound: right knee drives, both arms lift, the hips start to close.
        (4, {'pelvis': (16, 0, -25, 0, 86 * run, 2), 'spine': (12, 0, -8), 'neck': (-12, 0, 20),
             'head': (-6, 0, 12),
             **_foot('l', 6, 18, G + 4, toe=-10), **_foot('r', -10, 118 * run, G + 32, toe=-30, knee_out=90),
             'hand_r': R(.70, -.25, .55, .80), 'hand_l': R(.75, .20, .60, .75)}),
        (8, {'pelvis': (8, 0, -side_on * .65, 0, 172 * run, 2 + bound * .5), 'spine': (6, 0, -12),
             'chest': (2, 0, -10), 'neck': (-8, 0, 36), 'head': (-6, 0, 28),
             **_foot('l', 4, 112 * run, G + bound, toe=-40), **_foot('r', -12, bfc - 16, G + bound * .6, toe=-70),
             'hand_l': R(.90, .05, .55, .83), 'hand_r': R(.70, -.35, -.20, .90)}),
        # Back-foot contact: right foot parallel to the crease, left shoulder and front arm point at
        # the batter, ball hand cocked behind the head, head turned back to look over the front arm.
        (11, {'pelvis': (6, -6, -side_on, 0, bfc, -6), 'spine': (4, -6, -10), 'chest': (2, -4, -8),
              'neck': (-6, 0, 30 + side_on * .25), 'head': (-4, 0, 22 + side_on * .25),
              **_foot('r', -14, bfc, toe=-88), **_foot('l', -4, bfc + stride * .38, G + 30, toe=-60, knee_out=110),
              'hand_l': R(.95, .02, .62, .78), 'hand_r': R(.80, -.30, -.55, .78)}),
        # Front knee up, hips begin to open while the shoulders stay closed (hip-shoulder separation).
        (14, {'pelvis': (8, -4, -side_on * .78, 0, bfc + stride * .5, -8), 'spine': (6, -4, -18),
              'chest': (4, -2, -16), 'neck': (-8, 0, 28 + side_on * .3), 'head': (-6, 0, 22 + side_on * .25),
              **_foot('l', -6, ffc - 16, G + 10, toe=-40), **_foot('r', -14, bfc, G + 1, toe=-86),
              'hand_l': R(.90, .10, .75, .60), 'hand_r': R(.95, -.15, -.70, -.70)}),
        # Front-foot contact: long braced stride, front arm pulling down, bowling arm low behind.
        (16, {'pelvis': (10, -2, -side_on * .5, -2, ffc - stride * .36, -8), 'spine': (4, 0, -22),
              'chest': (2, 0, -18), 'neck': (-8, 0, 26 + side_on * .2), 'head': (-6, 0, 20 + side_on * .2),
              **_foot('l', -6, ffc, toe=-35), **_foot('r', -14, bfc + 16, G + 9, toe=-80),
              'hand_l': R(.85, .35, .45, .30), 'hand_r': R(.97, -.20, -.55, -.80)}),
        (18, {'pelvis': (12, 0, -side_on * .25, -2, ffc - stride * .2, -7), 'spine': (8, 0, -8),
              **_foot('r', -15, bfc + stride * .45, G + 16, toe=-74),
              'chest': (6, 0, 0), 'neck': (-10, 0, 14), 'head': (-8, 0, 12),
              'hand_r': R(.97, -.30, -.25 + arm_back, .92), 'hand_l': R(.82, .42, .05, -.60)}),
        # Release: braced front leg, chest has rotated through, bowling arm vertical over the
        # right shoulder and just ahead of it, front arm tucked into the left hip, head still.
        (_RELEASE, {'pelvis': (14, 0, -10, -2, rel, -8), 'spine': (upright, 2, 4), 'chest': (8, 4, 10),
                    'neck': (-upright, 0, 2), 'head': (-8, 0, 2),
                    **_foot('r', -16, bfc + stride * .72, G + 24, toe=-60),
                    'hand_r': R(.99, -.10, .30 - arm_back, .95), 'hand_l': R(.80, .45, -.10, -.88)}),
        (22, {'pelvis': (24, 0, 12, -2, rel + 26 * follow, -8), 'spine': (26, 2, 14), 'chest': (14, 0, 10),
              'neck': (-24, 0, -10), 'head': (-12, 0, -8),
              **_foot('r', -10, rel - 36, G + 26, toe=-30, knee_out=110),
              'hand_r': R(.95, .45, .70, -.20), 'hand_l': R(.85, .30, -.60, -.70), 'grip_r': 'open'}),
        # Follow-through strides carry the momentum on past the crease.
        (26, {'pelvis': (22, 0, 18, 0, rel + 80 * follow, -10), 'spine': (22, 0, 8),
              **_foot('r', -8, rel + 96 * follow, toe=-10), **_foot('l', -4, ffc + 30 * follow, G + 16, toe=-20),
              'hand_r': R(.90, .55, .40, -.70), 'hand_l': R(.80, .30, -.50, -.80)}),
        (31, {'pelvis': (16, 0, 8, 0, rel + 122 * follow, -7), 'spine': (16, 0, 2), 'neck': (-16, 0, -2),
              'head': (-8, 0, -2),
              **_foot('l', 8, rel + 150 * follow, toe=0), **_foot('r', -8, rel + 108 * follow, G + 14, toe=-6),
              'hand_r': R(.75, .10, .60, -.78), 'hand_l': R(.75, .05, .55, -.83)}),
        (35, {**_foot('r', -9, end_y - 16, G + 7, toe=-3)}),
        (38, {'pelvis': (10, 0, 2, 0, end_y - 4, -6), 'spine': (10, 0, 0), 'neck': (-10, 0, 0), 'head': (-6, 0, 0),
              **_foot('r', -10, end_y - 6, toe=0)}),
        (41, {**_foot('l', 10, (rel + 150 * follow + end_y + 4) * .5, G + min(14.0, abs(end_y + 4 - rel - 150 * follow) * .3), toe=0)}),
        (45, {**_foot('l', 11, end_y + 4, toe=0)}),
    ], 'BallRelease', _RELEASE, 52, dict(RUN_STRIDE_BALL), _world(BOWLER_READY, end_y), travel,
        bowling_arm='r', bfc_frame=11, ffc_frame=16, kind='pace')


RUN_STRIDE_BALL = {
    'pelvis': (14, 0, -10, 0, 0, -5), 'spine': (12, 0, -6), 'neck': (-12, 0, 8), 'head': (-6, 0, 6),
    **_foot('l', 6, 14, toe=-10), **_foot('r', -8, -40, G + 22, toe=-15, knee_out=100),
    'hand_r': R(.55, -.20, .75, -.20), 'hand_l': R(.60, .10, .85, -.10),
    'elbow_r': (-60, -20, 110), 'elbow_l': (60, -20, 110), **_BALL_R,
}

_RUN_TRAVEL = _run_travel(RUN_START)

FAST_BOWL_R5 = _pace('FastBowl', 1.0, side_on=84, bound=28, stride=104, upright=16, arm_back=0.0,
                     follow=1.0, travel=_RUN_TRAVEL)
# Fast-medium: a semi-open action, lower bound, shorter stride, taller release, shorter carry.
FAST_MEDIUM_R5 = _pace('FastMedium', 1.0, side_on=58, bound=16,
                       stride=86, upright=9, arm_back=0.08, follow=0.72, travel=_RUN_TRAVEL)


def _spin(name, grip, wrist, side_on, arm, front_arm, chest_through, pivot, hop, stride, drift, crouch, bend,
          travel):
    """Spin: a bouncy approach, a gather hop, a shorter stride, and the body rotates THROUGH the
    release over a pivoting front foot instead of bracing against it. The fingers or wrist, not the
    run-up, impart the revolutions.

    The match drives every bowler's root down the same run-up curve, so the footfalls here are placed
    on that travel (see _run_travel) and the body never lags the root by more than a short stride.
    hop    - gather jump height          stride - back-foot to front-foot distance
    drift  - approach angle: lateral offset that closes to the stumps by front-foot contact
    crouch - how far the hips sit into the front leg     bend - forward trunk flexion in the finish
    """
    bfc = 228.0
    ffc = bfc + stride
    rel = ffc + 10.0
    end_y = travel(46)[1]
    start = dict(RUN_STRIDE_BALL, **{'grip_r': grip, 'pelvis': (10, 0, -6, drift, 0, -4),
                                     **_foot('l', 6 + drift, 14, toe=-10),
                                     **_foot('r', -8 + drift, -30, G + 16, toe=-15, knee_out=90)})
    return _action(name, [
        # Push off the planted left foot while the right knee drives through.
        (2, {'pelvis': (10, 0, -8, drift, 42, -3), **_foot('r', -8 + drift, 40, G + 22, toe=-20, knee_out=90)}),
        (4, {**_foot('l', 6 + drift * .9, 50, G + 10, toe=-20)}),
        # Gather hop: both feet off the turf, hands rise together to the chin, turning side-on in the air.
        (6, {'pelvis': (6, 0, -side_on * .5, drift * .8, 128, -4 + hop * .5), 'spine': (6, 0, -8),
             'neck': (-10, 0, 16 + side_on * .2), 'head': (-6, 0, 10 + side_on * .15),
             **_foot('l', 4 + drift * .7, 120, G + hop * .6, toe=-30),
             **_foot('r', -10 + drift * .6, 170, G + hop, toe=-50, knee_out=90),
             'hand_r': R(.62, -.20, .55, .80), 'hand_l': R(.70, .15, .60, .78)}),
        # Back-foot contact: right foot lands across the crease, front shoulder and arm at the batter.
        (10, {'pelvis': (6, -2, -side_on, drift * .4, bfc - 4, -6 - crouch * .3), 'spine': (4, 0, -12),
              'chest': (2, 0, -10), 'neck': (-6, 0, 24 + side_on * .3), 'head': (-4, 0, 18 + side_on * .25),
              **_foot('r', -10 + drift * .4, bfc, toe=-72),
              **_foot('l', -2 + drift * .3, bfc + 40, G + 20, toe=-40, knee_out=100),
              'hand_l': R(.90, .10, .60, .78), 'hand_r': R(.60, -.30, .10, .75)}),
        (13, {'pelvis': (7, -1, -side_on * .7, drift * .2, bfc + stride * .45, -8 - crouch * .6),
              **_foot('l', -3 + drift * .1, ffc - 18, G + 10, toe=-25), 'hand_l': front_arm}),
        # Front-foot contact: the front arm pulls down as the hips start to turn over the front leg.
        (15, {'pelvis': (8, 0, -side_on * .3, 0, ffc - 22, -10 - crouch), 'spine': (6, 0, -18),
              'chest': (4, 0, -16), 'neck': (-8, 0, 20 + side_on * .25), 'head': (-6, 0, 16 + side_on * .2),
              **_foot('l', -4, ffc, toe=-20), **_foot('r', -10 + drift * .4, bfc + 14, G + 8, toe=-70),
              'hand_l': R(.85, .35, .50, .30), 'hand_r': R(.90, -.25, -.35, .85)}),
        # Release: over the front foot, chest rotated through, the wrist and fingers doing the work.
        (_RELEASE, {'pelvis': (12, 0, chest_through * .4, 0, rel, -9 - crouch),
                    'spine': (12 + bend * .3, 0, chest_through * .6), 'chest': (8, 0, chest_through * .5),
                    'neck': (-12, 0, -chest_through * .7), 'head': (-8, 0, -chest_through * .5),
                    **_foot('r', -12, bfc + stride * .62, G + 20, toe=-60),
                    'hand_r': arm, 'hand_l': R(.80, .40, -.15, -.85), 'wrist_r': wrist}),
        # The pivot: the right leg swings round in front of the body and lands across the line.
        (24, {'pelvis': (18 + bend * .5, 0, chest_through + 25, 0, rel + 36, -6 - crouch * .5),
              'spine': (18 + bend, 0, 24), 'neck': (-18 - bend * .6, 0, -30), 'head': (-10, 0, -24),
              **_foot('r', 4 + pivot, ffc + 48, G + 14, toe=30, knee_out=90),
              'hand_r': R(.90, .60, .30, -.72), 'hand_l': R(.80, .35, -.40, -.84), 'grip_r': 'open',
              'wrist_r': (0, 0, 0)}),
        (28, {'pelvis': (14, 0, chest_through + 20, 0, rel + 66, -5), **_foot('r', 8 + pivot, ffc + 62, toe=40),
              **_foot('l', -4, ffc, toe=-20)}),
        (31, {'pelvis': (10, 0, 26, 0, rel + 90, -6), **_foot('l', 6, ffc + 100, G + 14, toe=10)}),
        (34, {'pelvis': (8, 0, 20, 0, end_y - 44, -5), 'spine': (8, 0, 6), 'neck': (-8, 0, -8), 'head': (-6, 0, -6),
              **_foot('l', 10, end_y - 40, toe=20)}),
        (35, {**_foot('r', -2, (ffc + 62 + end_y - 5) * .5, G + 16, toe=20)}),
        (40, {**_foot('r', -11, end_y - 5, toe=0), 'pelvis': (6, 0, 0, 0, end_y, -5), 'spine': (7, 0, 0),
              'neck': (-8, 0, 0), 'head': (-4, 0, 0)}),
        (43, {**_foot('l', 11, end_y - 16, G + 9, toe=10)}),
    ], 'BallRelease', _RELEASE, 46, start, _world(BOWLER_READY, end_y), travel,
        bowling_arm='r', bfc_frame=10, ffc_frame=15, kind='spin')


# Finger spin: short hop, straight approach, chest-on through the crease, a high arm just off
# vertical and the index finger ripping across the seam; a compact pivot.
OFF_SPIN_R5 = _spin('OffSpin', 'offspin', (0, -40, -26), side_on=55, arm=R(.98, .06, .30, .95),
                    front_arm=R(.90, .05, .70, .70), chest_through=18, pivot=2, hop=14, stride=74,
                    drift=0, crouch=0, bend=0, travel=_RUN_TRAVEL)
# Wrist spin: angled approach, a big gather jump, fully side-on, longer stride into a flexed front
# knee, a rounder arm with the wrist cocked and unfurling, and a big rotation and pivot through.
LEG_SPIN_R5 = _spin('LegSpin', 'legbreak', (0, 58, -34), side_on=88, arm=R(.95, -.30, .35, .88),
                    front_arm=R(.88, .25, .85, .35), chest_through=32, pivot=22, hop=30, stride=90,
                    drift=22, crouch=8, bend=14, travel=_RUN_TRAVEL)

BOWLING_R5 = [FAST_BOWL_R5, FAST_MEDIUM_R5, OFF_SPIN_R5, LEG_SPIN_R5]

# ---------------------------------------------------------------- fielding (right-handed)
#
# Event targets are where the match puts the ball relative to the athlete's root when the event
# fires: C26MatchGameMode::Collect squares a gather up 46cm in front of the root; catches arrive
# within reach. The runtime hand IK (UC26CricketerAnimInstance) closes the last few centimetres to
# the real ball, so the authored hands must already be there or close.

GATHER = (0.0, 46.0, 7.0)


def _hands(target, spread=6.0, back=7.0, up=2.0):
    """Both wrists placed so the palms close around a ball at `target` (author frame)."""
    x, y, z = target
    return {'hand_l': (x + spread, y - back, z + up), 'hand_r': (x - spread, y - back, z + up),
            'grip_l': 'ball', 'grip_r': 'ball'}


def _hands_down(target, spread=6.0):
    """Fingers pointing at the turf: wrists above the ball."""
    x, y, z = target
    return {'hand_l': (x + spread, y - 3, z + 8), 'hand_r': (x - spread, y - 3, z + 8),
            'grip_l': 'ball', 'grip_r': 'ball'}


# Side-on throwing base: left shoulder and left arm at the target, ball hand cocked high behind.
THROW_LOAD = {
    'pelvis': (6, -6, -80, 0, -6, -12), 'spine': (4, -4, -10), 'chest': (2, -2, -8),
    'neck': (-6, 0, 50), 'head': (-4, 0, 40),
    **_foot('r', -8, -18, toe=-88), **_foot('l', -2, 38, G + 5, toe=-40, knee_out=100),
    'hand_l': R(.90, .05, .80, .55), 'hand_r': R(.90, -.20, -.75, .50),
    'elbow_r': (-40, -60, 170), 'grip_r': 'ball', 'grip_l': 'flat',
}

PICKUP_R5 = _action('Pickup', [
    (3, {**_foot('l', 4, 30, G + 6, toe=-10), **_foot('r', -18, -6, G + 5, toe=-30), 'pelvis': (30, 0, -8, 0, 8, -26), 'spine': (26, 0, 0),
         'neck': (-26, 0, 0), 'head': (-10, 0, 0), 'hand_l': R(.85, .10, .55, -.82), 'hand_r': R(.85, -.10, .55, -.82)}),
    # Knees bend, head over the ball, left foot beside it, both hands meet it in front.
    (6, {**_foot('l', 14, 40, toe=-15), **_foot('r', -18, -12, toe=-40),
         'pelvis': (50, 0, -10, 0, 14, -52), 'spine': (36, 0, 4), 'chest': (12, 0, 2),
         'neck': (-38, 0, -4), 'head': (-12, 0, 0), **_hands_down(GATHER)}),
    (10, {'pelvis': (24, 0, -34, 0, 4, -24), 'spine': (18, 0, -8), 'neck': (-18, 0, 10), 'head': (-8, 0, 12),
          'hand_r': R(.55, -.10, .80, -.30), 'hand_l': R(.55, .15, .85, -.20), 'grip_l': 'open'}),
    (12, {**_foot('r', -12, -16, G + 6, toe=-70)}),
    (14, {**_foot('l', 4, 30, G + 8, toe=-50), 'pelvis': (10, -2, -64, 0, -4, -16), 'spine': (6, -2, -10),
          'neck': (-8, 0, 40), 'head': (-6, 0, 30)}),
], 'Pickup', 6, 17, dict(FIELDER_READY, **{'grip_r': 'open'}), THROW_LOAD, event_target=GATHER, hands='down')

PICKUP_RUNNING_R5 = _action('PickupRunning', [
    # Braking stride on the right foot, momentum still carrying the chest forward.
    (3, {**_foot('r', -10, 26, toe=-15), **_foot('l', 8, -18, G + 16, toe=-10),
         'pelvis': (30, 0, -8, 0, 10, -22), 'spine': (26, 0, 0), 'neck': (-24, 0, 0), 'head': (-10, 0, 0),
         'hand_l': R(.90, .15, .45, -.85), 'hand_r': R(.95, -.10, .50, -.85)}),
    # On the move: left foot plants beside the ball, low body, hands meet it in stride.
    (6, {**_foot('l', 14, 46, toe=-10), 'pelvis': (54, 0, -8, 0, 20, -52), 'spine': (36, 0, 4),
         'chest': (10, 0, 2), 'neck': (-40, 0, -4), 'head': (-12, 0, 0), **_hands_down(GATHER)}),
    # Momentum is used, not stopped: the right foot crosses behind (crow hop) into the side-on base.
    (10, {**_foot('r', -8, 10, G + 14, toe=-60), **_foot('l', 6, 44, G + 6, toe=-30), 'pelvis': (22, -2, -40, 0, 12, -24), 'spine': (16, 0, -6),
          'neck': (-16, 0, 20), 'head': (-8, 0, 16),
          'hand_r': R(.55, -.10, .80, -.30), 'hand_l': R(.55, .15, .85, -.20), 'grip_l': 'open'}),
    (13, {**_foot('r', -8, -18, toe=-88), 'pelvis': (10, -4, -70, 0, -2, -14), 'spine': (6, -2, -10),
          'neck': (-8, 0, 44), 'head': (-6, 0, 34)}),
], 'Pickup', 6, 17, shift_pose(dict(RUN[0][1], **{'grip_r': 'open', 'grip_l': 'open'}), 30),
    THROW_LOAD, event_target=GATHER, hands='down')

THROW_R5 = _action('Throw', [
    # Front foot strides at the target, hips fire first, the throwing elbow leads at shoulder height.
    (3, {**_foot('l', -4, 54, toe=-30), 'pelvis': (8, -6, -58, 0, 10, -14), 'spine': (6, -4, -16),
         'chest': (4, -2, -14), 'hand_r': R(.92, -.35, -.62, .62), 'hand_l': R(.85, .25, .70, .10)}),
    # Release high and in front of the head, chest through, front arm pulled into the side.
    (6, {'pelvis': (14, 0, -16, 0, 24, -12), 'spine': (16, 0, 4), 'chest': (10, 0, 6),
         'neck': (-16, 0, -12), 'head': (-8, 0, -10), **_foot('r', -10, -8, G + 12, toe=-60),
         'hand_r': R(.97, -.05, .60, .78), 'hand_l': R(.80, .40, -.20, -.85)}),
    (10, {'pelvis': (26, 0, 20, 0, 30, -14), 'spine': (28, 0, 14), 'neck': (-26, 0, -18), 'head': (-12, 0, -12),
          **_foot('r', -6, 34, G + 22, toe=-30, knee_out=100),
          'hand_r': R(.95, .50, .60, -.60), 'hand_l': R(.85, .30, -.40, -.84), 'grip_r': 'open'}),
    (15, {**_foot('r', -8, 58, toe=-10), 'pelvis': (16, 0, 10, 0, 40, -12), 'spine': (16, 0, 6),
          'neck': (-14, 0, -6), 'head': (-8, 0, -4)}),
    (18, {**_foot('l', 8, 44, G + 10, toe=0)}),
    (22, {**_foot('l', 20, 31, toe=0), 'pelvis': (10, 0, 0, 0, 34, -12), 'spine': (14, 0, 0),
          'hand_r': R(.74, -.34, .78, -.53), 'hand_l': R(.74, .34, .78, -.53), 'grip_l': 'flat', 'grip_r': 'flat'}),
    (25, {**_foot('r', -14, 42, G + 8, toe=-10)}),
], 'ThrowRelease', 6, 30, THROW_LOAD, shift_pose(FIELDER_READY, -30), overarm=True, thrower='r')

THROW_QUICK_R5 = _action('ThrowQuick', [
    # No crow hop: a short step and a flat, snapping arm at shoulder height for a close run-out.
    (2, {**_foot('l', -2, 40, toe=-40), 'pelvis': (8, -4, -62, 0, 4, -14), 'spine': (8, -4, -12),
         'hand_r': R(.90, -.55, -.55, .25), 'hand_l': R(.80, .30, .75, 0.0)}),
    (5, {'pelvis': (14, 0, -16, 0, 12, -14), 'spine': (14, -6, 4), 'chest': (8, -6, 8),
         'neck': (-12, 0, 4), 'head': (-8, 0, 2), **_foot('r', -10, -12, G + 6, toe=-70),
         'hand_r': R(.97, -.25, .92, .30), 'hand_l': R(.80, .45, -.10, -.85)}),
    (9, {'pelvis': (18, 0, 8, 0, 16, -14), 'spine': (18, 0, 12), 'neck': (-18, 0, -8), 'head': (-10, 0, -6),
         'hand_r': R(.90, .55, .70, -.40), 'hand_l': R(.80, .30, -.50, -.80), 'grip_r': 'open'}),
    (15, {**_foot('r', -10, 4, toe=-20), 'pelvis': (12, 0, 4, 0, 14, -12), 'spine': (12, 0, 2),
          'neck': (-10, 0, 0), 'head': (-6, 0, 0)}),
    (18, {**_foot('l', 10, 26, G + 8, toe=0)}),
    (21, {**_foot('l', 20, 15, toe=0), **_foot('r', -16, 9, G + 6, toe=0)}),
], 'ThrowRelease', 5, 24, THROW_LOAD, shift_pose(FIELDER_READY, -14), overarm=False, thrower='r')

CHEST = (0.0, 42.0, 122.0)
HIGH = (0.0, 26.0, 194.0)    # fingertips at full stretch; higher than this is a jump, not this clip
LOW = (0.0, 50.0, 30.0)

CATCH_R5 = _action('Catch', [
    (3, {'pelvis': (6, 0, 0, 0, 2, -10), 'spine': (4, 0, 0), 'neck': (-18, 0, 0), 'head': (-10, 0, 0),
         'hand_l': R(.62, .28, .82, -.30), 'hand_r': R(.62, -.28, .82, -.30), 'grip_l': 'open', 'grip_r': 'open'}),
    # Fingers up, hands in front of the eyes, elbows soft.
    (6, {'pelvis': (6, 0, 0, 0, 4, -8), 'neck': (-14, 0, 0), 'head': (-8, 0, 0),
         'elbow_l': (40, 0, 80), 'elbow_r': (-40, 0, 80), **_hands(CHEST, back=8, up=-4)}),
    # Give: the hands ride back into the chest instead of stopping dead.
    (11, {'pelvis': (12, 0, 0, 0, -4, -14), 'spine': (10, 0, 0),
          **_hands((0.0, 22.0, 112.0), back=8, up=-4)}),
    (18, {'pelvis': (8, 0, 0, 0, -2, -12), 'hand_r': R(.60, -.20, .80, -.50), 'hand_l': R(.60, .20, .80, -.50)}),
], 'Catch', 6, 24, FIELDER_READY, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'}),
    event_target=CHEST)

CATCH_HIGH_R5 = _action('CatchHigh', [
    (3, {'pelvis': (2, 0, 0, 0, -2, -14), 'spine': (-2, 0, 0), 'neck': (-24, 0, 0), 'head': (-18, 0, 0),
         'hand_l': R(.90, .20, .30, .90), 'hand_r': R(.90, -.20, .30, .90), 'grip_l': 'open', 'grip_r': 'open'}),
    # Tall, arms extended overhead, eyes up under the ball; the knees give as it lands in the hands.
    (6, {'pelvis': (-4, 0, 0, 0, -4, 0), 'spine': (-8, 0, 0), 'chest': (-4, 0, 0), 'neck': (-26, 0, 0),
         'head': (-22, 0, 0), 'elbow_l': (40, 20, 220), 'elbow_r': (-40, 20, 220),
         **_hands(HIGH, back=-4, up=-8)}),
    (11, {'pelvis': (8, 0, 0, 0, -2, -18), 'spine': (6, 0, 0), 'neck': (-16, 0, 0), 'head': (-10, 0, 0),
          **_hands((0.0, 26.0, 128.0), back=8, up=-4)}),
    (18, {'pelvis': (8, 0, 0, 0, -2, -12), 'hand_r': R(.60, -.20, .80, -.50), 'hand_l': R(.60, .20, .80, -.50)}),
], 'Catch', 6, 24, FIELDER_READY, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'}),
    event_target=HIGH)

CATCH_LOW_R5 = _action('CatchLow', [
    (3, {**_foot('l', 10, 28, G + 6, toe=-10), 'pelvis': (22, 0, 0, 0, 8, -24), 'spine': (20, 0, 0),
         'neck': (-26, 0, 0), 'head': (-10, 0, 0), 'hand_l': R(.85, .20, .50, -.80), 'hand_r': R(.85, -.20, .50, -.80),
         'grip_l': 'open', 'grip_r': 'open'}),
    # Front knee drives down to the ball, fingers point at the turf, hands under it.
    (6, {**_foot('l', 12, 40, toe=-10), 'pelvis': (40, 0, 0, 0, 14, -44), 'spine': (28, 0, 0), 'chest': (8, 0, 0),
         'neck': (-36, 0, 0), 'head': (-12, 0, 0), **_hands_down(LOW)}),
    (12, {'pelvis': (24, 0, 0, 0, 8, -26), 'spine': (18, 0, 0), 'neck': (-18, 0, 0), 'head': (-8, 0, 0),
          **_hands((0.0, 26.0, 104.0), back=8, up=-4)}),
    (16, {**_foot('l', 16, 20, G + 10, toe=0)}),
    (20, {**_foot('l', 20, 1, toe=0), 'pelvis': (10, 0, 0, 0, 0, -13), 'spine': (14, 0, 0),
          'hand_r': R(.60, -.20, .80, -.50), 'hand_l': R(.60, .20, .80, -.50)}),
], 'Catch', 6, 26, FIELDER_READY, dict(FIELDER_READY, **{'hand_r': R(.66, -.26, .74, -.62), 'grip_r': 'ball'}),
    event_target=LOW)


def _dive(name, target, event, length, ground):
    """A full-length dive to the athlete's LEFT (+X): drive off the right leg, the body flies flat,
    both hands arrive at the ball, the left hip and shoulder land, then the athlete rolls up.
    The right-side dive is this clip mirrored whole."""
    tx, ty, tz = target
    catch = not ground
    return _action(name, [
        # Load: drop the hips and cross the right leg over to push off towards the ball.
        (3, {'pelvis': (14, 10, 20, 12, 4, -24), 'spine': (12, 12, 10), 'neck': (-16, -10, -10), 'head': (-10, 0, -10),
             **_foot('l', 26, 4, G + 10, toe=30), **_foot('r', -18, 1, toe=10),
             'hand_l': R(.80, .60, .40, -.20), 'hand_r': R(.80, .20, .50, -.30), 'grip_l': 'open', 'grip_r': 'open'}),
        # Flight: body horizontal, arms long, eyes on the ball.
        (event - 2, {'pelvis': (30, 70, 40, tx * .55, ty * .5, -52 if ground else -40), 'spine': (10, 16, 10),
                     'chest': (4, 8, 4), 'neck': (-24, -30, -10), 'head': (-10, -20, -6),
                     **_foot('l', tx * .45, 6, G + 26, toe=40, knee_out=90), **_foot('r', tx * .15, -12, G + 36, toe=20),
                     'hand_l': R(.97, .80, .55, -.10 if ground else .10), 'hand_r': R(.97, .70, .65, -.05 if ground else .15)}),
        (event, {'pelvis': (34, 80, 40, tx * .64, ty * .55, -70 if ground else -58), 'spine': (8, 14, 8),
                 'neck': (-26, -36, -12), 'head': (-10, -22, -6),
                 **_foot('l', tx * .52, 2, G + 14, toe=50), **_foot('r', tx * .20, -16, G + 26, toe=30),
                 **(_hands_down(target, spread=5) if ground else _hands(target, spread=5, back=5, up=-2))}),
        # Land on the left hip and forearm, ball kept up (catch) or pinned (stop).
        (event + 5, {'pelvis': (36, 84, 40, tx * .68, ty * .5, -66), 'spine': (10, 12, 10),
                     **_foot('l', tx * .56, 0, G + 12, toe=60, knee_out=80), **_foot('r', tx * .30, -18, G + 14, toe=40, knee_out=80),
                     **_hands((tx * .92, ty * .8, tz + 12 if catch else 12), spread=5, back=6)}),
        # Roll up onto a knee, then stand.
        (event + 7, {'pelvis': (34, 68, 36, tx * .66, ty * .48, -56),
                     **_foot('l', tx * .60, 4, G + 18, toe=50, knee_out=90), **_foot('r', tx * .33, -18, G + 16, toe=35, knee_out=80)}),
        (event + 9, {'pelvis': (30, 40, 26, tx * .62, ty * .45, -46), 'spine': (16, 8, 8),
                     **_foot('l', tx * .66, 10, G + 12, toe=40, knee_out=60), **_foot('r', tx * .36, -18, G + 12, toe=30, knee_out=50)}),
        (event + 13, {'pelvis': (30, 30, 20, tx * .58, ty * .4, -42), 'spine': (20, 6, 8), 'neck': (-20, -10, -8),
                      'head': (-8, -6, -4), **_foot('l', tx * .60, 16, toe=20), **_foot('r', tx * .40, -20, G + 2, toe=10, knee_out=16),
                      'hand_r': R(.60, -.10, .80, -.40), 'hand_l': R(.60, .20, .80, -.40)}),
        (event + 16, {**_foot('r', tx * .39, -13, G + 8, toe=5)}),
        (event + 18, {**_foot('l', tx * .55 + 10, ty * .15 + 8, G + 8, toe=10)}),
        (event + 20, {'pelvis': (14, 6, 8, tx * .50, ty * .3, -22), 'spine': (14, 0, 4), 'neck': (-14, 0, 0),
                      'head': (-8, 0, 0), **_foot('r', tx * .50 - 20, ty * .3 - 1, toe=0),
                      **_foot('l', tx * .50 + 20, ty * .3 + 1, toe=0)}),
    ], 'Catch' if catch else 'Pickup', event, length, FIELDER_READY,
        dict(shift_pose(FIELDER_READY, 0), **{'pelvis': (10, 0, 0, tx * .50, ty * .3, -13),
                                              **_foot('l', tx * .50 + 20, ty * .3 + 1), **_foot('r', tx * .50 - 20, ty * .3 - 1),
                                              'grip_r': 'ball'}),
        event_target=target, hands='down' if ground else 'up', dive=True)


DIVE_CATCH_L = _dive('DiveCatch_L', (112.0, 30.0, 64.0), 8, 40, ground=False)
DIVE_STOP_L = _dive('DiveStop_L', (118.0, 24.0, 7.0), 8, 36, ground=True)

SLIDE_SAVE = _action('SlideSave', [
    # Full-speed boundary slide: drop onto the right hip, left leg long in front, right leg folded
    # out to the side, right hand sweeps the ball back infield.
    (4, {**_foot('l', 6, 80, G + 10, toe=0, knee_out=70), **_foot('r', -14, 18, G + 6, toe=-40, knee_out=50),
         'pelvis': (-6, -14, -6, -4, 40, -30), 'spine': (4, -6, -4), 'neck': (-4, 8, 0), 'head': (-10, 0, 0),
         'hand_r': R(.90, -.40, .40, -.80), 'hand_l': R(.80, .60, .20, -.50), 'grip_l': 'open', 'grip_r': 'open'}),
    (8, {**_foot('l', 6, 110, G + 1, toe=20, knee_out=40), 'foot_r': (-34, 40, G + 1), 'ankle_r': (0, 0, -80),
         'knee_r': (-90, 40, 24), 'pelvis': (-4, -26, -8, -10, 52, -70), 'spine': (46, -12, -10),
         'neck': (-18, 10, 0), 'head': (-14, 0, 0), **_hands_down((-14.0, 84.0, 7.0), spread=4)}),
    (14, {**_foot('l', 8, 118, G + 1, toe=20, knee_out=40), 'foot_r': (-34, 52, G + 1), 'knee_r': (-90, 52, 24),
          'pelvis': (-10, -22, -10, -10, 62, -60), 'spine': (12, -8, -4),
          'hand_r': R(.70, -.30, .60, -.40), 'hand_l': R(.60, .40, .40, -.60)}),
    (22, {**_foot('r', -10, 50, toe=-20), 'knee_r': (-40, 140, 60), 'pelvis': (20, -6, -6, -4, 54, -38),
          'spine': (18, -2, -2), 'neck': (-16, 0, 0), 'head': (-8, 0, 0)}),
], 'Pickup', 8, 32, shift_pose(dict(RUN[0][1], **{'grip_r': 'open', 'grip_l': 'open'}), 40),
    dict(shift_pose(FIELDER_READY, -52), **{'grip_r': 'ball'}), event_target=(-14.0, 84.0, 7.0), hands='down')

# ---------------------------------------------------------------- wicketkeeping

KEEPER_READY_R5 = {
    'pelvis': (22, 0, 0, 0, 0, -44), 'spine': (26, 0, 0), 'chest': (8, 0, 0), 'neck': (-30, 0, 0), 'head': (-10, 0, 0),
    **_foot('l', 24, 2, toe=12), **_foot('r', -24, -2, toe=-12),
    **_hands((0.0, 40.0, 38.0), spread=7, back=6, up=0), 'grip_l': 'keeper', 'grip_r': 'keeper',
    'elbow_l': (40, 10, 40), 'elbow_r': (-40, 10, 40),
}
KEEPER_RISE_R5 = dict(KEEPER_READY_R5, **{'pelvis': (14, 0, 0, 0, 0, -24), 'spine': (18, 0, 0), 'neck': (-22, 0, 0),
                                          **_hands((0.0, 44.0, 70.0), spread=7, back=6, up=0),
                                          'grip_l': 'keeper', 'grip_r': 'keeper'})


def _shuffle_left():
    """Lateral keeper shuffle towards the athlete's left: low, square, feet never cross."""
    step = 52.0
    a = dict(KEEPER_RISE_R5, **{'pelvis': (16, 0, 0, 0, 0, -30)})
    return [
        (0, dict(a, **{**_foot('l', 24, 2, toe=10), **_foot('r', -24, -2, toe=-10)})),
        (3, dict(a, **{**_foot('l', 24 + step * .5, 2, G + 8, toe=10), **_foot('r', -24, -2, toe=-10),
                       'pelvis': (16, 6, 0, step * .25, 0, -26)})),
        (6, dict(a, **{**_foot('l', 24 + step, 2, toe=10), **_foot('r', -24 + step * .3, -2, G + 4, toe=-10),
                       'pelvis': (16, 0, 0, step * .6, 0, -32)})),
        (9, dict(a, **{**_foot('l', 24 + step, 2, toe=10), **_foot('r', -24 + step, -2, G + 6, toe=-10),
                       'pelvis': (16, -4, 0, step * .9, 0, -28)})),
        (12, dict(a, **{**_foot('l', 24 + step, 2, toe=10), **_foot('r', -24 + step, -2, toe=-10),
                        'pelvis': (16, 0, 0, step, 0, -30)})),
    ]


def _shuffle_loop():
    """In place: the root carries the keeper sideways, so the feet travel back under it."""
    keys = _shuffle_left()
    return [(f, dict(spec, **{k: (v[0] - 52.0 * f / 12.0, v[1], v[2]) for k, v in spec.items()
                              if k.startswith(('foot_', 'knee_', 'hand_', 'elbow_')) and len(v) == 3
                              and all(isinstance(c, (int, float)) for c in v)},
                     pelvis=spec['pelvis'][:3] + (spec['pelvis'][3] - 52.0 * f / 12.0,) + spec['pelvis'][4:]))
            for f, spec in keys]


KEEPER_TAKE = (0.0, 52.0, 62.0)
KEEPER_LOW = (0.0, 58.0, 18.0)

KEEPER_RECEIVE_R5 = _action('KeeperReceive', [
    (3, dict(KEEPER_RISE_R5)),
    # Rise with the bounce, gloves together fingers down, take it in front of the body.
    (6, {'pelvis': (14, 0, 0, 0, 2, -28), **_hands(KEEPER_TAKE, spread=7, back=4, up=-2),
         'grip_l': 'keeper', 'grip_r': 'keeper'}),
    # Give: the gloves ride back towards the right hip.
    (11, {'pelvis': (14, 0, -8, 0, -2, -26), 'spine': (18, 0, -6),
          **_hands((-14.0, 30.0, 70.0), spread=6, back=4, up=0), 'grip_l': 'keeper', 'grip_r': 'keeper'}),
    (20, dict(KEEPER_RISE_R5, **{'pelvis': (14, 0, 0, 0, 0, -22)})),
], 'Catch', 6, 30, KEEPER_READY_R5, KEEPER_READY_R5, event_target=KEEPER_TAKE, keeper=True)

KEEPER_TAKE_LOW_R5 = _action('KeeperTakeLow', [
    (3, dict(KEEPER_READY_R5, **{'pelvis': (26, 0, 0, 0, 4, -48)})),
    # Stay down with a low ball: gloves in a cup on the turf, head over them.
    (6, {'pelvis': (32, 0, 0, 0, 8, -52), 'spine': (30, 0, 0), 'neck': (-36, 0, 0), 'head': (-14, 0, 0),
         **_hands_down(KEEPER_LOW, spread=7), 'grip_l': 'keeper', 'grip_r': 'keeper'}),
    (12, {'pelvis': (22, 0, 0, 0, 2, -40), 'spine': (24, 0, 0), 'neck': (-26, 0, 0),
          **_hands((-6.0, 36.0, 52.0), spread=6, back=4, up=0), 'grip_l': 'keeper', 'grip_r': 'keeper'}),
], 'Catch', 6, 26, KEEPER_READY_R5, KEEPER_READY_R5, event_target=KEEPER_LOW, keeper=True)

KEEPER_DIVE_L = _action('KeeperDive_L', [
    (2, dict(KEEPER_READY_R5, **{'pelvis': (20, 8, 20, 10, 2, -40), **_foot('r', -22, -2, G + 4, toe=20)})),
    (5, {'pelvis': (26, 60, 40, 60, 10, -46), 'spine': (8, 14, 8), 'neck': (-24, -30, -8), 'head': (-10, -20, -4),
         **_foot('l', 58, 4, G + 22, toe=40), **_foot('r', 18, -8, G + 30, toe=20),
         **_hands((72.0, 42.0, 56.0), spread=6, back=4, up=0), 'grip_l': 'keeper', 'grip_r': 'keeper'}),
    # Full stretch: gloves together at the ball, body airborne and flat.
    (7, {'pelvis': (30, 76, 40, 72, 16, -58), 'spine': (8, 12, 8),
         **_foot('l', 66, 4, G + 16, toe=50), **_foot('r', 24, -12, G + 24, toe=30),
         **_hands((118.0, 40.0, 58.0), spread=6, back=4, up=-2), 'grip_l': 'keeper', 'grip_r': 'keeper'}),
    (12, {'pelvis': (34, 84, 40, 78, 14, -66), **_foot('l', 72, 0, G + 12, toe=60, knee_out=80), **_foot('r', 36, -16, G + 14, toe=40, knee_out=80),
          **_hands((104.0, 34.0, 44.0), spread=6, back=4)}),
    (15, {'pelvis': (32, 60, 32, 76, 12, -48), **_foot('l', 74, 6, G + 16, toe=50, knee_out=90),
          **_foot('r', 42, -16, G + 14, toe=30, knee_out=70)}),
    (20, {'pelvis': (30, 30, 20, 70, 8, -50), 'spine': (22, 6, 8), **_foot('l', 76, 14, toe=20),
          **_foot('r', 50, -16, G + 2, toe=10, knee_out=16), **_hands((60.0, 40.0, 70.0), spread=6, back=4)}),
    (24, {**_foot('r', 44, -8, G + 8, toe=0)}),
    (26, {**_foot('l', 82, 8, G + 8, toe=12)}),
], 'Catch', 7, 30, KEEPER_READY_R5,
    dict(KEEPER_RISE_R5, **{'pelvis': (14, 0, 0, 62, 0, -24), **_foot('l', 86, 2, toe=12), **_foot('r', 38, -2, toe=-12),
                            **_hands((62.0, 44.0, 70.0), spread=7, back=6, up=0),
                            'grip_l': 'keeper', 'grip_r': 'keeper'}),
    event_target=(118.0, 40.0, 58.0), keeper=True, dive=True)

FIELDING_R5 = [PICKUP_R5, PICKUP_RUNNING_R5, THROW_R5, THROW_QUICK_R5, CATCH_R5, CATCH_HIGH_R5, CATCH_LOW_R5,
               SLIDE_SAVE, KEEPER_RECEIVE_R5, KEEPER_TAKE_LOW_R5]
MIRRORED_R5 = [DIVE_CATCH_L, DIVE_STOP_L, KEEPER_DIVE_L]   # left source; _R is the whole-body mirror


# ================================================================= celebrations and signals

CELEBRATE = [
    (0, FIELDER_READY),
    (8, dict(FIELDER_READY, **{'pelvis': (2, 0, 0, 0, 0, -4), 'spine': (-6, 0, 0),
                               'neck': (-10, 0, 0),
                               'hand_l': R(.80, .62, .22, .04), 'hand_r': R(.80, -.62, .22, .04),
                               'elbow_l': (54, -22, 112), 'elbow_r': (-54, -22, 112)})),
    (16, dict(FIELDER_READY, **{'pelvis': (-6, 0, 0, 0, 0, 2), 'spine': (-14, 0, 0),
                               'neck': (-16, 0, 0), 'head': (-10, 0, 0),
                               'hand_l': R(.96, .26, .10, .96), 'hand_r': R(.96, -.26, .10, .96),
                               'elbow_l': (48, -18, 150), 'elbow_r': (-48, -18, 150)})),
    (26, dict(FIELDER_READY, **{'pelvis': (-2, 0, 0, 0, 0, -2), 'spine': (-8, 0, 0),
                               'hand_l': R(.86, .30, .24, .84), 'hand_r': R(.86, -.30, .24, .84)})),
    (38, dict(FIELDER_READY, **{'hand_l': R(.92, .28, .16, .90), 'hand_r': R(.92, -.28, .16, .90)})),
    (50, FIELDER_READY),
]

BATTER_CELEBRATE_R = [
    (0, BATTER_READY_R),
    (10, dict(BATTER_READY_R, **{'pelvis': (4, 0, -30, 0, 0, -8), 'spine': (-4, 0, -6),
                                 'neck': (-12, 0, 16),
                                 **_bat_hands((-12, 3, 94), (0.06, 0.20, 0.98), (0.10, 0.98, -0.10))})),
    # Bat raised to the dressing room, helmet hand out to the side.
    (22, {'pelvis': (-4, 0, -18, 0, 0, 2), 'spine': (-12, 0, 0), 'chest': (-6, 0, 4),
          'neck': (-14, 0, 8), 'head': (-10, 0, 6),
          'foot_l': (4, 10, GROUND), 'foot_r': (-6, -13, GROUND),
          'hand_l': (10, 12, 168), 'shaft': (0.0, 0.10, 0.99), 'face': (0.0, 1.0, 0.0),
          'hand_r': R(.84, -.60, .06, .60),
          'elbow_l': (30, -16, 158), 'elbow_r': (-52, -20, 128),
          'grip_l': 'bat', 'grip_r': 'open'}),
    (34, {'pelvis': (-2, 0, -20, 0, 0, 0), 'spine': (-10, 0, 0), 'neck': (-12, 0, 10),
          'foot_l': (4, 10, GROUND), 'foot_r': (-6, -13, GROUND),
          'hand_l': (10, 12, 164), 'shaft': (0.0, 0.10, 0.99), 'face': (0.0, 1.0, 0.0),
          'hand_r': R(.80, -.56, .10, .52),
          'elbow_l': (32, -14, 150), 'elbow_r': (-50, -18, 122),
          'grip_l': 'bat', 'grip_r': 'open'}),
    (48, BATTER_READY_R),
]

DISAPPOINTED = [
    (0, FIELDER_READY),
    (12, dict(FIELDER_READY, **{'pelvis': (16, 0, 0, 0, 0, -10), 'spine': (18, 0, 0),
                                'neck': (26, 0, 0), 'head': (16, 0, 0),
                                'hand_l': R(.72, .34, .10, -.94), 'hand_r': R(.72, -.34, .10, -.94)})),
    (24, dict(FIELDER_READY, **{'pelvis': (14, 0, 0, 0, 0, -12), 'spine': (16, 0, 0),
                                'neck': (24, 0, 0), 'head': (14, 0, 0),
                                'hand_l': R(.44, .30, .12, -.30), 'hand_r': R(.78, -.52, .06, -.70),
                                'elbow_l': (40, -26, 116), 'elbow_r': (-52, -20, 104)})),
    (40, dict(FIELDER_READY, **{'neck': (12, 0, 0), 'head': (8, 0, 0)})),
    (52, FIELDER_READY),
]


def _signal(hold, rise_frames=12, hold_frames=22, length=48):
    return [(0, UMPIRE_READY), (rise_frames, hold),
            (rise_frames + hold_frames, hold), (length, UMPIRE_READY)]


# One arm straight up.
SIGNAL_OUT = _signal({
    'pelvis': (0, 0, 0, 0, 0, 0), 'spine': (-2, 0, -4), 'neck': (-6, 0, 0),
    'foot_l': (12, 0, GROUND), 'foot_r': (-12, 0, GROUND),
    'hand_r': R(.98, -.12, .04, .99), 'elbow_r': (-40, -14, 168),
    'hand_l': R(.56, .20, -.62, -.76), 'elbow_l': (40, -34, 118),
    'grip_r': 'open', 'grip_l': 'open',
})
# One arm swept horizontally across the body at waist height.
SIGNAL_FOUR = _signal({
    'pelvis': (0, 0, 6, 0, 0, -2), 'spine': (2, 0, 8), 'neck': (-6, 0, -6),
    'foot_l': (12, 0, GROUND), 'foot_r': (-12, 0, GROUND),
    'hand_r': R(.90, .62, .48, -.62), 'elbow_r': (-10, -6, 118),
    'hand_l': R(.54, .22, -.60, -.77), 'elbow_l': (42, -32, 118),
    'grip_r': 'flat', 'grip_l': 'open',
})
# Both arms straight overhead.
SIGNAL_SIX = _signal({
    'pelvis': (-2, 0, 0, 0, 0, 2), 'spine': (-6, 0, 0), 'neck': (-10, 0, 0), 'head': (-6, 0, 0),
    'foot_l': (13, 0, GROUND), 'foot_r': (-13, 0, GROUND),
    'hand_l': R(.98, .16, .06, .98), 'hand_r': R(.98, -.16, .06, .98),
    'elbow_l': (42, -14, 168), 'elbow_r': (-42, -14, 168),
    'grip_l': 'open', 'grip_r': 'open',
})
# Both arms straight out to the sides at shoulder height.
SIGNAL_WIDE = _signal({
    'pelvis': (0, 0, 0, 0, 0, 0), 'spine': (0, 0, 0), 'neck': (-4, 0, 0),
    'foot_l': (13, 0, GROUND), 'foot_r': (-13, 0, GROUND),
    'hand_l': R(.98, .98, .06, .10), 'hand_r': R(.98, -.98, .06, .10),
    'elbow_l': (44, -18, 140), 'elbow_r': (-44, -18, 140),
    'grip_l': 'flat', 'grip_r': 'flat',
})
UMPIRE_WALK = _locomotion(reach=20, lift=10, drop=2.6, lean=3,
                          fwd=(.72, .26, .38, -.89), back=(.76, .28, -.34, -.90), cycle=34)


# ================================================================= assembly

def _loopclip(name, keys, speed=0.0, dense=False):
    return {'name': name, 'keys': keys, 'loop': True, 'speed': speed, 'dense': dense}


def _oneshot(name, keys, event=None, contact=0, dense=False, travel=None, resolve_mixed=False):
    return {'name': name, 'keys': keys, 'loop': False, 'event': event, 'contact': contact, 'dense': dense,
            'travel': travel, 'resolve_mixed': resolve_mixed}


def _mirror_keys(keys):
    return [(f, mirror(s)) for f, s in keys]


def build_manifest():
    """Every clip the profile contract requires, plus the variations the brief asks for by name.

    Left-handed entries are mirrored from the right-handed source at authoring time, so they are
    real assets on disk rather than a runtime flag - the validator can check them like any other.
    """
    clips = [
        _loopclip('FielderReady', [(0, FIELDER_READY), (40, FIELDER_READY_SHIFT),
                                   (80, FIELDER_KNEES), (120, FIELDER_READY)]),
        _loopclip('KeeperReady', [(0, KEEPER_READY_R5), (34, KEEPER_RISE_R5), (70, KEEPER_READY_R5)], dense=True),
        _loopclip('KeeperShuffle_L', _shuffle_loop(), 130.0, dense=True),
        _loopclip('KeeperShuffle_R', _mirror_keys(_shuffle_loop()), 130.0, dense=True),
        _loopclip('UmpireReady', [(0, UMPIRE_READY),
                                  (44, dict(UMPIRE_READY, **{'pelvis': (3, 0, -5, 0, 0, -3),
                                                             'neck': (-6, 0, 8)})),
                                  (92, UMPIRE_READY)]),
        _loopclip('BowlerReady', [(0, BOWLER_READY),
                                  (36, dict(BOWLER_READY, **{'pelvis': (6, 0, -10, 0, 0, -7),
                                                             'neck': (-10, 0, 10),
                                                             'hand_r': R(.46, -.10, .86, -.50)})),
                                  (76, BOWLER_READY)]),
        _loopclip('BatterReady_R', [(0, BATTER_READY_R), (30, BATTER_READY_TAP),
                                    (64, BATTER_READY_R)], dense=True),
        _loopclip('Walk', WALK, 100.0), _loopclip('Run', RUN, 310.0),
        _loopclip('Jog', JOG, 200.0), _loopclip('Sprint', SPRINT, 460.0),
        _loopclip('UmpireWalk', UMPIRE_WALK, 60.0),
        _loopclip('BatterRun_R', BATTER_RUN_R, 300.0),
        _oneshot('Start', START), _oneshot('Stop', STOP),
        _oneshot('TurnLeft', _turn(1)), _oneshot('TurnRight', _turn(-1)),
        _oneshot('Celebrate', CELEBRATE), _oneshot('Disappointed', DISAPPOINTED),
        _oneshot('BatterCelebrate_R', BATTER_CELEBRATE_R),
        _oneshot('SignalOut', SIGNAL_OUT), _oneshot('SignalFour', SIGNAL_FOUR),
        _oneshot('SignalSix', SIGNAL_SIX), _oneshot('SignalWide', SIGNAL_WIDE),
    ]
    # Round 5: right-handed fielding/keeping sources, whole-body mirrored dives, and every bowling
    # action for both arms (the _L bowling clip is a genuine left-arm action, not a flipped arm).
    for action in FIELDING_R5:
        clips.append(_oneshot(action['name'], action['keys'], action['event'], action['contact'], dense=True,
                              resolve_mixed=True))
    for action in MIRRORED_R5:
        right = action['name'][:-2] + '_R'
        clips.append(_oneshot(action['name'], action['keys'], action['event'], action['contact'], dense=True,
                              resolve_mixed=True))
        clips.append(_oneshot(right, _mirror_keys(action['keys']), action['event'], action['contact'], dense=True,
                              resolve_mixed=True))
    for action in BOWLING_R5:
        for hand, keys in (('_R', action['keys']), ('_L', _mirror_keys(action['keys']))):
            clips.append(_oneshot(action['name'] + hand, keys, action['event'], action['contact'],
                                  dense=True, travel=action['travel'], resolve_mixed=True))

    # Mirror the batter-specific loops and one-shots.
    clips.append(_loopclip('BatterReady_L', _mirror_keys(
        [(0, BATTER_READY_R), (30, BATTER_READY_TAP), (64, BATTER_READY_R)]), dense=True))
    clips.append(_loopclip('BatterRun_L', _mirror_keys(BATTER_RUN_R), 300.0))
    clips.append(_oneshot('BatterCelebrate_L', _mirror_keys(BATTER_CELEBRATE_R)))

    for shot in SHOTS:
        clips.append(_oneshot(f"{shot['name']}_R", shot['keys'], shot['event'], shot['contact'], dense=True))
        clips.append(_oneshot(f"{shot['name']}_L", _mirror_keys(shot['keys']),
                              shot['event'], shot['contact'], dense=True))
    for delivery in [d for d in DELIVERIES if d['name'] not in {a['name'] for a in BOWLING_R5}]:
        clips.append(_oneshot(f"{delivery['name']}_R", delivery['keys'],
                              delivery['event'], delivery['contact']))
        clips.append(_oneshot(f"{delivery['name']}_L", _mirror_keys(delivery['keys']),
                              delivery['event'], delivery['contact']))
    return clips
