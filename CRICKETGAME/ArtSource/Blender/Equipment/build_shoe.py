"""Cricket shoe. Local frame: origin at the ankle joint, toes +X, up +Z.

The base character ships in dark street trainers. Under a floodlit night rig those read as two
black holes where the athlete meets the turf, which is the worst possible place to lose contrast:
the feet are what ground the player. A white cricket shoe with a coloured flash and a spiked sole
fixes the grounding and adds a role cue at the same time.
"""
import math
from c26_build import Builder, material, tube

UPPER, SOLE, FLASH, LACE, SPIKE = 0, 1, 2, 3, 4

# x along the foot, half-width, sole height, upper height
LAST = [(-7.6, 3.30, 0.0, 7.6), (-5.4, 4.05, 0.0, 8.4), (-2.4, 4.45, 0.1, 8.0),
        (1.0, 4.60, 0.2, 6.2), (4.6, 4.70, 0.3, 4.6), (8.4, 4.55, 0.4, 3.6),
        (12.0, 4.00, 0.5, 3.0), (15.0, 3.15, 0.6, 2.5), (17.2, 1.90, 0.7, 1.8)]


def _ring(x, w, z0, z1, sides=14, cy=0.0):
    """Section through the shoe: flat on the ground, domed over the instep."""
    pts = []
    for j in range(sides):
        a = 2 * math.pi * j / sides
        c, s = math.cos(a), math.sin(a)
        y = w * math.copysign(abs(c) ** 0.72, c) if c else 0.0
        sy = math.copysign(abs(s) ** 0.72, s) if s else 0.0
        z = (z0 + z1) * 0.5 + (z1 - z0) * 0.5 * sy
        pts.append((x, cy + y, z))
    return pts


def build(name, side=1):
    mats = [material('M_C26_ShoeUpper', (0.790, 0.800, 0.782), 0.52),
            material('M_C26_ShoeSole', (0.130, 0.136, 0.148), 0.72),
            material('M_C26_ShoeFlash', (0.055, 0.300, 0.345), 0.44),
            material('M_C26_ShoeLace', (0.660, 0.668, 0.650), 0.86),
            material('M_C26_ShoeSpike', (0.400, 0.410, 0.430), 0.30, metallic=0.8)]
    b = Builder()
    rings = [_ring(x, w, sole + 0.9, up) for x, w, sole, up in LAST]
    rings.insert(0, _ring(-8.4, 2.55, 1.9, 6.6))
    rings.append(_ring(18.1, 0.95, 1.3, 1.7))   # blunt toe: a single apex cap points the shoe
    b.loft(rings, UPPER, cap_start=(-9.0, 0, 4.2), cap_end=(18.5, 0, 1.5), v_scale=2.0)

    # Midsole slab: a distinct band all the way round, which is what separates a sports shoe
    # from a moulded lump at gameplay distance.
    b.loft([[(p[0], p[1] * 1.04, min(p[2], 1.05)) for p in _ring(x, w, 0.0, 1.9)]
            for x, w, sole, up in LAST], SOLE,
           cap_start=(-8.6, 0, 0.9), cap_end=(18.0, 0, 1.2))

    # Swoosh-style flash along the outside quarter -- original CRICKET 26 kit graphic, no logos.
    def last_width(x):
        """Half-width of the last at x, so the flash lies on the upper instead of floating off
        it wherever the shoe narrows."""
        for i in range(len(LAST) - 1):
            if LAST[i][0] <= x <= LAST[i + 1][0]:
                t = (x - LAST[i][0]) / (LAST[i + 1][0] - LAST[i][0])
                return LAST[i][1] + (LAST[i + 1][1] - LAST[i][1]) * t
        return LAST[0][1] if x < LAST[0][0] else LAST[-1][1]

    for i in range(6):
        t0, t1 = i / 6.0, (i + 1) / 6.0
        def pt(t, lift):
            x = -3.4 + t * 13.6
            z = 2.15 + math.sin(t * math.pi) * 1.7 + lift
            # Section narrows with height, so track it: the flash sits on the quarter panel.
            w = last_width(x) * (1.0 - 0.030 * max(0.0, z - 1.9)) + 0.10
            return (x, side * w, z)
        b.quad(pt(t0, -0.70), pt(t1, -0.70), pt(t1, 0.70), pt(t0, 0.70), FLASH)

    # Lace panel: five bands across the instep.
    for i in range(5):
        x = -0.6 + i * 2.5
        w = 2.5 - i * 0.24
        z = 6.6 - i * 0.72
        b.loft(tube([(x - 0.35, -w, z), (x, 0, z + 0.45), (x + 0.35, w, z)], [0.32] * 3, 5), LACE)

    # Studs. Six moulded spikes, the detail that says cricket rather than tennis.
    for x, y in ((13.4, 0.45), (11.0, -0.62), (7.0, 0.70), (3.2, -0.72), (-3.4, 0.62), (-5.8, -0.55)):
        w = min(LAST, key=lambda l: abs(l[0] - x))[1]
        cy = y * w
        b.loft([_ring(x, 0.62, -0.05, 0.05, sides=6, cy=cy),
                _ring(x, 0.32, -0.95, -0.85, sides=6, cy=cy)], SPIKE, cap_end=(x, cy, -1.15))
    return b.build(name, mats, smooth_angle=32.0)
