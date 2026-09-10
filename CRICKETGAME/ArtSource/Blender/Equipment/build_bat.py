"""Hero cricket bat. Local frame: origin at the top of the handle, blade down -Z, face +X.

That is the frame AC26Athlete::PlaceKit already poses the bat in, so the imported mesh drops
straight onto the existing grip solve. Dimensions are real: 10.8 cm blade width, 3.8 cm edges,
6.2 cm through the swell, 55 cm blade, 83 cm overall to the toe -- which is also
C26Field::BatLength, so the contact solver keeps meeting the middle of the willow.
"""
import math
from c26_build import Builder, material, circle, tube

WILLOW, GRIP, CANE, TWINE, LABEL = 0, 1, 2, 3, 4


def blade_ring(z, w, face, edge, spine, sides=24):
    """Rounded-rectangle section -- flat hitting face, blunt edge walls, ridged back.

    A pure ellipse (what the procedural bat used) has no edge and no spine, so it reads as a
    paddle from every camera. `face` is how far the hitting face stands off the origin plane,
    `edge` the back plane at the edge walls, `spine` the depth at the centre of the back.
    """
    pts = []
    half, mid = (face + edge) * 0.5, (face - edge) * 0.5
    for j in range(sides):
        a = 2 * math.pi * j / sides
        c, s = math.cos(a), math.sin(a)
        sy = math.copysign(abs(s) ** 0.55, s) if s else 0.0
        y = w * math.copysign(abs(c) ** 0.55, c) if c else 0.0
        x = mid + half * sy
        if sy < 0.0:  # back half only: raise the spine, tapering out to the edges
            ridge = max(0.0, 1.0 - (abs(y) / max(w, 1e-6)) ** 1.35) ** 1.45
            x -= (spine - edge) * (-sy) * ridge
        pts.append((x, y, z))
    return pts


def build():
    mats = [material('M_C26_BatWillow', (0.700, 0.612, 0.436), 0.46),
            material('M_C26_BatGrip', (0.028, 0.030, 0.036), 0.84),
            material('M_C26_BatCane', (0.520, 0.408, 0.240), 0.58),
            material('M_C26_BatTwine', (0.075, 0.068, 0.060), 0.78),
            material('M_C26_BatLabel', (0.640, 0.128, 0.092), 0.40)]
    b = Builder()

    # Parallel edges through the hitting area with a short, definite shoulder, then a toe that
    # rounds off over the last 8 cm. An evenly tapered oval -- which is what the procedural bat
    # was -- reads as a paddle; the shoulder and the parallel edges are what say "cricket bat".
    #            z      halfwidth  face  edge  spine
    profile = [(-21.0,  1.50,      0.60, 0.78, 0.88),
               (-23.5,  2.70,      0.84, 1.34, 1.70),
               (-26.0,  4.05,      0.98, 1.92, 2.70),
               (-28.5,  4.95,      1.08, 2.32, 3.52),
               (-31.0,  5.32,      1.15, 2.52, 4.10),
               (-35.0,  5.40,      1.19, 2.58, 4.52),
               (-42.0,  5.42,      1.20, 2.60, 4.86),
               (-50.0,  5.42,      1.20, 2.60, 5.02),
               (-58.0,  5.42,      1.20, 2.60, 5.00),
               (-65.0,  5.40,      1.19, 2.58, 4.76),
               (-71.0,  5.34,      1.16, 2.48, 4.30),
               (-75.5,  5.10,      1.08, 2.22, 3.62),
               (-78.5,  4.66,      0.96, 1.86, 2.86),
               (-80.8,  3.90,      0.80, 1.44, 2.06),
               (-82.3,  2.86,      0.60, 1.02, 1.38),
               (-83.0,  1.85,      0.40, 0.70, 0.94)]
    rings = [blade_ring(*p) for p in profile]
    b.loft(rings, WILLOW, cap_start=(0.0, 0.0, -20.6), cap_end=(-0.35, 0.0, -84.0), v_scale=2.2)

    # Cane handle. Oval in section -- narrower through the face direction than across it, the way
    # a bound cane handle actually sits in the hands, which also keeps the grip from reading round.
    handle = []
    for z, rx, ry in ((2.2, 1.48, 1.66), (-6.0, 1.52, 1.70), (-16.0, 1.60, 1.78), (-27.0, 1.70, 1.88)):
        handle.append([(math.sin(2 * math.pi * j / 16) * rx, math.cos(2 * math.pi * j / 16) * ry, z)
                       for j in range(16)])
    b.loft(handle, CANE, cap_start=(0.0, 0.0, 3.0), cap_end=(0.0, 0.0, -28.0))

    # Rubber grip: a sleeve with raised bands. The bands are what read as a grip at gameplay
    # distance -- a plain black cylinder just reads as more handle.
    grip_rings = []
    for i in range(19):
        t = i / 18.0
        z = 2.6 - t * 25.0
        band = 0.20 * (1.0 if i % 3 == 1 else 0.0)
        rx, ry = 1.60 + t * 0.16 + band, 1.80 + t * 0.16 + band
        grip_rings.append([(math.sin(2 * math.pi * j / 16) * rx, math.cos(2 * math.pi * j / 16) * ry, z)
                           for j in range(16)])
    # Capped at both ends: the sleeve is wider than the narrow top of the blade, so an open
    # bottom ring shows as a hole straight up the handle from any low camera.
    b.loft(grip_rings, GRIP, cap_start=(0.0, 0.0, 3.3), cap_end=(0.0, 0.0, -23.6), v_scale=4.0)

    # Splice binding: the twine collar where the handle enters the shoulders.
    for z0, z1, r in ((-20.4, -23.4, 1.98), (-24.6, -26.2, 2.34)):
        b.loft([circle(0, 0, z0, r, 14, squash=1.12), circle(0, 0, z1, r * 1.04, 14, squash=1.12)], TWINE,
               cap_start=(0.0, 0.0, z0 + 0.3), cap_end=(0.0, 0.0, z1 - 0.3))

    # Face label and toe sticker, a hair proud of the willow so they never z-fight.
    for z0, z1, hw in ((-35.0, -46.0, 3.40), (-63.0, -68.5, 2.35)):
        x0 = 1.18 + 0.07
        b.quad((x0, -hw, z0), (x0, hw, z0), (x0, hw, z1), (x0, -hw, z1), LABEL,
               (0, 0), (1, 0), (1, 1), (0, 1))

    obj = b.build('SM_C26_Bat_Hero', mats, smooth_angle=25.0)
    return obj
