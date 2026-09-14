"""Round-2 cricket shoe: sculpted last, real collar, tongue, seated laces, studs.

Local frame (unchanged from v001): origin at the ankle joint, toes +X, up +Z.
Improvements over v001, all in the same frame and size envelope:
  - rounded toe box with overlay toe cap (no blunt wedge)
  - sculpted collar: padded sides, dipped throat + Achilles, dark inner lining
  - tongue slab under the throat, eyestay strips, 10 metal eyelets
  - 5 cross laces seated in a throat groove + tied bow (no floating staples)
  - heel counter overlay + pull tab
  - flash on both quarters (outside swoosh + inside dash)
  - midsole with arch recess, heel bevel and toe spring; welt cord at the joint
  - full-length outsole plate with 7 cricket studs (forefoot blades + conicals)
Slot names are byte-identical to v001 so UE material mapping keeps working.
Longest dimension is asserted within 27.5 +- 1.5 cm.
"""
import math
import sys
from pathlib import Path

from mathutils import Vector

sys.path.insert(0, str(Path(__file__).parent))
from c26_build import Builder, circle, material, tube

UPPER, SOLE, FLASH, LACE, SPIKE = 0, 1, 2, 3, 4

# x along foot, half-width, sole height, vamp (top) height
LAST = [(-8.6, 2.60, 0.0, 8.2), (-7.0, 3.60, 0.0, 8.8), (-5.0, 4.20, 0.0, 8.2),
        (-2.6, 4.50, 0.1, 7.0), (0.0, 4.60, 0.2, 6.0), (3.0, 4.65, 0.3, 4.9),
        (6.4, 4.55, 0.4, 4.0), (9.6, 4.25, 0.5, 3.3), (12.4, 3.75, 0.6, 2.8),
        (14.8, 3.05, 0.7, 2.4), (16.6, 2.20, 0.85, 2.0), (17.8, 1.30, 1.0, 1.7)]


def _throat_depth(x):
    """Lace-throat groove depth: the channel the cross laces sit in."""
    if -3.5 <= x <= 5.0:
        return 0.55 * max(0.0, 1.0 - ((x - 0.75) / 4.5) ** 2)
    return 0.0


def _collar_dip(x):
    """Top-line dip for the foot entry: throat hollow + Achilles hollow."""
    dip = 1.7 * max(0.0, 1.0 - ((x + 2.2) / 3.0) ** 2)
    dip += 2.3 * max(0.0, 1.0 - ((x + 8.0) / 2.6) ** 2)
    return dip


def _ring(x, w, z0, z1, sides=14, sculpt=True):
    """Section: flat sole, domed vamp; near the ankle the top line is sculpted
    into a collar (padded sides, dipped throat and Achilles)."""
    pts = []
    for j in range(sides):
        a = 2 * math.pi * j / sides
        c, s = math.cos(a), math.sin(a)
        y = w * math.copysign(abs(c) ** 0.70, c) if c else 0.0
        sy = math.copysign(abs(s) ** 0.70, s) if s else 0.0
        z = (z0 + z1) * 0.5 + (z1 - z0) * 0.5 * sy
        if sculpt:
            topness = max(0.0, sy) ** 2
            sideness = 1.0 - abs(c)
            z -= _collar_dip(x) * topness
            z += 0.45 * topness * sideness * max(0.0, 1.0 - ((x + 5.5) / 4.5) ** 2)
            z -= _throat_depth(x) * topness * max(0.0, 1.0 - (abs(y) / 1.4) ** 2)
        pts.append((x, y, z))
    return pts


def _last_width(x):
    for i in range(len(LAST) - 1):
        if LAST[i][0] <= x <= LAST[i + 1][0]:
            t = (x - LAST[i][0]) / (LAST[i + 1][0] - LAST[i][0])
            return LAST[i][1] + (LAST[i + 1][1] - LAST[i][1]) * t
    return LAST[0][1] if x < LAST[0][0] else LAST[-1][1]


def _vamp_top(x):
    for i in range(len(LAST) - 1):
        if LAST[i][0] <= x <= LAST[i + 1][0]:
            t = (x - LAST[i][0]) / (LAST[i + 1][0] - LAST[i][0])
            return LAST[i][3] + (LAST[i + 1][3] - LAST[i][3]) * t - _collar_dip(x)
    return LAST[-1][3]


def build(name, side=1):
    mats = [material('M_C26_ShoeUpper', (0.790, 0.800, 0.782), 0.52),
            material('M_C26_ShoeSole', (0.130, 0.136, 0.148), 0.72),
            material('M_C26_ShoeFlash', (0.055, 0.300, 0.345), 0.44),
            material('M_C26_ShoeLace', (0.660, 0.668, 0.650), 0.86),
            material('M_C26_ShoeSpike', (0.400, 0.410, 0.430), 0.30, metallic=0.8)]
    b = Builder()
    rings = [_ring(x, w, sole + 0.9, up) for x, w, sole, up in LAST]
    rings.insert(0, _ring(-9.2, 1.90, 1.9, 7.0))
    rings.append(_ring(18.6, 0.70, 1.5, 1.9))
    b.loft(rings, UPPER, cap_start=(-9.6, 0, 4.4), cap_end=(18.9, 0, 1.7), v_scale=2.0)

    # Toe cap overlay: a proud second skin over the front third.
    cap = []
    for x in (7.6, 10.0, 12.4, 14.8, 16.6, 17.8):
        w = _last_width(x)
        ring = []
        for j in range(12):
            a = math.pi * j / 11  # over the top, edge to edge
            y = -w * math.cos(a) * 0.99
            z = 0.9 + (_vamp_top(x) - 0.4) * math.sin(a) ** 0.8 + 0.14
            ring.append((x, y, z))
        cap.append(ring)
    b.loft(cap, UPPER, closed=False)

    # Heel counter overlay wrapping the back.
    counter = []
    for x in (-9.0, -7.6, -6.2, -5.0):
        w = _last_width(x) + 0.12
        ring = []
        for j in range(12):
            a = math.pi * j / 11
            y = -w * math.cos(a)
            z = 1.0 + (_vamp_top(x) - 0.6) * math.sin(a) ** 0.7 + 0.12
            ring.append((x, y, z))
        counter.append(ring)
    b.loft(counter, UPPER, closed=False)

    # Padded collar roll following the sculpted top edge + dark inner lining.
    edge = []
    for x in (-8.8, -7.6, -6.4, -5.2, -4.0, -2.8):
        w = _last_width(x)
        edge.append((x, side * (w * 0.55), _vamp_top(x) + 0.55))
        edge.append((x, -side * (w * 0.55), _vamp_top(x) + 0.55))
    b.loft(tube([Vector(p) for p in edge[::2]] + [Vector(p) for p in edge[1::2]],
                [0.55] * len(edge), 7), UPPER,
           cap_start=edge[0], cap_end=edge[-1])
    b.quad((-8.2, -2.2, 6.2), (-8.2, 2.2, 6.2), (-3.4, 1.6, 4.6), (-3.4, -1.6, 4.6), SOLE)

    # Heel pull tab.
    b.loft(tube([Vector((-9.0, 0, 7.2)), Vector((-9.5, 0, 8.6)), Vector((-9.1, 0, 9.4))],
                [0.34, 0.34, 0.30], 6), FLASH, cap_end=(-9.1, 0, 9.4))

    # Tongue slab under the throat, top edge standing proud of the top lace.
    tongue = []
    for x, z, w in ((-5.0, 4.0, 1.9), (-3.6, 4.9, 1.7), (-2.6, 5.6, 1.4), (-2.0, 6.1, 1.2)):
        tongue.append([(x - 0.5, -w, z - 0.35), (x - 0.5, w, z - 0.35),
                       (x + 0.5, w, z + 0.35), (x + 0.5, -w, z + 0.35)])
    for q in tongue:
        b.quad(*q, UPPER)

    # Eyestay strips + metal eyelets + seated cross laces + bow.
    for eside in (1, -1):
        strip = []
        for x in (-3.2, -1.4, 0.4, 2.2, 4.0):
            w = 2.05 - (x + 3.2) * 0.06
            strip.append((x, eside * w, _vamp_top(x) + 0.28))
        b.loft(tube([Vector(p) for p in strip], [0.42] * len(strip), 6), UPPER,
               cap_start=strip[0], cap_end=strip[-1])
        for x in (-3.2, -1.4, 0.4, 2.2, 4.0):
            w = 2.05 - (x + 3.2) * 0.06
            z = _vamp_top(x) + 0.28
            eye = [(x + 0.34 * math.cos(2 * math.pi * j / 6), eside * (w + 0.34 * math.sin(2 * math.pi * j / 6)), z)
                   for j in range(6)]
            b.loft(tube([Vector(p) for p in eye], [0.13] * 6, 4), SPIKE)
    for i, x in enumerate((-3.0, -1.2, 0.6, 2.4, 4.0)):
        z = _vamp_top(x) - _throat_depth(x) + 0.35 - i * 0.02
        w = 2.0 - (x + 3.0) * 0.06
        b.loft(tube([Vector((x - 0.3, -w, z)), Vector((x, 0, z + 0.28)), Vector((x + 0.3, w, z))],
                    [0.30, 0.30, 0.30], 6), LACE)
    knot = (-3.1, 0, 5.95)
    b.loft(tube([Vector((-3.8, 0, 5.7)), Vector(knot), Vector((-2.4, 0, 5.7))],
                [0.30, 0.42, 0.30], 6), LACE, cap_start=(-3.8, 0, 5.7), cap_end=(-2.4, 0, 5.7))
    for lsgn in (1, -1):
        b.loft(tube([Vector(knot), Vector((-4.0, lsgn * 1.3, 6.05)), Vector((-5.0, lsgn * 2.1, 5.7))],
                    [0.28, 0.24, 0.18], 6), LACE, cap_end=(-5.0, lsgn * 2.1, 5.7))

    # Quarter flash, both sides: outside swoosh + inside dash.
    for fside, x0, x1, lift in ((side, -3.4, 10.2, 1.0), (-side, -2.0, 6.0, 0.7)):
        for i in range(6):
            t0, t1 = i / 6.0, (i + 1) / 6.0

            def pt(t, dz, _x0=x0, _x1=x1, _lift=lift, _fside=fside):
                x = _x0 + t * (_x1 - _x0)
                z = 2.15 + math.sin(t * math.pi) * 1.7 * _lift + dz
                w = _last_width(x) * (1.0 - 0.030 * max(0.0, z - 1.9)) + 0.10
                return (x, _fside * w, z)
            b.quad(pt(t0, -0.70), pt(t1, -0.70), pt(t1, 0.70), pt(t0, 0.70), FLASH)

    # Midsole slab with arch recess, heel bevel and toe spring.
    sole_rings = []
    for x, w, sole, up in LAST:
        lift = 0.0
        if x > 12.0:
            lift = (x - 12.0) * 0.10  # toe spring
        if x < -5.0:
            lift = (-5.0 - x) * 0.06  # heel bevel
        sole_rings.append([(p[0], p[1] * 1.05, min(p[2], 1.05) + lift)
                           for p in _ring(x, w, 0.0, 1.9, sculpt=False)])
    b.loft(sole_rings, SOLE, cap_start=(-8.8, 0, 1.0), cap_end=(18.2, 0, 2.0))

    # Welt cord around the upper/midsole joint.
    for wside in (1, -1):
        welt = [(x, wside * (_last_width(x) * 1.02 + 0.06), 1.15 + (0.10 if x > 12 else 0.0))
                for x, _, _, _ in LAST]
        b.loft(tube([Vector(p) for p in welt], [0.14] * len(welt), 5), LACE)

    # Outsole plate + 7 cricket studs: bladed forefoot pair, conical rest.
    plate = []
    for x, w, sole, up in LAST:
        lift = (x - 12.0) * 0.10 if x > 12.0 else ((-5.0 - x) * 0.06 if x < -5.0 else 0.0)
        plate.append([(p[0], p[1] * 0.99, min(p[2], 0.42) + lift - 0.12)
                      for p in _ring(x, w * 0.97, 0.0, 1.2, sides=12, sculpt=False)])
    b.loft(plate, SOLE, cap_start=(-8.6, 0, 0.35), cap_end=(17.8, 0, 1.15))
    studs = [((12.6, 0.55), 'blade'), ((9.8, -0.68), 'blade'),
             ((6.2, 0.72), 'cone'), ((2.6, -0.70), 'cone'),
             ((-3.2, 0.60), 'cone'), ((-5.6, -0.58), 'cone'), ((-4.4, 0.0), 'cone')]
    for (x, yf), kind in studs:
        w = _last_width(x)
        cy = yf * w
        base = -0.05 + ((x - 12.0) * 0.10 if x > 12.0 else 0.0)
        if kind == 'blade':
            b.loft(tube([Vector((x - 0.55, cy, base)), Vector((x + 0.55, cy, base - 0.75))],
                        [0.42, 0.30], 6), SPIKE, cap_end=(x + 0.55, cy, base - 0.75))
        else:
            rings = [circle(x, cy, base, 0.44, 8), circle(x, cy, base - 0.85, 0.20, 8)]
            b.loft([[(p[0], p[1], p[2]) for p in r] for r in rings], SPIKE,
                   cap_end=(x, cy, base - 1.05))

    obj = b.build(name, mats, smooth_angle=32.0)
    longest = max(obj.dimensions) / 0.01
    assert 26.0 <= longest <= 29.0, (name, longest)
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('SHOE_V002 %-16s longest=%.2fcm verts=%d tris=%d' % (name, longest,
                                                               len(obj.data.vertices), tris))
    return obj
