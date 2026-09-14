"""Round 5 gate for the bowling, fielding and wicketkeeping library.

Bakes each clip exactly as author_cricket_actions.py does (dense solve, root travel subtracted),
adds the gameplay root travel back, samples EVERY frame and checks:

  handedness  - bowling: the bowling-side foot lands first (back foot), the other foot is the front
                foot, the non-bowling shoulder leads at back-foot contact, the bowling hand is the
                high hand over its own shoulder at release. Throws: the throwing shoulder is back
                at the load and the throwing hand is high and in front at release.
  ball/hand   - the ball hand never jumps; catches and pickups put the palms on the event target
  footwork    - no planted foot skating in WORLD space, no foot or knee through the floor
  kinetic     - pelvis turns before the chest reaches release (hips lead), no pelvis/chest snap
  reach       - no wrist dislocated by an unreachable IK target
  root        - the body stays over the gameplay root (no lagging ghost body)
  variety     - fast, fast-medium and spin actions are measurably different, not re-timed copies

Run headless:
  Blender --background --python ArtSource/Blender/Premium/action_lab.py -- [--only FastBowl_R,...] [--render]
Writes Artifacts/CharacterAudit/ActionLab/report.json and exits non-zero on any failure.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
import c26_rig as rig_lib          # noqa: E402
import c26_actions as actions      # noqa: E402

OUT = ROOT / 'Artifacts/CharacterAudit/ActionLab'
OUT.mkdir(parents=True, exist_ok=True)

parser = argparse.ArgumentParser()
parser.add_argument('--only', help='Comma-separated clip names')
parser.add_argument('--render', action='store_true')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
only = set(args.only.split(',')) if args.only else None

FLOOR = actions.GROUND
BONES = ('pelvis', 'spine_05', 'neck_01', 'head', 'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r',
         'hand_l', 'hand_r', 'middle_01_l', 'middle_01_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
         'foot_l', 'foot_r', 'ball_l', 'ball_r')


def clips():
    """(name, source action, keys, mirrored) for every Round 5 clip, as build_manifest() bakes them."""
    out = []
    for a in actions.FIELDING_R5:
        out.append((a['name'], a, a['keys'], False))
    for a in actions.MIRRORED_R5:
        out.append((a['name'], a, a['keys'], False))
        out.append((a['name'][:-2] + '_R', a, actions._mirror_keys(a['keys']), True))
    for a in actions.BOWLING_R5:
        out.append((a['name'] + '_R', a, a['keys'], False))
        out.append((a['name'] + '_L', a, actions._mirror_keys(a['keys']), True))
    return out


def sample(rig, frame, travel):
    bpy.context.scene.frame_set(frame)
    pb = rig.pose.bones
    ty = travel(frame)[1] if travel else 0.0
    # Rig frame -> authoring frame (+Y forward) -> world (gameplay root travel added back).
    h = {n: Vector((pb[n].head.x, -pb[n].head.y + ty, pb[n].head.z)) for n in BONES}
    rot = {n: pb[n].matrix.to_quaternion() for n in ('pelvis', 'spine_05')}
    return h, rot, ty


def palm(h, side):
    return h[f'hand_{side}'].lerp(h[f'middle_01_{side}'], 0.55)


def kind_of(src):
    return src.get('kind') or src.get('thrower')


def lead(h, front, back):
    """How much `front` leads `back` down +Y as a fraction of their separation: 1 = fully side-on."""
    d = (h[front] - h[back]).to_2d()
    return d.y / max(d.length, 1e-3)


def yaw(h, a, b):
    d = h[a] - h[b]
    return math.degrees(math.atan2(d.y, -d.x))   # 0 = square to +Y, 90 = side-on, left end leading


def check(rig, name, src, keys, mirrored, forearm):
    travel = src.get('travel')
    action = rig_lib.bake(rig, f'LAB_{name}', keys, dense=True, travel=travel, resolve_mixed=True)
    first, last = int(keys[0][0]), int(keys[-1][0])
    frames = [(f,) + sample(rig, f, travel) for f in range(first, last + 1)]
    fails, notes = [], {}

    def fail(msg):
        if msg not in fails:
            fails.append(msg)

    at = {f: (h, rot, ty) for f, h, rot, ty in frames}
    event = src['contact']
    dive, slide = bool(src.get('dive')), name.startswith('SlideSave')
    worst_slide, worst_rot, worst_jump, worst_root = (0.0, ''), {'pelvis': 0.0, 'spine_05': 0.0}, 0.0, (0.0, '')
    fast = bool(src.get('kind') or src.get('thrower'))
    prev = None
    for f, h, rot, ty in frames:
        for side in 'lr':
            if h[f'foot_{side}'].z < FLOOR - 1.5:
                fail(f'f{f}: {side} foot below the floor ({h[f"foot_{side}"].z:.1f})')
            if h[f'calf_{side}'].z < (1.0 if dive or slide else 4.0):
                fail(f'f{f}: {side} knee through the floor ({h[f"calf_{side}"].z:.1f})')
            gap = abs((h[f'hand_{side}'] - h[f'lowerarm_{side}']).length - forearm)
            if gap > 1.0:
                fail(f'f{f}: {side} wrist dislocated {gap:.1f}cm (hand target out of reach)')
        root = Vector((h['pelvis'].x, h['pelvis'].y - ty))
        if not dive and not slide:
            worst_root = max(worst_root, (root.length, f'f{f}'))
        if prev:
            ph, prot = prev[1], prev[2]
            for side in 'lr':
                a0, a1 = ph[f'foot_{side}'], h[f'foot_{side}']
                if a0.z < FLOOR + 1.0 and a1.z < FLOOR + 1.0 and not slide:
                    worst_slide = max(worst_slide, ((a1 - a0).to_2d().length, f'f{f} {side}'))
                jump = (palm(h, side) - palm(ph, side)).length
                worst_jump = max(worst_jump, jump)
                if jump > (95.0 if kind_of(src) else 60.0):
                    fail(f'f{f}: {side} hand teleports {jump:.0f}cm in one frame')
            # A release whips the trunk at up to ~1100 deg/s (37/frame); anything else is a pop.
            for n, limit in (('pelvis', 28.0 if dive else 22.0), ('spine_05', 38.0 if fast else 30.0)):
                ang = math.degrees(prot[n].rotation_difference(rot[n]).angle)
                ang = min(ang, 360.0 - ang)
                worst_rot[n] = max(worst_rot[n], ang)
                if ang > limit:
                    fail(f'f{f}: {n} snaps ({ang:.0f} deg in one frame)')
        prev = (f, h, rot)
    if worst_slide[0] > 1.6:
        fail(f'planted foot skates {worst_slide[0]:.1f}cm/frame ({worst_slide[1]})')
    if worst_root[0] > 75.0:
        fail(f'body drifts {worst_root[0]:.0f}cm off the gameplay root ({worst_root[1]})')

    kind = src.get('kind')
    if kind:
        arm = 'l' if mirrored else 'r'
        front = 'r' if mirrored else 'l'
        sign = -1.0 if mirrored else 1.0          # mirrored clip: side-on means the RIGHT end leads
        h_b, _, _ = at[src['bfc_frame']]
        h_f, _, _ = at[src['ffc_frame']]
        h_r, _, _ = at[event]
        if h_b[f'foot_{arm}'].z > FLOOR + 2.5:
            fail(f'back-foot contact is not on the bowling-side ({arm}) foot (z {h_b[f"foot_{arm}"].z:.1f})')
        if h_f[f'foot_{front}'].z > FLOOR + 2.5:
            fail(f'front-foot contact is not on the {front} foot (z {h_f[f"foot_{front}"].z:.1f})')
        if h_f[f'foot_{front}'].y - h_f[f'foot_{arm}'].y < 40.0:
            fail(f'delivery stride is not led by the {front} foot '
                 f'({h_f[f"foot_{front}"].y - h_f[f"foot_{arm}"].y:.0f}cm)')
        shoulder_bfc = lead(h_b, f'upperarm_{front}', f'upperarm_{arm}')
        notes['bfc_shoulder_side_on_deg'] = round(sign * yaw(h_b, 'upperarm_l', 'upperarm_r'), 1)
        if shoulder_bfc < (0.6 if kind == 'pace' else 0.4):
            fail(f'{front} shoulder does not lead at back-foot contact (lead {shoulder_bfc:.2f})')
        low = min((at[f][0]['pelvis'].z, f) for f in range(src['bfc_frame'], event + 4))
        notes['min_hip_height_delivery'] = round(low[0], 1)
        if low[0] < 78.0:
            fail(f'bowler collapses into a squat through the delivery (hips {low[0]:.0f}cm at f{low[1]})')
        if h_r['head'].z < 140.0:
            fail(f'head only {h_r["head"].z:.0f}cm high at release (not braced tall)')
        hand, shoulder = h_r[f'hand_{arm}'], h_r[f'upperarm_{arm}']
        if hand.z < h_r['head'].z + 12.0:
            fail(f'bowling hand ({arm}) is not above the head at release ({hand.z - h_r["head"].z:.0f}cm)')
        if hand.z < h_r[f'hand_{front}'].z + 55.0:
            fail(f'the {arm} hand is not the high bowling hand at release')
        up = hand - shoulder
        tilt = math.degrees(up.angle(Vector((0, 0, 1)), 0.0))
        notes['release_arm_from_vertical_deg'] = round(tilt, 1)
        if tilt > (32.0 if kind == 'pace' else 48.0):
            fail(f'bowling arm {tilt:.0f} deg from vertical at release')
        if sign * (hand.x - h_r['head'].x) > 12.0:
            fail(f'bowling hand crosses over the head to the wrong side ({hand.x - h_r["head"].x:.0f}cm)')
        if not (-12.0 < hand.y - shoulder.y < 60.0):
            fail(f'release point not just ahead of the shoulder ({hand.y - shoulder.y:.0f}cm)')
        # Kinetic chain: the hips have opened before the chest does.
        f0, f1 = src['bfc_frame'], event
        hips = [sign * yaw(at[f][0], 'thigh_l', 'thigh_r') for f in range(f0, f1 + 1)]
        chest = [sign * yaw(at[f][0], 'upperarm_l', 'upperarm_r') for f in range(f0, f1 + 1)]

        def half_open(series):
            start, end = series[0], series[-1]
            mid = start + (end - start) * 0.5
            return next((i for i, v in enumerate(series) if (v - mid) * (end - start) >= 0), len(series))
        notes['hips_open_frame'], notes['chest_open_frame'] = f0 + half_open(hips), f0 + half_open(chest)
        if half_open(hips) > half_open(chest):
            fail('chest opens before the hips (no hip-shoulder separation)')
        notes['release_hand'] = [round(v, 1) for v in hand]
        notes['release_height_over_head'] = round(hand.z - h_r['head'].z, 1)

    target = src.get('event_target')
    if target is not None:
        tx = -target[0] if mirrored else target[0]
        h, _, ty = at[event]
        want = Vector((tx, target[1] + ty, target[2]))
        mid = (palm(h, 'l') + palm(h, 'r')) * 0.5
        miss = (mid - want).length
        notes['event_palm_miss_cm'] = round(miss, 1)
        if miss > 14.0:
            fail(f'hands miss the ball at the {src["event"]} frame by {miss:.0f}cm '
                 f'(palms {tuple(round(v) for v in mid)} ball {tuple(round(v) for v in want)})')
        span = (palm(h, 'l') - palm(h, 'r')).length
        if span > 22.0:
            fail(f'hands are {span:.0f}cm apart at the {src["event"]} frame')
    if src.get('thrower'):
        h_l, _, _ = at[first + 1]
        if lead(h_l, 'upperarm_l', 'upperarm_r') < 0.55:
            fail('throwing load is not side-on with the left shoulder at the target')
        h, _, _ = at[event]
        hand, shoulder = h['hand_r'], h['upperarm_r']
        if src.get('overarm') and hand.z < shoulder.z + 12.0:
            fail(f'overarm release hand is below the shoulder ({hand.z - shoulder.z:.0f}cm)')
        if hand.y < shoulder.y - 5.0:
            fail(f'throw releases behind the shoulder ({hand.y - shoulder.y:.0f}cm)')
    notes.update({'max_planted_slide_cm': round(worst_slide[0], 2), 'max_hand_cm_per_frame': round(worst_jump, 1),
                  'max_root_offset_cm': round(worst_root[0], 1),
                  'max_deg_per_frame': {k: round(v, 1) for k, v in worst_rot.items()}})
    traj = [[at[f][0][n] - Vector((0, at[f][2], 0)) for n in ('hand_r', 'hand_l', 'foot_l', 'foot_r', 'pelvis', 'head')]
            for f in range(first, last + 1)]
    return {'name': name, 'pass': not fails, 'failures': fails[:12], 'failure_count': len(fails),
            'metrics': notes}, traj, action


def distinct(a, b):
    total, n = 0.0, 24
    for k in range(n):
        fa = a[int(k / (n - 1) * (len(a) - 1))]
        fb = b[int(k / (n - 1) * (len(b) - 1))]
        total += sum((pa - pb).length for pa, pb in zip(fa, fb)) / len(fa)
    return round(total / n, 1)


def render(rig, name, keys, src):
    import c26_sheet
    frames = sorted({int(k[0]) for k in keys} | {src['contact']})
    travel = src.get('travel')

    def pose(r, f):
        bpy.context.scene.frame_set(f)
        r.location.y = -(travel(f)[1] if travel else 0.0) * 0.01   # show world travel (rig faces -Y)
    entries = [(str(f), f, 'side') for f in frames] + [(str(f), f, 'threequarter') for f in frames]
    path = OUT / f'{name}.png'
    c26_sheet.sheet(rig, pose, entries, path, cols=len(frames))
    rig.location.y = 0.0
    return str(path)


def main():
    rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=args.render)
    forearm = rig_lib.rest_len(rig, 'lowerarm_l', 'hand_l')
    if rig.animation_data is None:
        rig.animation_data_create()
    report, trajs = [], {}
    for name, src, keys, mirrored in clips():
        if only and name not in only:
            continue
        result, traj, _ = check(rig, name, src, keys, mirrored, forearm)
        trajs[name] = traj
        if args.render:
            result['sheet'] = render(rig, name, keys, src)
        report.append(result)
        print('C26_ACTION', name, 'PASS' if result['pass'] else 'FAIL', result['failure_count'],
              json.dumps(result['metrics']))
        for msg in result['failures']:
            print('   ', msg)
    pairs, too_close = {}, {}
    for a, b in (('FastBowl_R', 'FastMedium_R'), ('FastBowl_R', 'OffSpin_R'), ('FastBowl_R', 'LegSpin_R'),
                 ('OffSpin_R', 'LegSpin_R'), ('FastBowl_R', 'FastBowl_L'), ('OffSpin_R', 'OffSpin_L'),
                 ('Throw', 'ThrowQuick'), ('Catch', 'CatchHigh'), ('Catch', 'CatchLow'),
                 ('Pickup', 'PickupRunning'), ('DiveCatch_L', 'DiveCatch_R')):
        if a in trajs and b in trajs:
            pairs[f'{a}|{b}'] = distinct(trajs[a], trajs[b])
            if pairs[f'{a}|{b}'] < 8.0:
                too_close[f'{a}|{b}'] = pairs[f'{a}|{b}']
    (OUT / 'report.json').write_text(json.dumps({'clips': report, 'distinctness_cm': pairs,
                                                 'too_similar': too_close}, indent=1))
    failed = [r['name'] for r in report if not r['pass']]
    print('C26_DISTINCT', json.dumps(pairs))
    print('C26_ACTION_LAB', 'PASS' if not failed and not too_close else 'FAIL',
          'clips', len(report), 'failed', failed, 'too_similar', too_close)
    return 0 if not failed and not too_close else 1


code = main()
sys.stdout.flush()
if code:
    sys.exit(code)
