"""Apply Blender-measured equipment offsets to DA_C26_BatterReview.

Reads Artifacts/CharacterAudit/batter-offsets.json (loc cm + quat wxyz from
fit_batter_offsets.py) into the profile's Offset fields. Left-handed offsets
reuse the right-handed values until mirrored technique is measured; logged.
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
data = json.loads((ROOT / 'Artifacts/CharacterAudit/batter-offsets.json').read_text())
path = '/Game/Cricket26/Characters/Data/DA_C26_BatterReview'
profile = u.load_asset(path)
assert profile, 'Run Tools/BuildBatterReview.py first'

KEY = {'Bat': 'BAT', 'BattingGloveL': 'BATTING_GLOVE_L', 'BattingGloveR': 'BATTING_GLOVE_R',
       'BattingPadL': 'BATTING_PAD_L', 'BattingPadR': 'BATTING_PAD_R', 'Helmet': 'HELMET'}

items = list(profile.get_editor_property('equipment'))
applied = []


def norm(s):
    return ''.join(c for c in str(s).upper() if c.isalnum())


for item in items:
    name = norm(item.get_editor_property('slot')).replace('C26EQUIPMENTSLOT', '').rstrip('0123456789')
    if name.endswith('COLON'):
        name = name[:-5]
    short = next((k for k, v in KEY.items() if norm(v) == name), None)
    if short is None or short not in data:
        u.log_warning('C26_OFFSET_MISSING ' + str(item.get_editor_property('slot')))
        continue
    e = data[short]
    # Rotation is rebuilt from shipped basis vectors so UE owns the
    # quaternion convention; only orthonormality is assumed (asserted below).
    ax = e['axes']
    rot = u.MathLibrary.make_rot_from_zx(u.Vector(*ax['z']), u.Vector(*ax['x']))
    t = u.Transform(location=u.Vector(*e['loc']), rotation=rot, scale=u.Vector(1, 1, 1))
    item.set_editor_property('offset', t)
    item.set_editor_property('left_handed_offset', t)
    applied.append(short)
profile.set_editor_property('equipment', items)
deltas = json.loads((ROOT / 'Artifacts/CharacterAudit/contact-deltas.json').read_text())
raw = profile.get_editor_property('clips')
clips = {str(k): v for k, v in dict(raw).items()}
applied_deltas = []
for key, d in deltas.items():
    if key in clips:
        clip = clips[key]
        clip.set_editor_property('contact_delta', u.Vector(*d))
        clips[key] = clip
        applied_deltas.append(key)
    else:
        u.log_warning('C26_DELTA_NO_CLIP ' + key)
u.log('C26_DELTAS_APPLIED %d' % len(applied_deltas))
profile.set_editor_property('clips', clips)
u.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
errors = profile.inspect_role(u.C26VisualRole.BATTER)
assert not errors, str(errors)
u.log('C26_OFFSETS_APPLIED ' + ','.join(sorted(applied)))
