"""Premium hero batting pads. Drop-in replacement for build_guards.build_pad.

Contract (must not change -- PlacePad/PlaceKit and Dress() depend on it):
  * local frame: origin mid-shin, +Z up the shin, +X out of the leg front
  * material slots in order: M_C26_PadFace, M_C26_PadRoll, M_C26_PadStrap, M_C26_PadBuckle
  * longest dimension ~53.6 cm (Tools/ImportEquipment.py asserts 53.6 +- 1.5)
  * L/R authored separately, no mirrored negative-scale geometry

What changed vs v1 (measured against the hero review leg, shin 42.2 cm):
  * open-back shell wrapped to +-~112 deg instead of a full 360 deg tube, so the
    back of the shell no longer cuts 2 cm into the calf while the front floats
  * shell offset follows the real trouser surface (knee 8.5 / mid-shin 7.5 /
    ankle 5 side, front 5-7) plus ~1 cm ease and ~2.5 cm face padding
  * knee roll centred at local z=+14.5, i.e. on the knee under the real
    placement (origin sits 27.1 cm above the ankle, knee at +15.1)
  * thigh flap flares over the quads (face +12.5 at the top) instead of coning
    to a point; instep flap pitches forward over the shoe, clear of the toes
  * three face bolsters with grooves, edge piping + return flange (edge
    construction), top/bottom bindings, rear straps with outer buckles
  * straps span the back only (v1 bands crossed the front face); buckles sit on
    the outer calf (v1 had both on the inner side)

Run: Blender --background --python build_premium_pads.py  (from Equipment dir)
"""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from c26_build import Builder, material, tube, report, export_fbx, clear_scene  # noqa: E402

FACE, ROLL, STRAP, BUCKLE = 0, 1, 2, 3

# Local frame is cm, origin at the placement point (ankle + 0.56*shin + lifts).
# Axis of the leg passes through (-0.8, 0): the placement pushes 0.8 cm forward.
AXIS_X = -0.8

# z, face_x (shell base at dead front), half-width, wrap half-angle (deg)
STATIONS = [
    (24.5, 12.5, 10.5, 108),   # thigh flap over the quads
    (22.0, 11.5, 10.3, 110),
    (18.5, 10.0, 9.8, 112),    # just above the patella
    (14.5, 9.2, 9.5, 114),     # knee line: the roll lives here
    (11.0, 8.2, 9.2, 114),
    (6.0, 7.3, 9.3, 114),
    (0.0, 7.0, 9.0, 114),
    (-6.0, 6.6, 8.4, 114),
    (-12.0, 6.2, 7.6, 114),
    (-17.0, 6.0, 6.8, 112),
    (-21.0, 6.2, 6.2, 110),
    (-24.0, 6.8, 6.0, 108),    # instep: pitch forward over the shoe
    (-26.5, 7.2, 5.8, 105),
    (-28.5, 7.0, 5.5, 100),    # bottom edge: overlaps shoe upper, off the toes
]
ARC_PTS = 21

# Rear straps: z, band half-width, strap radius measured from the leg axis
STRAPS = [(-13.0, 1.3, 8.4), (0.0, 1.3, 10.8), (9.0, 1.3, 10.4)]


def _bump(x):
    x = max(0.0, 1.0 - x * x)
    return x * x


def bolster_lift(angle_deg):
    """Three vertical face rolls with grooves between; zero outside the face."""
    a = abs(angle_deg)
    if a > 42.0:
        return 0.0
    return 1.05 * _bump(a / 11.0) + 0.85 * _bump((a - 24.0) / 10.0)


def shell_point(z, face_x, half_w, angle_deg):
    """Shell surface for one arc sample. Elliptical wrap: face depth from
    face_x, side reach from half_w, so the wrap hugs instead of ballooning."""
    a = math.radians(angle_deg)
    ca, sa = math.cos(a), math.sin(a)
    # Superellipse-ish blend between face depth (front) and side reach.
    rx = face_x - AXIS_X if ca > 0 else (face_x - AXIS_X) * 0.9
    r = 1.0 / math.sqrt((ca / rx) ** 2 + (sa / half_w) ** 2) if (ca or sa) else rx
    lift = bolster_lift(angle_deg)
    rr = r + lift
    return (AXIS_X + rr * ca, rr * sa, z)


def build(name, buckle_side=1):
    mats = [material('M_C26_PadFace', (0.780, 0.790, 0.760), 0.75),
            material('M_C26_PadRoll', (0.680, 0.690, 0.660), 0.82),
            material('M_C26_PadStrap', (0.050, 0.050, 0.060), 0.80),
            material('M_C26_PadBuckle', (0.450, 0.460, 0.480), 0.35, metallic=0.85)]
    b = Builder()
    rims = [[], []]  # rim paths for piping: [0] = +Y side, [1] = -Y side
    rows = []
    for (z, face_x, half_w, wrap) in STATIONS:
        def flange(rim, dist, zz=z):
            inward = (AXIS_X - rim[0], 0.0 - rim[1], 0.0)
            il = math.hypot(*inward[:2]) or 1.0
            return (rim[0] + inward[0] / il * dist, rim[1] + inward[1] / il * dist, zz)

        # Arc runs +wrap -> -wrap so the (a, a+1, a+1+n, a+n) winding comes out
        # with outward normals for rows stacked top -> bottom.
        arc = [shell_point(z, face_x, half_w, wrap - 2 * wrap * j / (ARC_PTS - 1))
               for j in range(ARC_PTS)]
        rims[0].append(arc[0])
        rims[1].append(arc[-1])
        # Return flange: fold each rim back toward the leg so the edge reads as
        # 1.3 cm of real thickness instead of a zero-width shell cut.
        row = [flange(arc[0], 1.3), flange(arc[0], 0.65)]
        row.extend(arc)
        row.append(flange(arc[-1], 0.65))
        row.append(flange(arc[-1], 1.3))
        rows.append(row)
    n = len(rows[0])
    base = len(b.v)
    for row in rows:
        b.v.extend(row)
    for r in range(len(rows) - 1):
        for j in range(n - 1):
            a = base + r * n + j
            b.f.append((a, a + 1, a + 1 + n, a + n))
            b.m.append(FACE)
            u0, u1 = j / n, (j + 1) / n
            b.uv.append([(u0, r / 13), (u1, r / 13), (u1, (r + 1) / 13), (u0, (r + 1) / 13)])
    # Edge piping along both rims (capped ends: no hollow tube mouths).
    for rim in rims:
        b.loft(tube(rim, [0.5] * len(rim), 5), ROLL, cap_start=rim[0], cap_end=rim[-1])
    # Knee roll: horizontal capsule across the face at the knee line, curved in
    # plan view to follow the shell so the ends bury instead of poking past it.
    z_knee = 14.5
    face_knee = next(f for (z, f, w, a) in STATIONS if z == z_knee)
    half_knee = next(w for (z, f, w, a) in STATIONS if z == z_knee)
    rx_knee = face_knee - AXIS_X + 1.0  # bolster crown the roll sits on
    path = []
    for j in range(9):
        y = -8.5 + 17.0 * j / 8.0
        frac = min(1.0, abs(y) / half_knee)
        x = AXIS_X + rx_knee * math.sqrt(max(0.05, 1.0 - frac * frac))
        x += 0.5 - 0.3 * max(0.0, (abs(y) - 6.5) / 2.0)  # ends sink into the face
        path.append((x, y, z_knee))
    b.loft(tube(path, [1.3] * len(path), 8), ROLL, cap_start=path[0], cap_end=path[-1])
    # Top and bottom bindings: rolled rims closing the shell edges (capped).
    for zi, sta in ((0, STATIONS[0]), (-1, STATIONS[-1])):
        z, face_x, half_w, wrap = sta
        arc = [shell_point(z, face_x, half_w, -wrap + 2 * wrap * j / 11) for j in range(12)]
        b.loft(tube(arc, [0.7] * len(arc), 5), ROLL, cap_start=arc[0], cap_end=arc[-1])
    # Rear straps: back-only bands whose ends land on the wing rims, plus
    # anchor tabs (the attachment points) and an outer buckle each.
    for (z, hw, rs) in STRAPS:
        span = 68.0
        arc = []
        for j in range(15):
            ang = math.radians(180.0 - span + 2 * span * j / 14.0)
            arc.append((AXIS_X + rs * math.cos(ang), rs * math.sin(ang), z))
        rings = [[(p[0], p[1], z - hw) for p in arc], [(p[0], p[1], z + hw) for p in arc]]
        b.loft(rings, STRAP, closed=False)
        for end in (arc[0], arc[-1]):
            cx = end[0] + 0.9
            cy = end[1] + (0.9 if end[1] > 0 else -0.9)
            b.quad((end[0], end[1], z - hw), (cx, end[1], z - hw),
                   (cx, end[1], z + hw), (end[0], end[1], z + hw), ROLL)
        # Buckle straddles the strap end on the OUTER side (mirrored per leg).
        end_ang = math.radians(180.0 - span if buckle_side > 0 else 180.0 + span)
        bx = AXIS_X + rs * math.cos(end_ang)
        by = rs * math.sin(end_ang) + buckle_side * 0.2
        w, h, d = 2.4, 1.5, 3.0
        corners = [(bx - w / 2, by - h / 2, z - d / 2), (bx + w / 2, by - h / 2, z - d / 2),
                   (bx + w / 2, by + h / 2, z - d / 2), (bx - w / 2, by + h / 2, z - d / 2)]
        top = [(x, y, zz + d) for (x, y, zz) in corners]
        b.quad(*corners, BUCKLE)
        b.quad(*top, BUCKLE)
        for j in range(4):
            k = (j + 1) % 4
            b.quad(corners[j], corners[k], top[k], top[j], BUCKLE)
    return b.build(name, mats, smooth_angle=30.0)


def run(export=True, save=True):
    import bpy
    clear_scene()
    made = [build('SM_C26_Pad_L', buckle_side=1), build('SM_C26_Pad_R', buckle_side=-1)]
    report(made)
    if export:
        for o in made:
            export_fbx([o], str(HERE.parent.parent / 'Exports/Equipment' / (o.name + '.fbx')))
    if save:
        bpy.ops.wm.save_as_mainfile(filepath=str(HERE / 'C26_Pads_Premium.blend'))
    return made


if __name__ == '__main__':
    run()
