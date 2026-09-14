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

# Right-handed stance: hips ~48 deg closed, shoulders a further 20, head turned back square to the
# bowler. The turn is spread across pelvis, chest, neck_01, neck_02 and head so no joint exceeds 30.
# Hands rest cleanly at the right hip with the bat blade grounded just outside the back foot.
BATTER_READY_R = {
    'pelvis': (8, 0, -38, 0, 0, -9), 'spine': (14, 0, -9), 'chest': (4, 0, -14),
    'neck': (-10, 0, 30), 'head': (-4, 0, 28),
    'foot_l': (1, 10, GROUND), 'foot_r': (-3, -13, GROUND),
    'elbow_l': (14, 12, 115), 'elbow_r': (-22, -10, 105),
    **_bat_hands((-12, 3, 92), (0.06, 0.20, 0.98), (0.10, 0.98, -0.10)),
}
BATTER_READY_TAP = dict(BATTER_READY_R, **{   # bat tap: the stance breathes instead of freezing
    'pelvis': (9, 0, -38, 0, 0, -10), 'spine': (15, 0, -9),
    **_bat_hands((-12, 3, 90), (0.06, 0.20, 0.98), (0.10, 0.98, -0.10)),
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

def _shot(name, backlift, stride, plant, contact, through, recover, contact_frame=30, length=58):
    """A stroke as five real phases. `contact_frame` is where the BatContact notify goes, and the
    simulation warps its own timing onto that frame rather than the clip dictating the outcome."""
    return {
        'name': name, 'event': 'BatContact', 'contact': contact_frame,
        'keys': [(0, BATTER_READY_R), (8, backlift), (16, stride), (23, plant),
                 (contact_frame, contact), (38, through), (48, recover), (length, BATTER_READY_R)],
    }


# Backlift is shared: hands lift up and back over the off stump along the right hip,
# front shoulder drops, weight settles on the back foot. Bat blade angles back towards slips.
_BACKLIFT = dict(BATTER_READY_R, **{
    'pelvis': (6, 0, -42, 0, -2, -11), 'spine': (12, 0, -11), 'chest': (2, -6, -16),
    'neck': (-10, 0, 32), 'head': (-4, 0, 28),
    'foot_l': (1, 12, GROUND + 2), 'foot_r': (-3, -14, GROUND),
    'elbow_l': (18, 14, 125), 'elbow_r': (-26, -14, 115),
    **_bat_hands((-14, -2, 108), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
})

COVER_DRIVE = _shot(
    'COVERDRIVE',
    backlift=_BACKLIFT,
    # Stride is across and towards the pitch of the ball, not merely forward.
    stride=dict(_BACKLIFT, **{
        'pelvis': (11, 0, -40, -3, 6, -16), 'spine': (16, 0, -12), 'chest': (4, -4, -18),
        'foot_l': (-7, 29, GROUND), 'foot_r': (-3, -15, GROUND + 1),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (15, 0, -34, -5, 9, -19), 'spine': (19, 0, -4), 'chest': (7, -2, -8),
        'neck': (-14, 0, 24), 'head': (-8, 0, 22),
        'foot_l': (-8, 31, GROUND), 'foot_r': (-4, -16, GROUND + 3),
        'elbow_l': (14, 18, 124), 'elbow_r': (-22, -6, 108),
        **_bat_hands((-14, 22, 102), (-0.20, -0.25, 0.94), (-0.30, 0.94, 0.15)),
    }),
    # Head over the ball, hands out in front of the front pad, high front elbow.
    contact=dict(BATTER_READY_R, **{
        'pelvis': (17, 0, -12, -5, 10, -18), 'spine': (20, 0, 12), 'chest': (8, 0, 10),
        'neck': (-16, 0, 10), 'head': (-10, 0, 8),
        'foot_l': (-8, 31, GROUND), 'foot_r': (-5, -17, GROUND + 5),
        'elbow_l': (10, 24, 128), 'elbow_r': (-20, 6, 104),
        **_bat_hands((-12, 32, 94), (-0.25, -0.10, 0.96), (-0.40, 0.91, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (14, 0, 16, -4, 9, -15), 'spine': (14, 0, 26), 'chest': (4, 4, 20),
        'neck': (-12, 0, -8), 'head': (-8, 0, -6),
        'foot_l': (-8, 31, GROUND), 'foot_r': (-7, -16, GROUND + 9),
        'elbow_l': (6, 28, 142), 'elbow_r': (-14, 18, 124),
        **_bat_hands((-8, 36, 140), (-0.25, -0.45, -0.86), (-0.40, 0.91, 0.0)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (10, 0, -40, -2, 5, -14), 'spine': (14, 0, -6),
        'foot_l': (-5, 24, GROUND), 'foot_r': (-4, -16, GROUND),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-12, 16, 104), (-0.10, 0.10, 0.99), (-0.10, 0.99, -0.05)),
    }),
)

STRAIGHT_DRIVE = _shot(
    'STRAIGHTDRIVE', backlift=_BACKLIFT,
    stride=dict(_BACKLIFT, **{
        'pelvis': (10, 0, -40, 0, 6, -15), 'spine': (15, 0, -10), 'chest': (4, -4, -14),
        'neck': (-12, 0, 30), 'head': (-6, 0, 26),
        'foot_l': (1, 30, GROUND + 2), 'foot_r': (-3, -14, GROUND),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (14, 0, -32, 0, 9, -18), 'spine': (18, 0, -4), 'chest': (6, -2, -8),
        'neck': (-14, 0, 24), 'head': (-8, 0, 20),
        'foot_l': (1, 32, GROUND), 'foot_r': (-4, -15, GROUND + 2),
        'elbow_l': (12, 18, 124), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 24, 102), (-0.05, -0.25, 0.96), (0.05, 0.96, 0.25)),
    }),
    contact=dict(BATTER_READY_R, **{
        'pelvis': (16, 0, -16, 0, 11, -16), 'spine': (20, 0, 10), 'chest': (8, 0, 8),
        'neck': (-16, 0, 12), 'head': (-10, 0, 8),
        'foot_l': (1, 32, GROUND), 'foot_r': (-5, -16, GROUND + 5),
        'elbow_l': (6, 26, 126), 'elbow_r': (-16, 8, 104),
        **_bat_hands((-4, 36, 95), (-0.02, -0.10, 0.99), (0.0, 1.0, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (14, 0, 10, 0, 10, -14), 'spine': (14, 0, 20), 'chest': (6, 2, 16),
        'neck': (-12, 0, -4), 'head': (-8, 0, -2),
        'foot_l': (1, 32, GROUND), 'foot_r': (-6, -15, GROUND + 8),
        'elbow_l': (6, 32, 142), 'elbow_r': (-12, 22, 128),
        **_bat_hands((0, 38, 142), (0.0, -0.45, -0.89), (0.0, 1.0, 0.0)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (10, 0, -32, 0, 5, -13), 'spine': (15, 0, -6),
        'foot_l': (1, 20, GROUND), 'foot_r': (-4, -14, GROUND),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-12, 16, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
)

ON_DRIVE = _shot(
    'ONDRIVE', backlift=_BACKLIFT,
    stride=dict(_BACKLIFT, **{
        'pelvis': (12, 0, -44, 3, 7, -16), 'spine': (17, 0, -8),
        'foot_l': (9, 28, GROUND), 'foot_r': (-3, -15, GROUND + 1),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (16, 0, -28, 5, 9, -19), 'spine': (20, 0, 2), 'neck': (-14, 0, 20),
        'foot_l': (11, 30, GROUND), 'foot_r': (-4, -16, GROUND + 3),
        'elbow_l': (12, 18, 124), 'elbow_r': (-18, -6, 108),
        **_bat_hands((-6, 22, 102), (0.15, -0.25, 0.95), (0.30, 0.95, 0.10)),
    }),
    # The bat works around the front pad, so the hips open earlier than in a straight drive.
    contact=dict(BATTER_READY_R, **{
        'pelvis': (17, 0, -6, 5, 10, -18), 'spine': (20, 0, 18), 'chest': (8, 2, 14),
        'neck': (-16, 0, 2), 'head': (-10, 0, 2),
        'foot_l': (11, 30, GROUND), 'foot_r': (-5, -17, GROUND + 5),
        'elbow_l': (8, 24, 126), 'elbow_r': (-12, 10, 104),
        **_bat_hands((2, 32, 94), (0.20, -0.10, 0.97), (0.35, 0.93, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (13, 0, 24, 4, 9, -15), 'spine': (13, 0, 30), 'chest': (4, 6, 22),
        'neck': (-10, 0, -14), 'foot_l': (11, 30, GROUND), 'foot_r': (-8, -14, GROUND + 10),
        'elbow_l': (10, 28, 142), 'elbow_r': (-8, 20, 124),
        **_bat_hands((6, 36, 140), (0.20, -0.45, -0.87), (0.35, 0.93, 0.0)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (10, 0, -38, 2, 5, -14), 'spine': (14, 0, -4),
        'foot_l': (7, 22, GROUND),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 16, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
)

# Back-foot stroke: rock back, open up, swing across the line at chest height, finish wrapped round.
PULL = _shot(
    'PULL',
    backlift=dict(_BACKLIFT, **{
        'pelvis': (4, 0, -56, 0, -6, -12), 'foot_r': (-5, -18, GROUND),
        'elbow_l': (18, 10, 128), 'elbow_r': (-24, -16, 118),
        **_bat_hands((-16, -4, 115), (-0.20, 0.60, -0.77), (0.20, 0.80, 0.56)),
    }),
    stride=dict(_BACKLIFT, **{
        'pelvis': (2, 0, -48, -4, -9, -15), 'spine': (8, 0, -8), 'neck': (-8, 0, 30),
        'foot_l': (4, 2, GROUND + 4), 'foot_r': (-10, -19, GROUND),
        'elbow_l': (18, 10, 128), 'elbow_r': (-24, -16, 118),
        **_bat_hands((-16, -4, 115), (-0.20, 0.60, -0.77), (0.20, 0.80, 0.56)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (2, 0, -30, -4, -9, -16), 'spine': (8, 0, 4), 'chest': (2, 0, -6),
        'neck': (-8, 0, 20), 'foot_l': (6, 4, GROUND + 2), 'foot_r': (-11, -19, GROUND),
        'elbow_l': (14, 16, 128), 'elbow_r': (-20, -8, 115),
        **_bat_hands((-14, 10, 120), (-0.40, 0.10, 0.91), (0.10, 0.99, 0.10)),
    }),
    contact=dict(BATTER_READY_R, **{
        'pelvis': (3, 0, 6, -3, -8, -15), 'spine': (6, 0, 28), 'chest': (0, 0, 20),
        'neck': (-6, 0, -14), 'head': (-4, 0, -10),
        'foot_l': (8, 6, GROUND + 1), 'foot_r': (-11, -19, GROUND),
        'elbow_l': (12, 24, 130), 'elbow_r': (-12, 14, 118),
        **_bat_hands((0, 24, 118), (0.50, 0.10, 0.86), (0.60, 0.79, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (2, 0, 42, -2, -7, -14), 'spine': (4, 0, 40), 'chest': (-2, 0, 28),
        'neck': (-4, 0, -34), 'head': (-2, 0, -22),
        'foot_l': (9, 5, GROUND + 3), 'foot_r': (-11, -19, GROUND),
        'elbow_l': (16, 20, 134), 'elbow_r': (-4, 20, 124),
        **_bat_hands((14, 16, 126), (0.80, -0.20, 0.56), (0.80, 0.58, 0.10)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (6, 0, -26, -1, -4, -14), 'spine': (10, 0, 4), 'neck': (-8, 0, 16),
        'foot_l': (5, 6, GROUND), 'foot_r': (-8, -17, GROUND),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-12, 10, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
    contact_frame=29,
)

# Front knee drives across and the back knee goes to the ground; the bat sweeps low and horizontal.
SWEEP = _shot(
    'SWEEP', backlift=_BACKLIFT,
    stride=dict(_BACKLIFT, **{
        'pelvis': (14, 0, -48, -2, 5, -24), 'spine': (18, 0, -10),
        'foot_l': (-4, 30, GROUND), 'foot_r': (-12, -12, GROUND + 4),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (22, 0, -38, -3, 6, -42), 'spine': (22, 0, -4), 'neck': (-18, 0, 26),
        'foot_l': (-6, 33, GROUND), 'foot_r': (-15, -9, GROUND + 12),
        'knee_r': (-15, -22, 2),
        'elbow_l': (12, 16, 100), 'elbow_r': (-18, -6, 88),
        **_bat_hands((-10, 20, 80), (-0.30, 0.0, 0.95), (0.0, 0.98, 0.15)),
    }),
    contact=dict(BATTER_READY_R, **{
        'pelvis': (26, 0, -18, -3, 6, -44), 'spine': (24, 0, 14), 'chest': (8, 0, 6),
        'neck': (-22, 0, 8), 'head': (-14, 0, 8),
        'foot_l': (-6, 33, GROUND), 'foot_r': (-15, -9, GROUND + 13),
        'knee_r': (-15, -24, 2),
        'elbow_l': (10, 20, 86), 'elbow_r': (-10, 12, 76),
        **_bat_hands((0, 28, 65), (0.50, 0.10, 0.86), (0.60, 0.79, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (24, 0, 4, -3, 6, -43), 'spine': (20, 0, 30), 'chest': (6, 0, 20),
        'neck': (-18, 0, -12), 'head': (-10, 0, -8),
        'foot_l': (-6, 33, GROUND), 'foot_r': (-15, -9, GROUND + 13),
        'knee_r': (-15, -24, 2),
        'elbow_l': (14, 18, 92), 'elbow_r': (-4, 16, 82),
        **_bat_hands((12, 20, 75), (0.80, -0.10, 0.59), (0.80, 0.58, 0.10)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (18, 0, -34, -2, 4, -30), 'spine': (18, 0, 0), 'neck': (-14, 0, 20),
        'foot_l': (-4, 26, GROUND), 'foot_r': (-12, -12, GROUND + 3),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 16, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
    contact_frame=31,
)

# Wristy deflection: almost no footwork, hands stay inside the line, the face rolls at contact.
LEG_GLANCE = _shot(
    'LEGGLANCE',
    backlift=dict(_BACKLIFT, **{
        'pelvis': (7, 0, -42, 0, 0, -12),
        'elbow_l': (18, 14, 125), 'elbow_r': (-26, -14, 115),
        **_bat_hands((-14, -2, 108), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    stride=dict(_BACKLIFT, **{
        'pelvis': (10, 0, -48, 1, 3, -14), 'spine': (14, 0, -12),
        'foot_l': (4, 18, GROUND),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (12, 0, -44, 2, 4, -15), 'spine': (15, 0, -8), 'neck': (-12, 0, 28),
        'foot_l': (5, 20, GROUND),
        'elbow_l': (14, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-8, 16, 102), (-0.05, -0.15, 0.98), (0.15, 0.98, 0.15)),
    }),
    contact=dict(BATTER_READY_R, **{
        'pelvis': (13, 0, -36, 2, 5, -15), 'spine': (16, 0, -2), 'chest': (5, 0, -8),
        'neck': (-13, 0, 22), 'head': (-9, 0, 18),
        'foot_l': (5, 20, GROUND), 'foot_r': (-4, -15, GROUND + 1),
        'elbow_l': (10, 20, 112), 'elbow_r': (-14, 8, 98),
        **_bat_hands((4, 22, 92), (0.25, -0.15, 0.95), (0.70, 0.71, 0.0)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (12, 0, -24, 2, 4, -15), 'spine': (14, 0, 10), 'chest': (4, 0, 4),
        'neck': (-11, 0, 12), 'foot_l': (5, 20, GROUND), 'foot_r': (-5, -15, GROUND + 2),
        'elbow_l': (12, 22, 118), 'elbow_r': (-8, 14, 106),
        **_bat_hands((8, 26, 100), (0.45, -0.20, 0.87), (0.80, 0.58, 0.10)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (10, 0, -44, 1, 2, -13),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 16, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
    contact_frame=30, length=52,
)

# Soft hands, bat vertical and angled down, no follow-through at all.
DEFENCE = _shot(
    'FRONTFOOTDEFENCE',
    backlift=_BACKLIFT,
    stride=dict(_BACKLIFT, **{
        'pelvis': (13, 0, -48, 0, 6, -17), 'spine': (18, 0, -12),
        'foot_l': (0, 25, GROUND),
        'elbow_l': (16, 14, 126), 'elbow_r': (-24, -12, 115),
        **_bat_hands((-14, -2, 110), (-0.15, 0.65, -0.74), (0.15, 0.85, 0.50)),
    }),
    plant=dict(_BACKLIFT, **{
        'pelvis': (17, 0, -44, 0, 8, -20), 'spine': (21, 0, -10), 'neck': (-16, 0, 26),
        'foot_l': (0, 27, GROUND), 'foot_r': (-4, -16, GROUND + 2),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 20, 100), (-0.05, -0.15, 0.98), (0.05, 0.98, 0.15)),
    }),
    contact=dict(BATTER_READY_R, **{
        'pelvis': (19, 0, -40, 0, 9, -21), 'spine': (23, 0, -6), 'chest': (9, 0, -10),
        'neck': (-18, 0, 24), 'head': (-13, 0, 20),
        'foot_l': (0, 27, GROUND), 'foot_r': (-4, -16, GROUND + 3),
        'elbow_l': (10, 20, 114), 'elbow_r': (-16, -4, 96),
        **_bat_hands((-6, 28, 92), (0.0, -0.15, 0.98), (0.0, 0.98, 0.15)),
    }),
    through=dict(BATTER_READY_R, **{
        'pelvis': (18, 0, -40, 0, 9, -20), 'spine': (22, 0, -6), 'chest': (8, 0, -10),
        'neck': (-17, 0, 24), 'head': (-12, 0, 20),
        'foot_l': (0, 27, GROUND), 'foot_r': (-4, -16, GROUND + 3),
        'elbow_l': (10, 20, 114), 'elbow_r': (-16, -4, 96),
        **_bat_hands((-6, 28, 92), (0.0, -0.15, 0.98), (0.0, 0.98, 0.15)),
    }),
    recover=dict(BATTER_READY_R, **{
        'pelvis': (12, 0, -46, 0, 4, -15), 'foot_l': (0, 20, GROUND),
        'elbow_l': (12, 16, 120), 'elbow_r': (-20, -6, 108),
        **_bat_hands((-10, 16, 104), (0.05, 0.10, 0.99), (0.05, 0.99, -0.05)),
    }),
    contact_frame=30, length=50,
)

SHOTS = [COVER_DRIVE, STRAIGHT_DRIVE, ON_DRIVE, PULL, SWEEP, LEG_GLANCE, DEFENCE]


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

def _loopclip(name, keys, speed=0.0):
    return {'name': name, 'keys': keys, 'loop': True, 'speed': speed}


def _oneshot(name, keys, event=None, contact=0):
    return {'name': name, 'keys': keys, 'loop': False, 'event': event, 'contact': contact}


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
        _loopclip('KeeperReady', [(0, KEEPER_READY), (34, KEEPER_RISE), (70, KEEPER_READY)]),
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
                                    (64, BATTER_READY_R)]),
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
    for action in (PICKUP, THROW, CATCH, KEEPER_RECEIVE):
        clips.append(_oneshot(action['name'], action['keys'], action['event'], action['contact']))

    # Mirror the batter-specific loops and one-shots.
    clips.append(_loopclip('BatterReady_L', _mirror_keys(
        [(0, BATTER_READY_R), (30, BATTER_READY_TAP), (64, BATTER_READY_R)])))
    clips.append(_loopclip('BatterRun_L', _mirror_keys(BATTER_RUN_R), 300.0))
    clips.append(_oneshot('BatterCelebrate_L', _mirror_keys(BATTER_CELEBRATE_R)))

    for shot in SHOTS:
        clips.append(_oneshot(f"{shot['name']}_R", shot['keys'], shot['event'], shot['contact']))
        clips.append(_oneshot(f"{shot['name']}_L", _mirror_keys(shot['keys']),
                              shot['event'], shot['contact']))
    for delivery in DELIVERIES:
        clips.append(_oneshot(f"{delivery['name']}_R", delivery['keys'],
                              delivery['event'], delivery['contact']))
        clips.append(_oneshot(f"{delivery['name']}_L", _mirror_keys(delivery['keys']),
                              delivery['event'], delivery['contact']))
    return clips
