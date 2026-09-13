"""Assemble ONE unapproved real-match batter review profile on the review body.

Review-only: pads/gloves/bat/helmet on the new skeleton with fixed socket offsets,
plus the authored shot library with BatContact markers. Nothing here approves a
match migration; batter remains OLD visuals until stance/garment/equipment gates pass.

Iteration protocol: offsets start at measured v1 values below. After each review
capture, adjust ONLY the documented offset constants, re-run this script, re-capture.
Do not hand-tweak assets in the editor (that state is lost and unreviewable).
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
LIB = u.EditorAssetLibrary
BODY_PATH = '/Game/Cricket26/Characters/Bodies/SK_C26_Athlete_Review'
GEAR = '/Game/Cricket26/Equipment'

# ---------------------------------------------------------------- v1 offsets
# Socket: (socket name, bone). All sockets sit at their bone origin, identity
# rotation; the equipment Offset below carries the authored-frame alignment.
# Equipment frames (see ArtSource/Blender/Equipment build scripts):
#   Bat:    origin top of handle, blade -Z, face +X
#   Glove:  origin palm, +Z wrist->fingertip, +X back of hand
#   Pad:    origin mid-shin, +Z up the shin, +X out of the leg front
#   Helmet: origin skull pivot, +X out of the face, +Z up
# v1: identity everywhere except a 90-degree bat roll guess is WITHHELD on
# purpose -- the first capture shows the raw frame mismatch, then we measure.
BAT_OFFSET = u.Transform(location=u.Vector(0, 0, 0))
GLOVE_OFFSET = u.Transform(location=u.Vector(0, 0, 0))
PAD_OFFSET = u.Transform(location=u.Vector(0, 0, 0))
HELMET_OFFSET = u.Transform(location=u.Vector(0, 0, 0))

mesh = u.load_asset(BODY_PATH)
assert mesh, 'Run Tools/BuildFielderMatchReview.py first'
skeleton = mesh.get_editor_property('skeleton')

for name, bone in [('BatGrip_L', 'hand_l'), ('BatGrip_R', 'hand_r'),
                   ('Glove_L', 'hand_l'), ('Glove_R', 'hand_r'),
                   ('Helmet', 'head'),
                   ('PadMount_L', 'calf_l'), ('PadMount_R', 'calf_r')]:
    assert u.C26CharacterProfile.set_equipment_socket(
        mesh, name, bone, u.Transform()), 'socket ' + name
LIB.save_loaded_asset(skeleton, only_if_is_dirty=False)

items = []
for slot, asset, socket, mirrored, offset in [
        ('BAT', 'SM_C26_Bat_Hero', 'BatGrip_L', 'BatGrip_R', BAT_OFFSET),
        ('BATTING_GLOVE_L', 'SM_C26_Glove_L', 'Glove_L', 'Glove_L', GLOVE_OFFSET),
        ('BATTING_GLOVE_R', 'SM_C26_Glove_R', 'Glove_R', 'Glove_R', GLOVE_OFFSET),
        ('BATTING_PAD_L', 'SM_C26_Pad_L', 'PadMount_L', 'PadMount_L', PAD_OFFSET),
        ('BATTING_PAD_R', 'SM_C26_Pad_R', 'PadMount_R', 'PadMount_R', PAD_OFFSET),
        ('HELMET', 'SM_C26_Helmet_Hero', 'Helmet', 'Helmet', HELMET_OFFSET)]:
    static = u.load_asset(GEAR + '/' + asset)
    assert static, asset
    assert mesh.find_socket(socket), socket
    item = u.C26EquipmentDefinition()
    item.set_editor_property('slot', getattr(u.C26EquipmentSlot, slot))
    item.set_editor_property('mesh', static)
    item.set_editor_property('socket', socket)
    item.set_editor_property('left_handed_socket', mirrored)
    item.set_editor_property('offset', offset)
    items.append(item)

manifest = json.loads((ROOT / 'ArtSource/Premium/AnimationSources/Cricket/manifest.json').read_text())
BAT_KEYS = {'BatterReady_R', 'BatterReady_L', 'BatterRun_R', 'BatterRun_L',
            'BatterCelebrate_R', 'BatterCelebrate_L', 'Walk', 'Run', 'Start', 'Stop',
            'TurnLeft', 'TurnRight', 'Celebrate', 'Disappointed'}
for shot in ('COVERDRIVE', 'STRAIGHTDRIVE', 'ONDRIVE', 'PULL', 'SWEEP', 'LEGGLANCE',
             'FRONTFOOTDEFENCE'):
    BAT_KEYS.add(shot + '_R')
    BAT_KEYS.add(shot + '_L')
clips = {}
for entry in manifest:
    if entry['name'] not in BAT_KEYS:
        continue
    seq = u.load_asset('/Game/Cricket26/Characters/Animations/Cricket/' + entry['asset'])
    assert seq, entry['asset']
    if entry['event']:
        names = [str(n) for n in u.AnimationLibrary.get_animation_notify_event_names(seq)]
        if names.count(entry['event']) != 1:
            u.log_error('C26_BATTER_CLIP_BAD_MARKER ' + entry['name'] + ' ' + str(names))
            continue
    clip = u.C26CricketClip()
    clip.set_editor_property('sequence', seq)
    clip.set_editor_property('loop', entry['loop'])
    clip.set_editor_property('ground_speed', max(1, entry['ground_speed']))
    clip.set_editor_property('event', entry['event'] or 'None')
    clips[entry['name']] = clip
clips['Run'].set_editor_property(
    'sequence', u.load_asset('/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run'))

path = '/Game/Cricket26/Characters/Data/DA_C26_BatterReview'
profile = u.load_asset(path) if LIB.does_asset_exist(path) else None
if profile is None:
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', u.C26CharacterProfile)
    profile = u.AssetToolsHelpers.get_asset_tools().create_asset(
        'DA_C26_BatterReview', '/Game/Cricket26/Characters/Data', u.C26CharacterProfile, factory)
profile.set_editor_property('body', mesh)
profile.set_editor_property('skeleton', skeleton)
profile.set_editor_property('equipment', items)
profile.set_editor_property('clips', clips)
profile.set_editor_property('approved_for_match', False)
profile.set_editor_property('source_and_license',
    'Project original MetaHuman assembly (Epic license); original project clothing/shoes; '
    'original authored equipment in ArtSource/Blender/Equipment; original offline cricket '
    'action sources in ArtSource/Blender/Premium/c26_actions.py. No proprietary game assets. '
    'Development review only.')
profile.set_editor_property('visual_review_evidence', 'Unapproved v1 identity offsets.')
errors = profile.inspect_role(u.C26VisualRole.BATTER)
assert not errors, str(errors)
LIB.save_loaded_asset(profile, only_if_is_dirty=False)
u.log('C26_BATTER_REVIEW_READY clips=%d equipment=%d' % (len(clips), len(items)))
