"""Measure batter equipment offsets from the authored stance (no guessing).

Poses the review rig at BATTER_READY_R, ports the legacy PlaceKit formulas
(bat from the two palms, pads from knee/ankle, gloves from wrist/knuckle,
helmet from head+facing) and expresses each desired world matrix in the
socket bone's posed frame. Units are centimetres, matching the UE import
(height validator certifies the cm scale). Output JSON is applied to
DA_C26_BatterReview by a trivial UE script.
"""
import json
import sys
from pathlib import Path

from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
import c26_rig as rig_lib
import c26_actions as actions

OUT = ROOT / 'Artifacts/CharacterAudit/batter-offsets.json'

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend', keep_mesh=False)
rig_lib.apply(rig, actions.BATTER_READY_R)
pb = rig.pose.bones


def head(name):
    return pb[name].head.copy()


def mat(name):
    return pb[name].matrix.copy()


def make_from_zx(z, x):
    z = z.normalized()
    x = (x - z * x.dot(z)).normalized()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed().to_4x4()


def make_from_xz(x, z):
    x = x.normalized()
    z = (z - x * z.dot(x)).normalized()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed().to_4x4()


# Facing from the feet split (front foot minus back foot, horizontal).
facing = head('foot_l') - head('foot_r')
facing.z = 0
facing.normalize()
up = Vector((0, 0, 1))

offsets = {}
desired_frames = {}
BONES = ('pelvis', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'clavicle_r', 'upperarm_l', 'upperarm_r',
         'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
         'foot_l', 'foot_r', 'ball_l', 'ball_r')


def record(slot, bone, desired):
    desired_frames[slot] = {'bone': bone, 'origin': list(desired.translation),
                            'z': list(desired.col[2].to_3d().normalized()),
                            'x': list(desired.col[0].to_3d().normalized())}
    off = mat(bone).inverted() @ desired
    loc = off.translation
    rot = off.to_3x3()
    cols = [rot.col[i].normalized() for i in range(3)]
    # Round-trip check: bone * offset must reproduce the desired matrix.
    check = mat(bone) @ off
    err = (check.translation - desired.translation).length
    assert err < 0.05, (slot, err)
    offsets[slot] = {'loc': [round(v, 3) for v in loc],
                     'axes': {k: [round(v, 5) for v in cols[i]] for i, k in enumerate('xyz')},
                     'fit_error_cm': round(err, 4)}


def palm(side):
    wrist = head(f'hand_{side}')
    knuckle = head(f'middle_01_{side}')
    return wrist.lerp(knuckle, 0.55)


# Bat: handle spans the two palms; blade (-Z) continues past the bottom hand.
top, bottom = palm('l'), palm('r')
shaft = top - bottom
if shaft.length < 4.0:
    shaft = facing
# The animations orient hand_l so that hand_l @ BAT_OFFSET_L IS the authored bat (c26_rig.apply),
# so the socket must reproduce exactly that frame or every stroke's blade path is wrong in game.
bat = mat('hand_l') @ rig_lib.BAT_OFFSET_L
record('Bat', 'hand_l', bat)

# Gloves: centred on the palm, opening toward the wrist.
for side in ('l', 'r'):
    wrist = head(f'hand_{side}')
    along = (wrist - head(f'lowerarm_{side}')).normalized()
    target = wrist.lerp(palm(side), 1.15) + along * 1.5
    record(f'BattingGlove{side.upper()}', f'hand_{side}',
           Matrix.Translation(target) @ make_from_zx(along, facing).to_4x4())

# Pads: mid-shin, stood off the front of the leg.
for side in ('l', 'r'):
    knee, ankle = head(f'calf_{side}'), head(f'foot_{side}')
    shin = knee - ankle
    target = ankle + shin * 0.56 + shin.normalized() * 3.5 + facing * 0.8
    record(f'BattingPad{side.upper()}', f'calf_{side}',
           Matrix.Translation(target) @ make_from_zx(shin, facing).to_4x4())

# Helmet: skull pivot above the head, facing down the pitch.
skull = head('head') + Vector((0, 0, 10.5)) + facing * 0.8
record('Helmet', 'head', Matrix.Translation(skull) @ make_from_xz(facing, up).to_4x4())

OUT.write_text(json.dumps(offsets, indent=1))
(ROOT / 'Artifacts/CharacterAudit/batter-desired-stance.json').write_text(json.dumps(
    {'bones': {n: list(head(n)) for n in BONES}, 'slots': desired_frames}, indent=1))
print('C26_BATTER_FIT', len(offsets), 'slots ->', OUT)
