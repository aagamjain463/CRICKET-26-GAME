"""Round 7: import the reaction clips and add them to the match profile DA_C26_DefaultPlayer.

Only the Round 7 clips are imported (hash-keyed) and only entries are ADDED to the profile clip map; the
body, equipment, existing clips and approval are left untouched.
Gate first: Blender --background --python ArtSource/Blender/Premium/action_lab.py  (C26_ACTION_LAB PASS)

  UnrealEditor CRICKETGAME.uproject -ExecutePythonScript="$PWD/Tools/ImportRound7Reactions.py"
"""
import json
import runpy
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
KEYS = {'CelebrateRestrained', 'CelebrateEnergetic', 'Appeal', 'HandsOnHead', 'Frustrated', 'Clap'}
for base in ('BatterAcknowledge', 'BatterBeaten', 'BatterEdge', 'BatterReset', 'BatterDismissed'):
    KEYS |= {base + '_R', base + '_L'}

runpy.run_path(str(ROOT / 'Tools/ImportCricketActions.py'), init_globals={'C26_ACTION_KEYS': KEYS},
               run_name='__main__')

manifest = {e['name']: e for e in json.loads(
    (ROOT / 'ArtSource/Premium/AnimationSources/Cricket/manifest.json').read_text())}
profile = u.load_asset('/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer')
assert profile, 'Run Tools/BuildDefaultPlayerProfile.py first'
clips = {str(k): v for k, v in profile.get_editor_property('clips').items()}
before = len(clips)
for name in sorted(KEYS):
    entry = manifest[name]
    seq = u.load_asset('/Game/Cricket26/Characters/Animations/Cricket/' + entry['asset'])
    assert seq, entry['asset']
    assert abs(seq.get_play_length() - entry['length']) < 0.05, (name, seq.get_play_length(), entry['length'])
    clip = u.C26CricketClip()
    clip.set_editor_property('sequence', seq)
    clip.set_editor_property('loop', False)
    clip.set_editor_property('ground_speed', 1.0)
    clip.set_editor_property('event', 'None')
    clip.set_editor_property('blend_seconds', 0.3)
    clips[name] = clip
profile.set_editor_property('clips', clips)
for role in ('BATTER', 'NON_STRIKER', 'BOWLER', 'FIELDER', 'KEEPER', 'UMPIRE'):
    errors = profile.inspect_role(getattr(u.C26VisualRole, role))
    assert not errors, (role, [str(e) for e in errors])
u.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
saved = profile.get_editor_property('clips')
hands = saved[u.Name('HandsOnHead')].get_editor_property('sequence')
assert abs(hands.get_play_length() - manifest['HandsOnHead']['length']) < 0.05, hands.get_play_length()
u.log('C26_ROUND7_PROFILE_READY clips=%d (was %d) added=%s' % (len(saved), before, ','.join(sorted(KEYS))))
u.SystemLibrary.quit_editor()
