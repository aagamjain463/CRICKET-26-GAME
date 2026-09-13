"""Import the authored cricket action library onto the candidate skeleton.

Every clip was authored on this exact rig, so it imports directly as an AnimSequence with no
retargeter involved. That is the point: the existing RTG_C26_MatchToFullBody retargets of the old
batting and bowling sources land the athlete flat on the ground because those sources carry a
different root frame, and no amount of tuning a retarget pose fixes a clip that was never authored
for this skeleton.

Run inside the full editor (the commandlet path crashes on skeletal FBX in 5.8):
  UnrealEditor CRICKETGAME.uproject -ExecutePythonScript=".../Tools/ImportCricketActions.py"
"""
import json
import unreal as u
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'ArtSource/Premium/AnimationSources/Cricket'
DEST = '/Game/Cricket26/Characters/Animations/Cricket'
SKELETON = '/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate_Skeleton'

skeleton = u.load_asset(SKELETON)
assert skeleton, 'Missing candidate skeleton; run Tools/ImportPremiumBody.py first'
manifest = json.loads((SRC / 'manifest.json').read_text())

options = u.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
options.import_mesh = False
options.import_animations = True
options.import_as_skeletal = True
options.skeleton = skeleton
anim = options.anim_sequence_import_data
for key, value in [('import_custom_attribute', True), ('remove_redundant_keys', False),
                   ('animation_length', u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME),
                   ('convert_scene', True), ('force_front_x_axis', False),
                   ('convert_scene_unit', True), ('use_default_sample_rate', False),
                   ('custom_sample_rate', 30)]:
    anim.set_editor_property(key, value)

tasks = []
for clip in manifest:
    existing = u.load_asset(f"{DEST}/{clip['asset']}")
    if existing and abs(existing.get_editor_property('sequence_length') - clip['length']) < 0.05:
        continue                      # already imported at the authored length; only notifies need fixing
    task = u.AssetImportTask()
    task.filename = str(ROOT / clip['fbx'])
    task.destination_path = DEST
    task.destination_name = clip['asset']
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = options
    tasks.append(task)
if tasks:
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
print('C26_REIMPORTED %d of %d' % (len(tasks), len(manifest)))

report, failures = [], []
for clip in manifest:
    path = f"{DEST}/{clip['asset']}"
    seq = u.load_asset(path)
    if not seq:
        failures.append(f"{clip['name']}: import produced no AnimSequence")
        continue
    # Locomotion is driven by the match's own transforms; an in-place clip with root motion would
    # fight them and slide the athlete. The profile validator rejects root motion for this reason.
    seq.set_editor_property('enable_root_motion', False)
    length = seq.get_editor_property('sequence_length')

    if clip['event']:
        # One interior notify per action clip, named exactly as the profile expects: the validator
        # matches FAnimNotifyEvent::NotifyName against FC26CricketClip::Event. Passing no notify
        # class makes the track name the notify name, which is what we want.
        t = min(max(clip['event_time'], 0.02), length - 0.02)
        track = clip['event']
        notify_class = getattr(u, 'AnimNotify_' + track, None)
        if notify_class is None:
            failures.append('%s: no UAnimNotify_%s class; rebuild the game module' % (clip['name'], track))
            continue
        u.AnimationLibrary.remove_all_animation_notify_events(seq)
        try:
            u.AnimationLibrary.add_animation_notify_track(seq, track, u.LinearColor(1, .4, .1, 1))
        except Exception:
            pass
        u.AnimationLibrary.add_animation_notify_event(seq, track, t, notify_class)
        try:
            names = [str(n) for n in u.AnimationLibrary.get_animation_notify_event_names(seq)]
        except Exception:
            names = ['<unreadable>']
        if track not in names:
            failures.append("%s: notify named %s, expected %s" % (clip['name'], names, track))
        report.append("%s: %s @ %.3fs of %.3fs -> %s" % (clip['name'], track, t, length, names))

    delta = abs(length - clip['length'])
    if delta > 0.05:
        failures.append(f"{clip['name']}: length {length:.3f}s, authored {clip['length']:.3f}s")

u.EditorAssetLibrary.save_directory(DEST, False, True)
out = ROOT / 'Artifacts/CharacterAudit/import-cricket-actions.json'
out.write_text(json.dumps({'imported': len(manifest) - len(failures),
                           'total': len(manifest),
                           'failures': failures, 'notifies': report}, indent=1))
for f in failures:
    u.log_error('C26_IMPORT_FAIL ' + f)
print('C26_IMPORTED %d/%d failures=%d' % (len(manifest) - len(failures), len(manifest), len(failures)))
