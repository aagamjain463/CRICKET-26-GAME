"""Import the refined candidate and assemble ONE unapproved real-match fielder profile.
Reuses the checkpoint's existing canonical skeleton, run and authored fielding clips.
"""
import json
import hashlib
import runpy
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
LIB = u.EditorAssetLibrary
FOLDER = '/Game/Cricket26/Characters/Bodies'
NAME = 'SK_C26_Athlete_Review'
canonical = u.load_asset(FOLDER + '/SK_C26_FullBody_Candidate')
assert canonical
skeleton = canonical.get_editor_property('skeleton')
options = u.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal = True
options.import_animations = options.import_materials = options.import_textures = False
options.skeleton = skeleton
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
options.skeletal_mesh_import_data.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
task = u.AssetImportTask()
task.filename = str(ROOT / 'ArtSource/Premium/FullBody' / (NAME + '.fbx'))
task.destination_path, task.destination_name = FOLDER, NAME
task.automated = task.replace_existing = True
task.options = options
digest = hashlib.sha256(Path(task.filename).read_bytes()).hexdigest()
existing = u.load_asset(FOLDER + '/' + NAME) if LIB.does_asset_exist(FOLDER + '/' + NAME) else None
if existing is None or LIB.get_metadata_tag(existing, 'C26.SourceSHA256') != digest:
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = u.load_asset(FOLDER + '/' + NAME)
assert mesh and mesh.get_editor_property('skeleton') == skeleton
materials = {str(s.material_slot_name): s.material_interface for s in canonical.get_editor_property('materials')}
slots = list(mesh.get_editor_property('materials'))
for slot in slots:
    label = str(slot.material_slot_name)
    assert label in materials, 'Unmapped material: ' + label
    slot.material_interface = materials[label]
mesh.set_editor_property('materials', slots)
editor = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
assert editor.regenerate_lod(mesh, 4, False, False)
assert not u.C26CharacterProfile.inspect_body(mesh)

# Socket translations are authored in the existing bone-local frames. The only ball
# remains the match's ball actor; these are attachment/contact markers, not new props.
for side in ('l', 'r'):
    assert u.C26CharacterProfile.set_equipment_socket(mesh, 'BallHand_' + side.upper(),
        'hand_' + side, u.Transform(location=u.Vector(0, 7, 0)))
LIB.save_loaded_asset(skeleton, only_if_is_dirty=False)
LIB.set_metadata_tag(mesh, 'C26.SourceSHA256', digest)
LIB.save_loaded_asset(mesh, only_if_is_dirty=False)

path = '/Game/Cricket26/Characters/Data/DA_C26_FielderReview'
profile = u.load_asset(path) if LIB.does_asset_exist(path) else None
if profile is None:
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', u.C26CharacterProfile)
    profile = u.AssetToolsHelpers.get_asset_tools().create_asset('DA_C26_FielderReview',
        '/Game/Cricket26/Characters/Data', u.C26CharacterProfile, factory)
profile.set_editor_property('body', mesh)
profile.set_editor_property('skeleton', skeleton)
profile.set_editor_property('approved_for_match', False)
profile.set_editor_property('source_and_license',
    'Project original MetaHuman assembly (Epic license); original project clothing/shoes; '
    'project existing Mixamo A_Run fixture (verify project acquisition record before distribution); '
    'original offline cricket action sources in ArtSource/Blender/Premium/c26_actions.py. '
    'No Cricket 24 or other proprietary game assets. Development review only.')
profile.set_editor_property('visual_review_evidence', 'Unapproved. See Artifacts/CharacterAudit and Docs/PREMIUM_CHARACTER_CONTINUATION.md')
clips = {}
manifest = json.loads((ROOT / 'ArtSource/Premium/AnimationSources/Cricket/manifest.json').read_text())
keys = {'FielderReady', 'BowlerReady', 'Walk', 'Run', 'Start', 'Stop', 'TurnLeft', 'TurnRight',
        'Pickup', 'Throw', 'Catch', 'Celebrate', 'Disappointed',
        'FastBowl_R', 'FastBowl_L', 'OffSpin_R', 'OffSpin_L', 'LegSpin_R', 'LegSpin_L'}
runpy.run_path(str(ROOT / 'Tools/ImportCricketActions.py'),
              init_globals={'C26_ACTION_KEYS': keys - {'Run'}}, run_name='__main__')
for entry in manifest:
    if entry['name'] not in keys:
        continue
    seq = u.load_asset('/Game/Cricket26/Characters/Animations/Cricket/' + entry['asset'])
    assert seq, entry['asset']
    if entry['event']:
        event = entry['event']
        names = [str(n) for n in u.AnimationLibrary.get_animation_notify_event_names(seq)]
        if names.count(event) != 1:
            assert 0 < entry['event_time'] < seq.get_play_length()
            for old_name in (event, 'None'):
                u.AnimationLibrary.remove_animation_notify_events_by_name(seq, old_name)
            tracks = [str(n) for n in u.AnimationLibrary.get_animation_notify_track_names(seq)]
            if event not in tracks:
                u.AnimationLibrary.add_animation_notify_track(seq, event, u.LinearColor(1, .4, .1, 1))
            u.AnimationLibrary.add_animation_notify_event(seq, event, entry['event_time'], getattr(u, 'AnimNotify_' + event))
            LIB.save_loaded_asset(seq, only_if_is_dirty=False)
    clip = u.C26CricketClip()
    clip.set_editor_property('sequence', seq)
    clip.set_editor_property('loop', entry['loop'])
    clip.set_editor_property('ground_speed', max(1, entry['ground_speed']))
    clip.set_editor_property('event', entry['event'] or 'None')
    clips[entry['name']] = clip
# Use the reviewed captured run, not the unreviewed generated gait.
clips['Run'].set_editor_property('sequence', u.load_asset('/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run'))
clips['Run'].set_editor_property('ground_speed', 450.0)
profile.set_editor_property('clips', clips)
assert not profile.inspect_role(u.C26VisualRole.FIELDER), str(profile.inspect_role(u.C26VisualRole.FIELDER))
assert not profile.inspect_role(u.C26VisualRole.BOWLER), str(profile.inspect_role(u.C26VisualRole.BOWLER))
assert profile.inspect_role(u.C26VisualRole.BATTER), 'An incomplete batting role must be rejected'
assert profile.inspect_role(u.C26VisualRole.KEEPER), 'Keeper gear must not be faked'
LIB.save_loaded_asset(profile, only_if_is_dirty=False)

report = {'profile': path, 'body': mesh.get_path_name(), 'approved_for_match': False,
    'role': 'Fielder+Bowler-outfield-review-only', 'clips': sorted(clips),
    'lods': [{'lod': i, 'vertices': editor.get_num_verts(mesh, i),
             'sections': editor.get_num_sections(mesh, i)} for i in range(editor.get_lod_count(mesh))]}
(ROOT / 'Artifacts/CharacterAudit/fielder-review-profile.json').write_text(json.dumps(report, indent=2))
u.log('C26_FIELDER_REVIEW_READY ' + json.dumps(report))
