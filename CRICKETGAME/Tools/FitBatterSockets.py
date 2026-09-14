"""Solve DA_C26_BatterReview equipment offsets in UE's own bone frames.

The old offsets were Blender bone-space numbers written straight into UE sockets. Blender and UE
bone frames differ, so in game the bat stuck out sideways from the hands (left hand no longer the
top hand) and the pads floated off the shins, although every authored stroke was correct.

Inputs, both measured at the stance frame of the same authored clip:
  Artifacts/CharacterAudit/batter-desired-stance.json  (fit_batter_offsets.py, Blender armature cm)
  Artifacts/CharacterAudit/ue-stance-bones.json        (-C26ShotReview -C26StanceOnly, UE component cm)
Blender armature -> UE component is fitted from the bone positions (a reflection is expected), each
desired equipment frame is mapped through it and expressed in its UE socket bone's frame.

Run inside the editor: UnrealEditor CRICKETGAME.uproject -ExecutePythonScript=Tools/FitBatterSockets.py
"""
import json
from pathlib import Path

import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
desired = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-desired-stance.json').read_text())
ue = json.loads((ROOT / 'Artifacts/CharacterAudit/ue-stance-bones.json').read_text())


def sub(a, b):
    return [a[i] - b[i] for i in range(3)]


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def mat_vec(m, v):
    return [dot(m[r], v) for r in range(3)]


def inv3(m):
    a, b, c = m[0]; d, e, f = m[1]; g, h, i = m[2]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    return [[(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det],
            [(f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det],
            [(d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det]], det


def norm(v):
    n = dot(v, v) ** 0.5
    return [c / n for c in v]


# Least-squares linear map about the pelvis: M = (sum b a^T)(sum a a^T)^-1
names = [n for n in desired['bones'] if n in ue and n != 'pelvis']
pb, pu = desired['bones']['pelvis'], ue['pelvis']['loc']
BA = [[0.0] * 3 for _ in range(3)]
AA = [[0.0] * 3 for _ in range(3)]
for n in names:
    a, b = sub(desired['bones'][n], pb), sub(ue[n]['loc'], pu)
    for r in range(3):
        for c in range(3):
            BA[r][c] += b[r] * a[c]
            AA[r][c] += a[r] * a[c]
AAi, _ = inv3(AA)
M = [[sum(BA[r][k] * AAi[k][c] for k in range(3)) for c in range(3)] for r in range(3)]
_, det = inv3(M)
residual = max(dot(sub(mat_vec(M, sub(desired['bones'][n], pb)), sub(ue[n]['loc'], pu)),
                   sub(mat_vec(M, sub(desired['bones'][n], pb)), sub(ue[n]['loc'], pu))) ** 0.5 for n in names)
u.log('C26_SOCKET_FIT map det=%.3f max_bone_residual=%.2fcm rows=%s' % (det, residual, M))
assert residual < 1.0, 'Blender and UE stance poses differ (%.2fcm): re-import the clips first' % residual

offsets = {}
for slot, frame in desired['slots'].items():
    bone = ue[frame['bone']]
    origin = [pu[i] + c for i, c in enumerate(mat_vec(M, sub(frame['origin'], pb)))]
    z, x = norm(mat_vec(M, frame['z'])), norm(mat_vec(M, frame['x']))
    d = sub(origin, bone['loc'])
    axes = [bone['x'], bone['y'], bone['z']]
    offsets[slot] = {'loc': [dot(d, a) for a in axes], 'z': [dot(z, a) for a in axes], 'x': [dot(x, a) for a in axes]}

SLOT = {'Bat': 'BAT', 'BattingGloveL': 'BATTING_GLOVE_L', 'BattingGloveR': 'BATTING_GLOVE_R',
        'BattingPadL': 'BATTING_PAD_L', 'BattingPadR': 'BATTING_PAD_R', 'Helmet': 'HELMET'}
path = '/Game/Cricket26/Characters/Data/DA_C26_BatterReview'
profile = u.load_asset(path)
items = list(profile.get_editor_property('equipment'))
applied = []
for item in items:
    slot = item.get_editor_property('slot')
    key = next((k for k, v in SLOT.items() if getattr(u.C26EquipmentSlot, v) == slot), None)
    if key not in offsets:
        continue
    e = offsets[key]
    t = u.Transform(location=u.Vector(*e['loc']),
                    rotation=u.MathLibrary.make_rot_from_zx(u.Vector(*e['z']), u.Vector(*e['x'])),
                    scale=u.Vector(1, 1, 1))
    item.set_editor_property('offset', t)
    item.set_editor_property('left_handed_offset', t)
    applied.append(key)
profile.set_editor_property('equipment', items)
u.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
(ROOT / 'Artifacts/CharacterAudit/batter-offsets-ue.json').write_text(json.dumps(offsets, indent=1))
u.log('C26_SOCKET_FIT_APPLIED %s' % applied)
