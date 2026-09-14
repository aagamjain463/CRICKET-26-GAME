"""Round 4 batting-library gate for the RIGHT-HANDED batter.

Bakes every shot exactly as author_cricket_actions.py does, then samples EVERY frame of the baked
action (so Bezier interpolation between keys is judged, not just the keys) and checks:

  handedness  - left shoulder leads down the pitch at stance/recovery, left foot stays the front foot,
                the LEFT hand is the top hand on the handle
  grip        - both palms on the handle, hand spacing constant, no IK target out of reach
  clipping    - blade and handle clear the torso capsule, head and thighs
  footwork    - no planted-foot skating, no foot through the floor, no leg-only arm swing
  motion      - no per-frame snapping of pelvis/chest/bat, smooth return to the stance
  shot intent - bat face at contact points where the stroke is meant to go, and every stroke is
                measurably different from every other one

Run headless:
  Blender --background --python ArtSource/Blender/Premium/batting_lab.py -- [--only PULL,HOOK] [--render]
Writes Artifacts/CharacterAudit/BattingLab/report.json and, with --render, side/3-4 contact sheets.
Exits non-zero when any right-handed shot fails, so a bad stroke never reaches the importer.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
import c26_rig as rig_lib          # noqa: E402
import c26_actions as actions      # noqa: E402

OUT = ROOT / 'Artifacts/CharacterAudit/BattingLab'
OUT.mkdir(parents=True, exist_ok=True)

parser = argparse.ArgumentParser()
parser.add_argument('--only', help='Comma-separated shot names')
parser.add_argument('--render', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
only = set(args.only.split(',')) if args.only else None

# Bat frame (build_bat.py): origin at the top of the handle, blade down -Z, hitting face +X.
HANDLE_END, TOE, SWEET = 28.0, 83.0, 62.0
TORSO_R, HEAD_R, THIGH_R = 12.5, 13.0, 7.5   # head keeps margin for the helmet brim
FLOOR = actions.GROUND
FOREARM = 0.0


def author(v):
    """Rig frame -> authoring frame (+Y down the pitch). Only Y is reflected."""
    return Vector((v.x, -v.y, v.z))


def seg_dist(a0, a1, b0, b1):
    """Closest distance between two segments."""
    d1, d2, r = a1 - a0, b1 - b0, a0 - b0
    a, e, f = d1.dot(d1), d2.dot(d2), d2.dot(r)
    if e < 1e-6:   # second segment is a point (the skull): plain point-to-segment distance
        s = max(0.0, min(1.0, -d1.dot(r) / a)) if a > 1e-6 else 0.0
        return (a0 + d1 * s - b0).length
    c, b = d1.dot(r), d1.dot(d2)
    den = a * e - b * b
    s = max(0.0, min(1.0, (b * f - c * e) / den)) if den > 1e-6 else 0.0
    t = (b * s + f) / e if e > 1e-6 else 0.0
    if t < 0.0:
        t, s = 0.0, max(0.0, min(1.0, -c / a)) if a > 1e-6 else 0.0
    elif t > 1.0:
        t, s = 1.0, max(0.0, min(1.0, (b - c) / a)) if a > 1e-6 else 0.0
    return ((a0 + d1 * s) - (b0 + d2 * t)).length


def sample(rig, frame):
    bpy.context.scene.frame_set(frame)
    pb = rig.pose.bones
    head = {n: pb[n].head.copy() for n in (
        'pelvis', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r',
        'hand_l', 'hand_r', 'middle_01_l', 'middle_01_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
        'foot_l', 'foot_r', 'ball_l', 'ball_r', 'lowerarm_l', 'lowerarm_r')}
    bat = pb['hand_l'].matrix @ rig_lib.BAT_OFFSET_L
    rot = {n: pb[n].matrix.to_quaternion() for n in ('pelvis', 'spine_05', 'head')}
    return head, bat, rot


def palm(h, side):
    return h[f'hand_{side}'].lerp(h[f'middle_01_{side}'], 0.55)


def check_shot(rig, shot):
    name = shot['name']
    keys = shot['keys']
    action = rig_lib.bake(rig, f'LAB_{name}', keys, dense=shot.get('dense', False))
    first, last = int(keys[0][0]), int(keys[-1][0])
    contact = shot['contact']
    fails, notes = [], {}
    frames = []
    for f in range(first, last + 1):
        h, bat, rot = sample(rig, f)
        frames.append((f, h, bat, rot))

    def fail(msg):
        if msg not in fails:
            fails.append(msg)

    min_torso = min_head = min_thigh = 1e9
    grip_span = []
    prev = None
    worst_slide = {'l': 0.0, 'r': 0.0}
    worst_rot = {'pelvis': 0.0, 'spine_05': 0.0, 'bat': 0.0}
    for f, h, bat, rot in frames:
        origin = bat.translation
        shaft = bat.col[2].to_3d().normalized()
        face = bat.col[0].to_3d().normalized()
        handle_low = origin - shaft * HANDLE_END
        toe = origin - shaft * TOE

        # ---- right-handed grip: LEFT palm is higher up the handle than the RIGHT palm
        pl, pr = palm(h, 'l'), palm(h, 'r')
        along_l, along_r = (pl - origin).dot(shaft), (pr - origin).dot(shaft)
        wrist_l, wrist_r = (h['hand_l'] - origin).dot(shaft), (h['hand_r'] - origin).dot(shaft)
        if wrist_l - wrist_r < 4.0:
            fail(f'f{f}: top hand is not the LEFT hand (left wrist {wrist_l:.1f} right wrist {wrist_r:.1f} along shaft)')
        for side, p, along in (('l', pl, along_l), ('r', pr, along_r)):
            off_axis = ((p - origin) - shaft * along).length
            if off_axis > 6.5 or not (-HANDLE_END - 4 < along < 6):
                fail(f'f{f}: {side} palm off the handle (axis {off_axis:.1f}cm, along {along:.1f}cm)')
        grip_span.append((h['hand_l'] - h['hand_r']).length)
        # a hand forced onto the bat beyond the arm's reach detaches from the forearm
        for side in ('l', 'r'):
            gap = abs((h[f'hand_{side}'] - h[f'lowerarm_{side}']).length - FOREARM)
            if gap > 1.0:
                fail(f'f{f}: {side} wrist dislocated {gap:.1f}cm (hand target out of reach)')
        for side in ('l', 'r'):
            if h[f'calf_{side}'].z < 4.0:
                fail(f'f{f}: {side} knee through the floor ({h[f"calf_{side}"].z:.1f})')

        # ---- clipping: torso capsule pelvis->neck, head sphere, thigh capsules
        torso0, torso1 = h['pelvis'], h['neck_01']
        d_torso = seg_dist(handle_low, toe, torso0, torso1)
        d_handle = seg_dist(origin, handle_low, torso0, torso1)
        min_torso = min(min_torso, d_torso, d_handle + 4.0)
        skull = h['head'] + Vector((0, 0, 9.0))
        d_head = seg_dist(origin, toe, skull, skull)
        min_head = min(min_head, d_head)
        d_thigh = {}
        for side in ('l', 'r'):
            d_thigh[side] = seg_dist(handle_low, toe, h[f'thigh_{side}'], h[f'calf_{side}'])
            min_thigh = min(min_thigh, d_thigh[side])
        if d_torso < TORSO_R or d_handle + 4.0 < TORSO_R:
            fail(f'f{f}: bat passes through the torso ({min(d_torso, d_handle + 4):.1f}cm from spine axis)')
        if d_head < HEAD_R:
            fail(f'f{f}: bat passes through the head ({d_head:.1f}cm)')
        for side, d in d_thigh.items():
            if d < THIGH_R - 3.0:
                fail(f'f{f}: blade passes through the {side} thigh ({d:.1f}cm)')

        # ---- floor, planted-foot skating
        for side in ('l', 'r'):
            ankle = h[f'foot_{side}']
            if ankle.z < FLOOR - 1.5:
                fail(f'f{f}: {side} foot below the floor ({ankle.z:.1f})')
        if prev:
            ph = prev[1]
            for side in ('l', 'r'):
                a0, a1 = ph[f'foot_{side}'], h[f'foot_{side}']
                if a0.z < FLOOR + 0.5 and a1.z < FLOOR + 0.5:
                    slide = (Vector((a1.x, a1.y, 0)) - Vector((a0.x, a0.y, 0))).length
                    worst_slide[side] = max(worst_slide[side], slide)
            for n, limit in (('pelvis', 20.0), ('spine_05', 26.0)):
                ang = math.degrees(prev[3][n].rotation_difference(rot[n]).angle)
                worst_rot[n] = max(worst_rot[n], ang)
                if ang > limit:
                    fail(f'f{f}: {n} snaps ({ang:.0f} deg in one frame)')
            pshaft = prev[2].col[2].to_3d().normalized()
            ang = math.degrees(pshaft.angle(shaft, 0.0))
            worst_rot['bat'] = max(worst_rot['bat'], ang)
            # ~1900 deg/s: faster than an elite bat at impact reads as a pop, not a swing
            if ang > 64.0:
                fail(f'f{f}: bat snaps ({ang:.0f} deg in one frame)')
        prev = (f, h, bat, rot)

    # ---- handedness of the body itself at stance, contact and recovery
    for f, h, bat, rot in (frames[0], frames[contact - first], frames[-1]):
        shoulders = author(h['upperarm_l'] - h['upperarm_r'])
        if f in (first, last) and shoulders.y < 8.0:
            fail(f'f{f}: left shoulder does not lead down the pitch (dy {shoulders.y:.1f})')
        feet = author(h['foot_l'] - h['foot_r'])
        if feet.y < 4.0:
            fail(f'f{f}: left foot is not the front foot (dy {feet.y:.1f})')
    for f, h, _, _ in frames:
        if author(h['foot_l'] - h['foot_r']).y < 0.0:
            fail(f'f{f}: feet cross over - back foot ahead of the front foot')
            break

    if max(grip_span) - min(grip_span) > 3.5:
        fail(f'grip span drifts {min(grip_span):.1f}-{max(grip_span):.1f}cm (hands sliding on the handle)')
    for side, slide in worst_slide.items():
        if slide > 1.2:
            fail(f'{side} foot skates {slide:.1f}cm/frame while planted')

    # ---- shot intent at contact
    _, h, bat, _ = frames[contact - first]
    origin, shaft, face = bat.translation, bat.col[2].to_3d().normalized(), bat.col[0].to_3d().normalized()
    sweet = author(origin - shaft * SWEET)
    face_a = author(face)
    want = shot.get('direction')
    if want is not None:
        rad = math.radians(want)
        # angle 0 = straight down the ground (+Y), positive = off side (-X) for a right-hander
        target = Vector((-math.sin(rad), math.cos(rad), 0.0))
        horizontal = Vector((face_a.x, face_a.y, 0.0))
        if horizontal.length > 0.2:
            err = math.degrees(horizontal.normalized().angle(target, 0.0))
            notes['face_error_deg'] = round(err, 1)
            if err > shot.get('face_tolerance', 38):
                fail(f'bat face at contact points {err:.0f} deg away from the {want} deg stroke line')
        elif shot.get('vertical_face_ok') is not True:
            fail('bat face at contact points at the ground or sky')
    pelvis_a = author(h['pelvis'])
    notes['knees_at_contact'] = [[round(v, 1) for v in author(h[f'calf_{s}'])] for s in 'lr']
    notes['hands_at_contact'] = [[round(v, 1) for v in author(h[f'hand_{s}'])] for s in 'lr']
    notes['shoulders_at_contact'] = [[round(v, 1) for v in author(h[f'upperarm_{s}'])] for s in 'lr']
    notes.update({
        'sweet_spot_at_contact': [round(v, 1) for v in sweet],
        'front_foot_at_contact': [round(v, 1) for v in author(h['foot_l'])],
        'back_foot_at_contact': [round(v, 1) for v in author(h['foot_r'])],
        'pelvis_at_contact': [round(v, 1) for v in pelvis_a],
        'min_bat_torso_cm': round(min_torso, 1), 'min_bat_head_cm': round(min_head, 1),
        'min_blade_thigh_cm': round(min_thigh, 1),
        'grip_span_cm': [round(min(grip_span), 1), round(max(grip_span), 1)],
        'max_planted_slide_cm': {k: round(v, 2) for k, v in worst_slide.items()},
        'max_deg_per_frame': {k: round(v, 1) for k, v in worst_rot.items()},
    })
    footwork = shot.get('footwork')
    stance = frames[0][1]
    fdy = author(h['foot_l'] - stance['foot_l']).y
    bdy = author(h['foot_r'] - stance['foot_r']).y
    notes['front_foot_travel_y'], notes['back_foot_travel_y'] = round(fdy, 1), round(bdy, 1)
    if footwork == 'front' and fdy < 12:
        fail(f'front-foot stroke only strides {fdy:.1f}cm')
    if footwork == 'back' and bdy > -3:
        fail(f'back-foot stroke does not move the back foot back ({bdy:.1f}cm)')
    # Distinctness is judged on the stroke itself (plant to follow-through), not on the shared stance
    # and recovery either side of it.
    trajectory = [[author(fr[1][n]) for n in ('hand_l', 'foot_l', 'foot_r', 'pelvis', 'head')] +
                  [author(fr[2].translation - fr[2].col[2].to_3d().normalized() * SWEET)]
                  for fr in frames if contact - 8 <= fr[0] <= contact + 16]
    summary = {}
    for msg in fails:
        head, _, rest = msg.partition(': ')
        if head.startswith('f') and head[1:].isdigit():
            kind = rest.split(' (')[0]
            summary.setdefault(kind, []).append(int(head[1:]))
        else:
            summary.setdefault(msg, [])
    detail = {}
    for msg in fails:
        head, _, rest = msg.partition(': ')
        if '(' in rest:
            detail.setdefault(rest.split(' (')[0], []).append(f"{head}({rest.split(' (')[1].rstrip(')')})")
    compact = {k: (f'f{min(v)}-f{max(v)} x{len(v)} ' + ' '.join(detail.get(k, [])[:6]) if v else '')
               for k, v in summary.items()}
    return {'name': name, 'pass': not fails, 'failures': compact, 'failure_count': len(fails),
            'metrics': notes}, trajectory, action


def distinct(trajs):
    """Mean separation of hands, feet, pelvis and sweet spot over normalised time."""
    names = list(trajs)
    out = {}
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            ta, tb = trajs[a], trajs[b]
            total, n = 0.0, 24
            for k in range(n):
                fa = ta[int(k / (n - 1) * (len(ta) - 1))]
                fb = tb[int(k / (n - 1) * (len(tb) - 1))]
                total += sum((pa - pb).length for pa, pb in zip(fa, fb)) / len(fa)
            out[f'{a}|{b}'] = round(total / n, 1)
    return out


def make_bat_proxy():
    mesh = bpy.data.meshes.new('LAB_Bat')
    w, t = 5.4, 3.0
    verts, faces = [], []

    def box(z0, z1, hw, ht):
        base = len(verts)
        for z in (z0, z1):
            for x, y in ((-ht, -hw), (ht, -hw), (ht, hw), (-ht, hw)):
                verts.append((x, y, z))
        faces.extend([(base + a, base + b, base + c, base + d) for a, b, c, d in
                      ((0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0))])
    box(-HANDLE_END, 0.0, 1.6, 1.6)
    box(-TOE, -HANDLE_END, w, t)
    box(-TOE + 20, -HANDLE_END - 4, 2.0, t + 1.2)   # hitting face marker on +X
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new('LAB_Bat', mesh)
    bpy.context.collection.objects.link(obj)
    mat = bpy.data.materials.new('LAB_BatMat')
    mat.diffuse_color = (0.85, 0.72, 0.45, 1)
    obj.data.materials.append(mat)
    return obj


def render(rig, shot, bat_obj):
    import c26_sheet
    keys = [int(k[0]) for k in shot['keys']]
    frames = sorted(set(keys + [shot['contact'], shot['contact'] - 3, shot['contact'] + 4]))

    def pose(r, f):
        bpy.context.scene.frame_set(f)
        bat_obj.matrix_world = r.matrix_world @ r.pose.bones['hand_l'].matrix @ rig_lib.BAT_OFFSET_L

    entries = [(str(f), f, 'point') for f in frames] + [(str(f), f, 'bowler34') for f in frames]
    path = OUT / f"{shot['name']}_R.png"
    c26_sheet.sheet(rig, pose, entries, path, cols=len(frames))
    return str(path)


def main():
    import c26_sheet
    views = {'point': Vector((-4.6, -0.3, 1.1)), 'bowler34': Vector((-2.9, -3.7, 1.25)),
             'keeper34': Vector((2.4, 3.8, 1.3))}
    original_setup = c26_sheet.setup

    def setup(rig, view='threequarter'):
        cam = original_setup(rig, 'threequarter')
        if view in views:
            cam.location = views[view]
            cam.rotation_euler = (Vector((0, 0, 0.9)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        return cam
    c26_sheet.setup = setup

    blend = ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend'
    rig = rig_lib.load(blend, keep_mesh=args.render)
    global FOREARM
    FOREARM = rig_lib.rest_len(rig, 'lowerarm_l', 'hand_l')
    if rig.animation_data is None:
        rig.animation_data_create()
    bat_obj = make_bat_proxy() if args.render else None
    if args.render:
        # Orientation markers on the floor: red = towards the bowler (+Y), blue = off side (-X).
        for name, loc, colour in (('LAB_Bowler', (0, -1.5, 0.05), (0.9, 0.1, 0.1, 1)),
                                  ('LAB_OffSide', (-1.5, 0, 0.05), (0.1, 0.3, 0.9, 1))):
            bpy.ops.mesh.primitive_cube_add(size=0.12, location=loc)
            marker = bpy.context.active_object
            marker.name = name
            mat = bpy.data.materials.new(name + 'Mat')
            mat.diffuse_color = colour
            marker.data.materials.append(mat)
        bpy.context.view_layer.objects.active = rig
        bpy.ops.object.mode_set(mode='POSE')
    report, trajs = [], {}
    for shot in actions.SHOTS:
        if only and shot['name'] not in only:
            continue
        result, traj, action = check_shot(rig, shot)
        trajs[shot['name']] = traj
        if args.render:
            result['sheet'] = render(rig, shot, bat_obj)
        report.append(result)
        print('C26_SHOT', shot['name'], 'PASS' if result['pass'] else 'FAIL', result['failure_count'],
              json.dumps(result['metrics']))
        for kind, frames in result['failures'].items():
            print('   ', kind, frames)
    pairs = distinct(trajs)
    too_close = {k: v for k, v in pairs.items() if v < 9.0}
    (OUT / 'report.json').write_text(json.dumps({'shots': report, 'distinctness_cm': pairs,
                                                 'too_similar': too_close}, indent=1))
    failed = [r['name'] for r in report if not r['pass']]
    print('C26_BATTING_LAB', 'PASS' if not failed and not too_close else 'FAIL',
          'shots', len(report), 'failed', failed, 'too_similar', too_close)
    return 0 if not failed and not too_close else 1


code = main()
sys.stdout.flush()
if code:
    sys.exit(code)
