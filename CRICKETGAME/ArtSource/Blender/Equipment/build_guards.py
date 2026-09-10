"""Batting pads and batting gloves.

Pad local frame: origin mid-shin, +Z up the shin, +X out of the front of the leg.
Glove local frame: origin at the palm, +Z wrist -> fingertip, +X the back of the hand.
Both match the frames AC26Athlete::PlaceKit already poses them in.

The pad the procedural code drew was a tube with a bulge. What makes a cricket pad recognisable
is the horizontal banding: three vertical bolsters down the face, a knee roll, and side wings
that carry past the widest part of the leg so the pad still reads from behind the striker --
which is the angle the gameplay camera actually watches him from.
"""
import math
from c26_build import Builder, material, tube

FACE, ROLL, STRAP, BUCKLE = 0, 1, 2, 3
PALM, PAD, CUFF, BAND = 0, 1, 2, 3

# z (cm along the shin), half-width, depth out the front, depth behind the calf
PAD_PROFILE = [(-25.0, 6.6, 7.2, 5.4), (-21.5, 8.6, 8.8, 6.6), (-17.0, 10.0, 9.6, 7.4),
               (-11.5, 10.7, 9.9, 7.8), (-5.5, 11.1, 10.1, 8.2), (0.5, 11.4, 10.2, 8.5),
               (6.0, 11.7, 10.4, 8.7), (10.5, 12.1, 11.4, 8.9), (14.0, 12.4, 12.0, 8.8),
               (17.5, 12.2, 11.5, 8.4), (20.0, 11.2, 10.6, 7.8), (22.0, 10.2, 10.0, 7.2)]
BOLSTERS = ((0.0, 2.85, 2.60), (-7.0, 2.50, 2.15), (7.0, 2.50, 2.15))


def _pad_ring(z, w, front, back, knee=0.0, sides=26):
    pts = []
    half, mid = (front + back) * 0.5, (front - back) * 0.5
    for j in range(sides):
        a = 2 * math.pi * j / sides
        c, s = math.cos(a), math.sin(a)
        y = w * math.copysign(abs(c) ** 0.62, c) if c else 0.0
        sy = math.copysign(abs(s) ** 0.62, s) if s else 0.0
        x = mid + half * sy
        if sy > 0.0:                       # front face only: bolsters and knee roll
            lift = 0.0
            for cy, hw, h in BOLSTERS:
                lift += h * max(0.0, 1.0 - ((y - cy) / hw) ** 2) ** 0.75
            x += (lift + knee) * (sy ** 0.55)
        pts.append((x, y, z))
    return pts


def build_pad(name, side=1):
    mats = [material('M_C26_PadFace', (0.760, 0.772, 0.740), 0.72),
            material('M_C26_PadRoll', (0.700, 0.712, 0.684), 0.80),
            material('M_C26_PadStrap', (0.045, 0.048, 0.055), 0.74),
            material('M_C26_PadBuckle', (0.420, 0.430, 0.450), 0.36, metallic=0.8)]
    b = Builder()
    rings = []
    for z, w, front, back in PAD_PROFILE:
        # Knee roll: a horizontal swell where the pad passes the knee, tallest at z=14.
        knee = 1.5 * max(0.0, 1.0 - ((z - 14.0) / 6.0) ** 2) ** 0.7
        rings.append(_pad_ring(z, w, front, back, knee))
    # Round both ends off. A single apex cap cones the pad to a visible point at the knee.
    rings.insert(0, _pad_ring(-27.0, 4.9, 5.6, 4.0))
    rings.append(_pad_ring(23.4, 8.4, 8.4, 6.0))
    b.loft(rings, FACE, cap_start=(0.5, 0, -28.2), cap_end=(0.4, 0, 24.4), v_scale=2.0)

    # Side wings: flaps standing proud of the widest part of the leg, so the pad reads in
    # silhouette from behind and from square.
    for wing in (-1, 1):
        cols = []
        for z, w, front, back in PAD_PROFILE[2:10]:
            inner = (mid_x(front, back), wing * (w - 0.2), z)
            outer = (mid_x(front, back) - 1.4, wing * (w + 2.5), z - 0.2)
            cols.append((inner, outer))
        for i in range(len(cols) - 1):
            (a, b2), (c, d) = cols[i], cols[i + 1]
            for lift in (0.55, -0.55):
                q = [(p[0] + lift, p[1], p[2]) for p in (a, b2, d, c)]
                b.quad(*q, ROLL) if (wing > 0) == (lift > 0) else b.quad(q[1], q[0], q[3], q[2], ROLL)
            b.quad((b2[0] + 0.55, b2[1], b2[2]), (b2[0] - 0.55, b2[1], b2[2]),
                   (d[0] - 0.55, d[1], d[2]), (d[0] + 0.55, d[1], d[2]), ROLL)

    # Three straps around the back with buckles on the outside, the way a pad is actually worn.
    # Each strap ring carries the same bolster and knee-roll lift as the body it sits on, plus a
    # margin: built flat, the top strap sank straight into the knee roll and only showed where
    # the pad happened to be thin.
    for z in (-19.0, -4.0, 12.0):
        prof = min(PAD_PROFILE, key=lambda q: abs(q[0] - z))
        w, front, back = prof[1], prof[2], prof[3]
        rings = []
        for dz in (-1.15, 1.15):
            knee = 1.5 * max(0.0, 1.0 - ((z + dz - 14.0) / 6.0) ** 2) ** 0.7
            rings.append(_pad_ring(z + dz, w + 0.45, front + 0.40, back + 0.45, knee, sides=26))
        b.loft(rings, STRAP)
        bx, by = mid_x(front, back) - 1.4, side * (w + 0.6)
        b.loft([[(bx - 1.4, by, z - 1.4), (bx + 1.4, by, z - 1.4),
                 (bx + 1.4, by, z + 1.4), (bx - 1.4, by, z + 1.4)],
                [(bx - 1.4, by + side * 0.8, z - 1.4), (bx + 1.4, by + side * 0.8, z - 1.4),
                 (bx + 1.4, by + side * 0.8, z + 1.4), (bx - 1.4, by + side * 0.8, z + 1.4)]],
               BUCKLE, cap_start=(bx, by, z), cap_end=(bx, by + side * 0.8, z))

    # Instep flap over the top of the shoe.
    b.loft([_pad_ring(-25.0, 6.6, 7.2, 5.4), _pad_ring(-27.6, 5.4, 6.2, 4.2)], ROLL,
           cap_end=(0.6, 0, -29.2))  # instep flap over the top of the shoe
    return b.build(name, mats, smooth_angle=27.0)


def mid_x(front, back):
    return (front - back) * 0.5


def build_glove(name, hand=1):
    """hand: +1 right, -1 left. Fingers splay away from the thumb side."""
    mats = [material('M_C26_GlovePalm', (0.520, 0.470, 0.400), 0.66),
            material('M_C26_GlovePad', (0.780, 0.788, 0.760), 0.70),
            material('M_C26_GloveCuff', (0.700, 0.710, 0.686), 0.78),
            material('M_C26_PadStrap', (0.045, 0.048, 0.055), 0.74)]
    b = Builder()

    # Cuff mouth -> wrist waist -> the swell of a closed fist, oval in section because a hand is
    # not round: wide across the knuckles, shallow front to back.
    body = []
    for z, rx, ry in ((-10.6, 4.20, 4.85), (-8.0, 3.85, 4.55), (-5.6, 3.45, 4.15),
                      (-2.4, 3.85, 4.60), (0.8, 4.10, 4.90), (3.2, 3.80, 4.55), (4.8, 3.00, 3.55)):
        body.append([(math.sin(2 * math.pi * j / 12) * rx, math.cos(2 * math.pi * j / 12) * ry * hand, z)
                     for j in range(12)])
    b.loft(body, PALM, cap_start=(0, 0, -11.6), cap_end=(0.4, 0, 5.8))

    # Cuff band and wrist strap.
    b.loft([[(v[0] * 1.06, v[1] * 1.06, -10.9) for v in body[0]],
            [(v[0] * 1.10, v[1] * 1.10, -8.4) for v in body[0]]], CUFF)
    b.loft([[(v[0] * 1.12, v[1] * 1.12, -7.6) for v in body[1]],
            [(v[0] * 1.12, v[1] * 1.12, -6.0) for v in body[1]]], BAND)

    # Four fingers, each three sausage segments arcing over the knuckles and down the palm side.
    # Segmented rolls are the single thing that stops a batting glove reading as one white lump.
    for finger in range(4):
        y = hand * ((finger - 1.5) * 2.55)
        length = 3.85 - 0.30 * max(0, finger - 2)
        base = 1.34 - finger * 0.06
        path, rad = [], []
        steps = 13
        for step in range(steps):
            t = step / (steps - 1.0)
            ang = math.radians(80 - t * 162)
            path.append((0.35 + math.sin(ang) * length, y, 2.55 + math.cos(ang) * length))
            # One tube per finger with the radius waisted three times, rather than three tubes
            # butted together: separate segments left flat end caps z-fighting into each other
            # and threw stray spikes wherever two caps met.
            waist = 0.86 + 0.14 * math.cos(t * 3.0 * 2 * math.pi)
            rad.append(base * waist * (1.0 - 0.14 * max(0.0, t - 0.82) / 0.18))
        b.loft(tube(path, rad, 8), PAD, cap_start=path[0], cap_end=path[-1])

    # Thumb: two segments up the side, with the large moulded guard a batting glove carries.
    thumb, trad = [], []
    for step in range(9):
        t = step / 8.0
        thumb.append((0.7 + t * 3.5, hand * (3.7 + math.sin(t * math.pi) * 0.9 - t * 1.3),
                      -4.0 + t * 7.6))
        trad.append(1.90 * (1.0 - 0.38 * t) * (0.90 + 0.10 * math.cos(t * 2.0 * 2 * math.pi)))
    b.loft(tube(thumb, trad, 9), PAD, cap_start=thumb[0], cap_end=thumb[-1])
    return b.build(name, mats, smooth_angle=36.0)
