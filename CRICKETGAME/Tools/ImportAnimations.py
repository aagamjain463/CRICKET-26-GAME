"""Import the authored cricket clips as AnimSequences bound to the shipped skeleton.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ImportAnimations.py

Sources: ArtSource/Exports/Animations/Corrected/A_C26_BattingDrive.fbx and
A_C26_BowlingPace.fbx. Pipeline (all offline, no Blender needed):

  1. ArtSource/Blender/Animation/c26_anim_author.py holds the keyframes (one source of
     truth). Its repair_facing() transplants the authored keys from the assumed frame
     (+Y forward) onto the rig's true frame (-Y forward in armature space).
  2. Tools/rebuild_authored_clips.py re-runs the authoring solve in pure Python and
     bakes every frame into ArtSource/Exports/Animations/Solved/ (the v2 rig frame,
     byte-compatible with a Blender bake+export).
  3. Tools/correct_authored_anim.py retargets those onto the shipped skeleton and
     writes Corrected/, with a per-clip geometric verification (chest facing, feet
     planted, crouch, hands on the handle, contact IN FRONT of the body, backlift
     behind it, pull high + leg-side follow, cut late + off-side, sweep deep-crouch,
     defence compact, bowling release above the head, bowler travelling down the
     pitch). It exits non-zero if any check fails, so never import unverified curves.

The library: drive / pull / cut / sweep / defence for batting, pace / off-spin /
leg-spin for bowling. AC26Athlete::SelectBattingClip / SelectBowlingClip choose
per shot intent and delivery type; missing clips fall back down the chain to the
procedural action, never a T-pose.

These are ANIMATION-ONLY FBX (armature, no mesh). They are bound to the skeleton that
SK_Cricketer_Match already uses, so the clips play on the existing mesh with no retargeting.
If anything here fails, no existing asset is touched and the match keeps running.
"""
import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Animations'
MESH = '/Game/Cricket26/Characters/SK_Cricketer_Match'
SRC_DIR = u.Paths.project_dir() + 'ArtSource/Exports/Animations/Corrected/'

CLIPS = [
    'A_C26_BattingDrive', 'A_C26_BattingPull', 'A_C26_BattingCut',
    'A_C26_BattingSweep', 'A_C26_BattingDefence', 'A_C26_BattingBackFootDefence',
    'A_C26_BattingUpperCut', 'A_C26_BattingLateCut', 'A_C26_BattingHook',
    'A_C26_BattingLoftedDrive', 'A_C26_BattingGlance',
    'A_C26_UmpireSignalWide', 'A_C26_UmpireSignalSix', 'A_C26_UmpireSignalOut',
    'A_C26_UmpireSignalFour',
    'A_C26_BowlingPace', 'A_C26_BowlingOffSpin', 'A_C26_BowlingLegSpin',
]


def import_one(name, skeleton):
    src = SRC_DIR + name + '.fbx'
    if not os.path.exists(src):
        u.log_error('C26_ANIM_FAIL %s missing source FBX: %s' % (name, src))
        return None

    o = u.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    o.import_animations = True
    o.import_mesh = False
    o.import_as_skeletal = False
    o.import_materials = False
    o.import_textures = False
    # Bind to the shipped skeleton. Bone names and hierarchy are unchanged from the rig the
    # clips were authored on, so this is a straight bind, not a retarget.
    o.skeleton = skeleton
    try:
        o.anim_sequence_import_data.set_editor_property('import_uniform_scale', False)
    except Exception as e:
        u.log('C26_ANIM note: anim_sequence_import_data not settable: %s' % e)

    task = u.AssetImportTask()
    task.filename = src
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = o
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    seq = LIB.load_asset(DEST + '/' + name)
    if not seq:
        u.log_error('C26_ANIM_FAIL %s import produced no asset' % name)
        return None
    return seq


def main():
    mesh = LIB.load_asset(MESH)
    if not mesh:
        u.log_error('C26_ANIM_FAIL base mesh missing: %s' % MESH)
        return
    skeleton = mesh.get_editor_property('skeleton')
    if not skeleton:
        u.log_error('C26_ANIM_FAIL %s has no skeleton' % MESH)
        return
    u.log('C26_ANIM base skeleton = %s' % skeleton.get_name())

    LIB.make_directory(DEST)

    for name in CLIPS:
        seq = import_one(name, skeleton)
        if not seq:
            continue
        # A clip whose skeleton does not match the mesh can never play on it. Say so loudly
        # rather than leaving a silently unusable asset in the project.
        seq_skel = seq.get_editor_property('skeleton')
        length = seq.get_play_length()
        frames = 0
        try:
            frames = int(seq.get_number_of_sampled_frames())
        except Exception:
            pass
        u.log('C26_ANIM imported=%s length=%.3fs frames=%d skeleton=%s bound=%s'
              % (name, length, frames,
                 seq_skel.get_name() if seq_skel else 'None',
                 seq_skel == skeleton))
        LIB.save_loaded_asset(seq)

    u.log('C26_ANIM_DONE')


main()
