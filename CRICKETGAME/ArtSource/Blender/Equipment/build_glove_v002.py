"""Round-2 batting gloves, authored around the measured Round-1 grip.

Glove local frame (unchanged from v001): origin at the palm, +Z wrist->fingertip,
+X the back of the hand. Finger polylines come from Artifacts/CharacterAudit/
grip-measure.json (posed BATTER_READY_R joints in this frame), so the casings
wrap the real curled fingers instead of a generic fist arc.

Construction (per hand):
  palm shell   - shaped oval loft, dorsal half in GlovePad, palm half in
                 GlovePalm, piping cords covering the side seams
  knuckle bar  - transverse protective roll across the four knuckles
  fingers      - one oval-section casing per finger on its measured polyline,
                 3 padded segments with grooves at the joints, rounded tip caps,
                 staggered break heights, tan piping in the inter-finger valleys
  thumb        - two-segment casing + outer moulded guard wing
  palm pads    - thenar pad + heel palm bar (GlovePalm)
  cuff         - flared cuff with elastic ridges + wrist strap (PadStrap slot)

Slot names are byte-identical to v001 so runtime Dress() keeps working.
Longest dimension is asserted <= 20.45 cm (ImportEquipment gate is 19.0 +- 1.5).
"""
import json
import math
import sys
from pathlib import Path

from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
from c26_build import Builder, material

PALM, PAD, CUFF, BAND = 0, 1, 2, 3
MEASURE = json.loads((ROOT / 'Artifacts/CharacterAudit/grip-measure.json').read_text())

# per-finger casing radii (across, front-back) in cm: skin ~0.8-0.9 + padding.
FINGER_RADII = {'index': (1.00, 1.16), 'middle': (1.04, 1.20),
                'ring': (0.94, 1.10), 'pinky': (0.84, 0.98)}
THUMB_RADII = (1.18, 1.30)
TIP_PAD = 0.60


def _resample(poly, n=26):
    """Evenly resample a polyline (cm Vectors) with light smoothing."""
    pts = [Vector(p) for p in poly]
    lengths = [0.0]
    for a, b in zip(pts, pts[1:]):
        lengths.append(lengths[-1] + (b - a).length)
    total = lengths[-1]
    out = []
    j = 0
    for i in range(n):
        s = total * i / (n - 1)
        while j < len(lengths) - 2 and lengths[j + 1] < s:
            j += 1
        t = (s - lengths[j]) / max(lengths[j + 1] - lengths[j], 1e-9)
        out.append(pts[j].lerp(pts[j + 1], max(0.0, min(1.0, t))))
    # one smoothing pass, endpoints pinned
    sm = [out[0]]
    for i in range(1, n - 1):
        sm.append((out[i - 1] + out[i] * 2.0 + out[i + 1]) * 0.25)
    sm.append(out[-1])
    return sm


def _frames(path):
    """Parallel-transport frames along a path. Returns (tangents, across, front)."""
    n = len(path)
    tangents = []
    for i in range(n):
        a = path[max(i - 1, 0)]
        b = path[min(i + 1, n - 1)]
        t = (b - a)
        tangents.append(t.normalized() if t.length > 1e-9 else Vector((0, 0, 1)))
    across = []
    ref = Vector((0, 1, 0))
    for t in tangents:
        a = ref - t * ref.dot(t)
        if a.length < 1e-6:
            a = Vector((1, 0, 0)) - t * t.x
        a.normalize()
        across.append(a)
        ref = a
    front = [t.cross(a).normalized() for t, a in zip(tangents, across)]
    return tangents, across, front


def _oval_ring(p, across, front, ra, rf, sides=8, phase=0.0):
    pts = []
    for j in range(sides):
        a = 2 * math.pi * j / sides + phase
        pts.append(tuple(p + across * (math.cos(a) * ra) + front * (math.sin(a) * rf)))
    return pts


def finger_casing(b, poly, radii, mat, groove_stations=(0.45, 0.72), tip_pad=TIP_PAD):
    """Sweep an oval casing along poly with padded segments and a rounded tip."""
    path = _resample(poly, 24)
    # extend past the tip for padding, continuing the end tangent
    end_t = (path[-1] - path[-3]).normalized()
    path = path + [path[-1] + end_t * tip_pad * 0.5, path[-1] + end_t * tip_pad]
    _, across, front = _frames(path)
    ra0, rf0 = radii
    rings = []
    m = len(path)
    for i, p in enumerate(path):
        t = i / (m - 1)
        # base flare where the casing dives into the palm shell
        flare = 1.0 + 0.28 * max(0.0, 1.0 - t / 0.16)
        # grooves at the joint stations
        g = 1.0
        for gs in groove_stations:
            g -= 0.20 * max(0.0, 1.0 - abs(t - gs) / 0.045)
        # taper into the rounded tip
        taper = 1.0 - 0.30 * max(0.0, (t - 0.90) / 0.10)
        rings.append(_oval_ring(p, across[i], front[i], ra0 * flare * g * taper,
                                rf0 * flare * g * taper))
    b.loft(rings, mat)
    # rounded tip cap: two shrinking rings + apex
    tip, et = path[-1], (path[-1] - path[-4]).normalized()
    _, ac2, fr2 = _frames([path[-3], path[-1], path[-1] + et])
    r1 = _oval_ring(tip + et * tip_pad * 0.28, ac2[1], fr2[1], ra0 * 0.52, rf0 * 0.52)
    b.loft([rings[-1], r1], mat)
    b.loft([r1], mat, cap_end=tuple(tip + et * (tip_pad * 0.28 + ra0 * 0.55)))
    return path, across, front


def finger_piping(b, path, across, front, ra, rf, mat, side_sign):
    """A tan seam cord down one side of a finger casing."""
    pts = []
    for i, p in enumerate(path[:-2]):
        t = i / (len(path) - 1)
        g = 1.0
        for gs in (0.45, 0.72):
            g -= 0.20 * max(0.0, 1.0 - abs(t - gs) / 0.045)
        pts.append(tuple(Vector(p) + across[i] * (ra * g * side_sign) + front[i] * (rf * 0.25 * g)))
    from c26_build import tube
    thin = pts[::2] + [pts[-1]]
    b.loft(tube(thin, [0.15] * len(thin), 4), mat, cap_start=thin[0], cap_end=thin[-1])


def build_glove(name, side_key, mirror):
    """mirror: +1 right hand data as measured, -1 left. Geometry is authored per
    hand from that hand's own measured polylines (no mirroring of shapes)."""
    g = MEASURE[side_key]
    mats = [material('M_C26_GlovePalm', (0.520, 0.470, 0.400), 0.66),
            material('M_C26_GlovePad', (0.780, 0.788, 0.760), 0.70),
            material('M_C26_GloveCuff', (0.700, 0.710, 0.686), 0.78),
            material('M_C26_PadStrap', (0.045, 0.048, 0.055), 0.74)]
    b = Builder()
    fingers = g['fingers']

    knuckle = {f: Vector(fingers[f]['poly'][1]) for f in ('index', 'middle', 'ring', 'pinky')}
    wrist = Vector(g['wrist_local'])

    # ---- palm shell: cuff top (-5.5) to just above the knuckles (+3.2), where the
    # fat finger-casing bases overlap each other and seal the opening.
    # (z, cx, rx, ry): centre-x drifts palm-ward through the swell.
    shell_profile = [(-5.5, 0.1, 2.55, 3.45), (-4.0, -0.1, 2.75, 3.75),
                     (-2.0, -0.35, 3.05, 4.10), (0.0, -0.45, 3.20, 4.30),
                     (1.8, -0.40, 3.15, 4.30), (3.2, -0.10, 2.90, 4.00)]
    # Scalloped mouth: the top edge dips at the sides so the outer fingers
    # emerge OVER the edge instead of piercing the shell wall.
    top_dip = [0.0, 0.0, 0.0, 0.0, 1.3, 2.6]
    for half, mat in (('dorsal', PAD), ('palm', PALM)):
        rings = []
        for ri, (z, cx, rx, ry) in enumerate(shell_profile):
            ring = []
            steps = 8
            for j in range(steps):
                if half == 'dorsal':
                    a = math.radians(-80 + 160 * j / (steps - 1))
                else:
                    a = math.radians(80 + 200 * j / (steps - 1))
                y = math.sin(a) * ry
                zz = z - top_dip[ri] * (abs(y) / ry) ** 1.5
                ring.append((cx + math.cos(a) * rx, y, zz))
            rings.append(ring)
        b.loft(rings, mat, closed=False)
    # side seam piping covers the open shell edges and reads as construction.
    from c26_build import tube
    for sgn in (1, -1):
        seam = []
        for ri, (z, cx, rx, ry) in enumerate(shell_profile):
            a = math.radians(80 * sgn)
            y = math.sin(a) * (ry + 0.08)
            zz = z - top_dip[ri] * (abs(y) / ry) ** 1.5
            seam.append((cx + math.cos(a) * (rx + 0.08), y, zz))
        b.loft(tube(seam, [0.16] * len(seam), 6), PALM, cap_start=seam[0], cap_end=seam[-1])

    # ---- knuckle bar: transverse protective roll, ends tucked at the sides.
    klist = [knuckle[f] for f in ('index', 'middle', 'ring', 'pinky')]
    klist_sorted = sorted(klist, key=lambda p: p.y, reverse=(mirror > 0))
    bar = []
    for i, k in enumerate(klist_sorted):
        bar.append(tuple(k + Vector((1.15, 0, 0.55))))
    bar_path = _resample(bar, 10)
    b.loft(tube(bar_path, [0.95 - 0.15 * abs(i / 9 - 0.5) * 2 for i in range(10)], 7),
           PAD, cap_start=bar_path[0], cap_end=bar_path[-1])

    # ---- finger casings on the measured polylines, starting inside the shell.
    # Groove stations are derived per finger from the measured joint positions
    # (PIP/DIP), so the breaks sit on the knuckles instead of at fixed heights.
    for f in ('index', 'middle', 'ring', 'pinky'):
        poly = [Vector(p) for p in fingers[f]['poly']]
        start = poly[0].lerp(poly[1], 0.55)
        casing_poly = [start] + poly[1:]
        cum, total = [0.0], 0.0
        for a, c in zip(casing_poly, casing_poly[1:]):
            total += (c - a).length
            cum.append(total)
        joints = [cum[2] / total, cum[3] / total]  # PIP, DIP as fractions
        path, across, front = finger_casing(
            b, casing_poly, FINGER_RADII[f], PAD, groove_stations=tuple(joints))
        ra, rf = FINGER_RADII[f]
        finger_piping(b, path, across, front, ra, rf, PALM, +1)
        finger_piping(b, path, across, front, ra, rf, PALM, -1)

    # ---- thumb casing + outer guard wing. The casing starts at the thumb base
    # with a wide flare so it merges into the shell/thenar instead of floating.
    tpoly = [Vector(p) for p in fingers['thumb']['poly']]
    tpath, tacross, tfront = finger_casing(
        b, tpoly, THUMB_RADII, PAD,
        groove_stations=(0.42, 0.66), tip_pad=0.60)
    # guard wing: flat slab on the outer (dorsal-radial) side of the thumb base.
    mid = tpath[len(tpath) // 3]
    wing = []
    for i in range(7):
        t = i / 6.0
        c = tpath[int(t * (len(tpath) // 2))]
        wing.append(tuple(c + tacross[int(t * (len(tpath) // 2))] * (1.35 + 0.25 * math.sin(t * math.pi))
                          + tfront[int(t * (len(tpath) // 2))] * 0.55))
    b.loft(tube(wing, [0.62 - 0.18 * t for t in [i / 6.0 for i in range(7)]], 7),
           PAD, cap_start=wing[0], cap_end=wing[-1])

    # ---- palm pads: thenar swell (thumb side, +Y on both measured hands) + heel bar.
    thenar_c = Vector(knuckle['index']).lerp(wrist, 0.45) + Vector((-1.5, 1.7, 0))
    b.loft(tube([tuple(thenar_c + Vector((0, 0, z))) for z in (-3.2, -1.0, 1.5)],
                [1.6, 1.9, 1.4], 8), PALM,
            cap_start=tuple(thenar_c + Vector((0, 0, -3.2))),
            cap_end=tuple(thenar_c + Vector((0, 0, 1.5))))
    heel_c = wrist + Vector((0.3, 0, 1.6))
    b.loft(tube([tuple(heel_c + Vector((0, dy, 0))) for dy in (-2.6, 0, 2.6)],
                [1.15, 1.35, 1.15], 8), PALM,
            cap_start=tuple(heel_c + Vector((0, -2.6, 0))),
            cap_end=tuple(heel_c + Vector((0, 2.6, 0))))

    # ---- cuff: flared, ribbed, with wrist strap.
    cuff_profile = [(-8.65, 3.30, 4.05), (-7.6, 3.10, 3.85), (-6.4, 2.85, 3.60),
                    (-5.4, 2.70, 3.50)]
    rings = []
    for z, rx, ry in cuff_profile:
        rings.append([(math.sin(2 * math.pi * j / 12) * rx,
                       math.cos(2 * math.pi * j / 12) * ry, z) for j in range(12)])
    b.loft(rings, CUFF)
    for z, lift in ((-7.95, 0.16), (-7.0, 0.16), (-6.05, 0.14)):
        ridge = [(math.sin(2 * math.pi * j / 12) * (2.95 + lift),
                  math.cos(2 * math.pi * j / 12) * (3.70 + lift), z) for j in range(12)]
        nxt = [(math.sin(2 * math.pi * j / 12) * (2.95 + lift),
                math.cos(2 * math.pi * j / 12) * (3.70 + lift), z + 0.42) for j in range(12)]
        b.loft([ridge, nxt], CUFF)
    # wrist strap across the back (+X) of the wrist, seated on the cuff.
    strap = [(x, y, -6.9) for x, y in ((2.95, -2.6), (3.25, 0.0), (2.95, 2.6))]
    b.loft(tube([Vector(p) for p in strap], [0.55, 0.62, 0.55], 7), BAND,
           cap_start=strap[0], cap_end=strap[-1])

    obj = b.build(name, mats, smooth_angle=36.0)
    # gate: longest dimension must stay within the 19.0 +- 1.5 cm import check.
    longest = max(obj.dimensions) / 0.01
    assert longest <= 20.45, (name, longest)
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('GLOVE_V002 %-16s longest=%.2fcm verts=%d tris=%d' % (name, longest,
                                                                len(obj.data.vertices), tris))
    return obj
