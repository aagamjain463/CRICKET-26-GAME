"""Verify the authored clips actually exist, bound to the right skeleton, with real data.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/VerifyAnimations.py

This is the check that decides whether the clips can ever play in-match. Three things
must all be true, and each is reported explicitly:

  1. the AnimSequence asset exists
  2. its skeleton IS the same object as the skeleton SK_Cricketer_Match skins to
  3. it contains tracks for the bones the athlete body actually poses

(3) matters because UPoseableMeshComponent poses by BONE NAME. A clip whose tracks are
named differently from the skeleton's bones imports "successfully" and then animates
nothing at all - the worst possible failure, because it looks like success.
"""
import unreal as u

LIB = u.EditorAssetLibrary
MESH = '/Game/Cricket26/Characters/SK_Cricketer_Match'
CLIPS = [
    '/Game/Cricket26/Animations/A_C26_BattingDrive',
    '/Game/Cricket26/Animations/A_C26_BowlingPace',
]
# The bones AC26Athlete drives by name. If a clip has no track for these, it cannot
# move the body even though the import reported success.
REQUIRED = [
    'mixamorig:Hips', 'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2',
    'mixamorig:Neck', 'mixamorig:Head',
    'mixamorig:LeftArm', 'mixamorig:LeftForeArm', 'mixamorig:LeftHand',
    'mixamorig:RightArm', 'mixamorig:RightForeArm', 'mixamorig:RightHand',
    'mixamorig:LeftUpLeg', 'mixamorig:LeftLeg', 'mixamorig:LeftFoot',
    'mixamorig:RightUpLeg', 'mixamorig:RightLeg', 'mixamorig:RightFoot',
]


def report(label, value):
    u.log('C26_VERIFY %s = %s' % (label, value))


def main():
    mesh = LIB.load_asset(MESH)
    if not mesh:
        u.log_error('C26_VERIFY_FAIL base mesh missing: %s' % MESH)
        return
    base_skel = mesh.get_editor_property('skeleton')
    if not base_skel:
        u.log_error('C26_VERIFY_FAIL %s has no skeleton' % MESH)
        return
    report('base_mesh', MESH)
    report('base_skeleton', '%s  (%s)' % (base_skel.get_name(), base_skel.get_path_name()))

    # Reference skeleton bone names, so we can compare clip track names against the truth.
    ref_bones = []
    try:
        ref = base_skel.get_editor_property('reference_skeleton')
        ref_bones = [str(b.get_editor_property('name')) for b in ref.get_editor_property('bone_tree')]
    except Exception as e:
        u.log('C26_VERIFY note: could not read reference skeleton bone tree: %s' % e)
    report('skeleton_bone_count', len(ref_bones))

    for path in CLIPS:
        name = path.rsplit('/', 1)[-1]
        u.log('C26_VERIFY ---- %s ----' % name)
        seq = LIB.load_asset(path)
        if not seq:
            u.log_error('C26_VERIFY_FAIL %s: asset does not exist at %s' % (name, path))
            continue

        # 1. exists
        report(name + '.class', seq.get_class().get_name())
        # 2. bound to the SAME skeleton object
        seq_skel = seq.get_editor_property('skeleton')
        report(name + '.skeleton', seq_skel.get_name() if seq_skel else 'None')
        same = seq_skel is not None and seq_skel.get_path_name() == base_skel.get_path_name()
        report(name + '.bound_to_base', same)
        if not same:
            u.log_error('C26_VERIFY_FAIL %s is bound to a DIFFERENT skeleton - it can never play '
                        'on SK_Cricketer_Match' % name)

        # 3. real data
        try:
            report(name + '.length_s', '%.4f' % seq.get_play_length())
        except Exception as e:
            report(name + '.length_s', 'unreadable (%s)' % e)
        try:
            report(name + '.sampled_frames', seq.get_number_of_sampled_frames())
        except Exception as e:
            report(name + '.sampled_frames', 'unreadable (%s)' % e)

        # 4. tracks named like the skeleton's bones
        tracks = []
        try:
            tracks = [str(t.get_editor_property('name')) for t in seq.get_editor_property('track_names')]
        except Exception as e:
            u.log('C26_VERIFY note: track_names unreadable: %s' % e)
        report(name + '.track_count', len(tracks))
        if tracks:
            report(name + '.track_sample', ', '.join(tracks[:6]))

        missing = [b for b in REQUIRED if tracks and b not in tracks]
        report(name + '.required_tracks_present', not missing if tracks else 'unknown')
        if missing:
            u.log_error('C26_VERIFY_FAIL %s missing tracks for: %s' % (name, ', '.join(missing)))

        # Cross-check against the skeleton's real bone names.
        if ref_bones and tracks:
            unknown = [t for t in tracks if t not in ref_bones]
            report(name + '.tracks_not_in_skeleton', len(unknown))
            if unknown:
                report(name + '.unknown_sample', ', '.join(unknown[:6]))

    u.log('C26_VERIFY_DONE')


main()
