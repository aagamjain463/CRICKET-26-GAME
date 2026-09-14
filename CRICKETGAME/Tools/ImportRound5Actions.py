"""Round 5: import the bowling, fielding and keeping library and add it to the outfield review profile.

Only the Round 5 clips are imported (hash-keyed, so unchanged sources are skipped) and only the clip
map of DA_C26_FielderReview is touched - the body, skeleton, sockets and LODs are left alone.
Gate first: Blender --background --python ArtSource/Blender/Premium/action_lab.py  (C26_ACTION_LAB PASS)

  UnrealEditor CRICKETGAME.uproject -ExecutePythonScript="$PWD/Tools/ImportRound5Actions.py"
"""
import json
import runpy
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
KEYS = {'Pickup', 'PickupRunning', 'Throw', 'ThrowQuick', 'Catch', 'CatchHigh', 'CatchLow', 'SlideSave',
        'DiveCatch_L', 'DiveCatch_R', 'DiveStop_L', 'DiveStop_R',
        'KeeperReady', 'KeeperShuffle_L', 'KeeperShuffle_R', 'KeeperReceive', 'KeeperTakeLow',
        'KeeperDive_L', 'KeeperDive_R',
        'FastBowl_R', 'FastBowl_L', 'FastMedium_R', 'FastMedium_L',
        'OffSpin_R', 'OffSpin_L', 'LegSpin_R', 'LegSpin_L'}

runpy.run_path(str(ROOT / 'Tools/ImportCricketActions.py'), init_globals={'C26_ACTION_KEYS': KEYS},
               run_name='__main__')

manifest = {e['name']: e for e in json.loads(
    (ROOT / 'ArtSource/Premium/AnimationSources/Cricket/manifest.json').read_text())}
profile = u.load_asset('/Game/Cricket26/Characters/Data/DA_C26_FielderReview')
assert profile, 'Run Tools/BuildFielderMatchReview.py first'
clips = dict(profile.get_editor_property('clips'))
for name in sorted(KEYS):
    entry = manifest[name]
    seq = u.load_asset('/Game/Cricket26/Characters/Animations/Cricket/' + entry['asset'])
    assert seq, entry['asset']
    if entry['event']:
        names = [str(n) for n in u.AnimationLibrary.get_animation_notify_event_names(seq)]
        assert names.count(entry['event']) == 1, (name, names)
    clip = u.C26CricketClip()
    clip.set_editor_property('sequence', seq)
    clip.set_editor_property('loop', entry['loop'])
    clip.set_editor_property('ground_speed', max(1, entry['ground_speed']))
    clip.set_editor_property('event', entry['event'] or 'None')
    clips[name] = clip
profile.set_editor_property('clips', clips)
assert not profile.inspect_role(u.C26VisualRole.FIELDER), str(profile.inspect_role(u.C26VisualRole.FIELDER))
assert not profile.inspect_role(u.C26VisualRole.BOWLER), str(profile.inspect_role(u.C26VisualRole.BOWLER))
u.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
u.log('C26_ROUND5_PROFILE_READY clips=%d added=%s' % (len(clips), ','.join(sorted(KEYS))))
