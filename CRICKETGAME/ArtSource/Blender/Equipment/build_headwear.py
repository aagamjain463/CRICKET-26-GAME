"""Cricket helmet, grille and fielder's cap.

Local frame for all three: origin at the skull pivot, +X out of the face, +Z up. That is the
frame AC26Athlete::PlaceKit already builds from the posed Head bone, so these drop onto the
existing head solve without re-deriving it.

The helmet the procedural code drew was an engine sphere plus a squashed sphere peak. This one
has the two things that actually make a cricket helmet recognisable: a rim that sweeps up over
the face and down over the ears and nape, and a peak that is a real downturned visor with
thickness rather than a flattened ball.
"""
import math
from c26_build import Builder, material, circle, tube

SHELL, PEAK, TRIM, PAD, BAR = 0, 1, 2, 3, 4
RX, RY, RZ = 12.1, 11.4, 11.9


def rim_z(a):
    """How low the shell drops at azimuth a (0 = straight out of the face)."""
    rear = (1.0 - math.cos(a)) * 0.5
    return -1.2 - 9.6 * _smooth(0.04, 0.52, rear)


def _smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def shell_point(a, e):
    return (RX * math.cos(e) * math.cos(a), RY * math.cos(e) * math.sin(a), RZ * math.sin(e))


def _dome(b, sides, rings, scale=1.0, mat=SHELL, cut=rim_z):
    """Ellipsoid cap whose lower rim height varies with azimuth, so the opening sweeps up over
    the face instead of being punched out as a flat hole."""
    out = []
    for r in range(rings):
        t = r / (rings - 1.0)
        ring = []
        for j in range(sides):
            a = 2 * math.pi * j / sides
            e0 = math.asin(max(-0.99, min(0.99, cut(a) / RZ)))
            e = e0 + (math.pi / 2 - e0) * (t ** 0.86)
            p = shell_point(a, e)
            ring.append((p[0] * scale, p[1] * scale, p[2] * scale))
        out.append(ring)
    b.loft(out, mat, v_scale=1.0)
    return out[0]


def build_helmet():
    mats = [material('M_C26_HelmetShell', (0.055, 0.300, 0.345), 0.26),
            material('M_C26_HelmetPeak', (0.048, 0.250, 0.290), 0.24),
            material('M_C26_HelmetTrim', (0.030, 0.032, 0.038), 0.62),
            material('M_C26_HelmetPad', (0.052, 0.050, 0.048), 0.88),
            material('M_C26_HelmetBar', (0.360, 0.372, 0.392), 0.34, metallic=0.85)]
    b = Builder()
    sides, rings = 28, 7
    rim = _dome(b, sides, rings)

    # Rolled rim: the outer shell turns under and runs back up as padding. Without it the shell
    # is a zero-thickness surface and every grazing camera angle shows a paper edge.
    lip, inner = [], []
    for j, p in enumerate(rim):
        a = 2 * math.pi * j / sides
        out = (math.cos(a), math.sin(a), 0.0)
        lip.append((p[0] + out[0] * 0.55, p[1] + out[1] * 0.55, p[2] - 0.95))
        inner.append((p[0] * 0.90, p[1] * 0.90, p[2] + 0.55))
    b.loft([rim, lip], TRIM)
    b.loft([lip, inner], PAD)

    # Peak: a downturned visor with real thickness, swept from the brow forward and dropping at
    # the tip, with the sides curling up the way a moulded peak does.
    top, bot = [], []
    cols = 13
    for i in range(6):
        u = i / 5.0
        tr, br = [], []
        for c in range(cols):
            v = c / (cols - 1.0) * 2 - 1
            width = 8.9 * (1 - 0.16 * u * u)
            y = v * width
            x = 8.4 + u * 9.0 * (1 - 0.18 * v * v)
            z = -0.9 - u * u * 3.1 + (v * v) * 1.35 * u
            tr.append((x, y, z + 0.45))
            br.append((x, y, z - 0.45))
        top.append(tr)
        bot.append(br)
    for row in range(5):
        for c in range(cols - 1):
            b.quad(top[row][c], top[row][c + 1], top[row + 1][c + 1], top[row + 1][c], PEAK,
                   (c / cols, row / 5), ((c + 1) / cols, row / 5), ((c + 1) / cols, (row + 1) / 5), (c / cols, (row + 1) / 5))
            b.quad(bot[row][c + 1], bot[row][c], bot[row + 1][c], bot[row + 1][c + 1], PEAK)
    for c in range(cols - 1):  # front edge of the peak
        b.quad(top[5][c], top[5][c + 1], bot[5][c + 1], bot[5][c], TRIM)
    for row in range(5):       # side edges
        b.quad(top[row][0], top[row + 1][0], bot[row + 1][0], bot[row][0], TRIM)
        b.quad(bot[row][cols - 1], bot[row + 1][cols - 1], top[row + 1][cols - 1], top[row][cols - 1], TRIM)

    # Raised centre crest, front to back over the crown. A recessed vent band was tried first
    # and rendered as scattered dark patches rather than slots at this shell resolution; a crest
    # is a real moulded-helmet feature and it survives a floodlit crown from any angle.
    arc = []
    for i in range(19):
        t = i / 18.0                          # sweep brow -> crown -> nape in the X-Z plane
        ang = -math.radians(6) + t * math.radians(192)
        arc.append((RX * math.cos(ang), 0.0, RZ * math.sin(ang)))
    b.loft([[(p[0] * 1.004, p[1] - 0.62, p[2] * 1.004) for p in arc],
            [(p[0] * 1.022, p[1], p[2] * 1.022) for p in arc],
            [(p[0] * 1.004, p[1] + 0.62, p[2] * 1.004) for p in arc]], SHELL, closed=False)

    return b.build('SM_C26_Helmet_Hero', mats, smooth_angle=34.0)


def build_grille():
    mats = [material('M_C26_HelmetBar', (0.360, 0.372, 0.392), 0.34, metallic=0.85),
            material('M_C26_HelmetTrim', (0.030, 0.032, 0.038), 0.62)]
    b = Builder()
    span = math.radians(62)
    # Round titanium bars on an arc that stands off the face, not flat quads. Five across, two
    # side stems and a centre stem: the grille silhouette is most of what says "batter" at range.
    for i, z in enumerate((-2.9, -5.7, -8.5, -11.3, -14.0)):
        reach = 10.9 - i * 0.30
        path, rad = [], []
        for k in range(11):
            t = k / 10.0 * 2 - 1
            a = t * span
            path.append((reach * math.cos(a) * 0.98, reach * math.sin(a) * 1.02, z + (1 - math.cos(a)) * 1.5))
            rad.append(0.30)
        b.loft(tube(path, rad, 6), 0, cap_start=path[0], cap_end=path[-1])
    for side in (-1, 1):
        path, rad = [], []
        for k in range(6):
            t = k / 5.0
            a = side * span
            path.append((10.9 * math.cos(a) * 0.98, 10.9 * math.sin(a) * 1.02, -2.5 - t * 12.2 + (1 - math.cos(a)) * 1.5))
            rad.append(0.34)
        b.loft(tube(path, rad, 6), 1, cap_start=path[0], cap_end=path[-1])
    path = [(11.05, 0, -2.7 - k / 5.0 * 11.9) for k in range(6)]
    b.loft(tube(path, [0.30] * 6, 6), 0, cap_start=path[0], cap_end=path[-1])
    return b.build('SM_C26_HelmetGrille_Hero', mats, smooth_angle=44.0)


def build_cap():
    mats = [material('M_C26_CapCrown', (0.055, 0.300, 0.345), 0.66),
            material('M_C26_CapPeak', (0.038, 0.210, 0.245), 0.58),
            material('M_C26_HelmetTrim', (0.030, 0.032, 0.038), 0.62)]
    b = Builder()
    sides, rings = 24, 6
    # A cap sits on the skull, not around it: lower crown, rim level all the way round.
    crown = _dome(b, sides, rings, scale=0.955, mat=0, cut=lambda a: -2.4)
    band = [(p[0] * 1.02, p[1] * 1.02, p[2] - 1.9) for p in crown]
    b.loft([crown, band], 2)
    b.loft([band, [(p[0] * 0.93, p[1] * 0.93, p[2] + 0.5) for p in band]], 2)
    top, bot, cols = [], [], 11
    for i in range(5):
        u = i / 4.0
        tr, br = [], []
        for c in range(cols):
            v = c / (cols - 1.0) * 2 - 1
            y = v * 8.2 * (1 - 0.20 * u * u)
            x = 8.8 + u * 8.2 * (1 - 0.22 * v * v)
            z = -3.4 - u * u * 2.4 + (v * v) * 1.5 * u
            tr.append((x, y, z + 0.30))
            br.append((x, y, z - 0.30))
        top.append(tr); bot.append(br)
    for row in range(4):
        for c in range(cols - 1):
            b.quad(top[row][c], top[row][c + 1], top[row + 1][c + 1], top[row + 1][c], 1)
            b.quad(bot[row][c + 1], bot[row][c], bot[row + 1][c], bot[row + 1][c + 1], 1)
    for c in range(cols - 1):
        b.quad(top[4][c], top[4][c + 1], bot[4][c + 1], bot[4][c], 2)
    return b.build('SM_C26_Cap_Hero', mats, smooth_angle=40.0)
