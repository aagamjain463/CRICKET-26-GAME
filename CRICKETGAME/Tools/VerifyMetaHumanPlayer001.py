"""Verify what MH_C26_Player_001 actually produced on disk after an assemble.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/VerifyMetaHumanPlayer001.py

The point of this script is to answer one question honestly: did `build_meta_human` write a real
skeletal mesh that AC26MetaHumanPlayer can load, or did it silently produce nothing?

AC26MetaHumanPlayer::BodyMeshPath() hard-codes

    /Game/Cricket26/Characters/MetaHumans/Players/Player_001/Body/SKM_MH_C26_Player_001.SKM_MH_C26_Player_001

so that exact path is checked first and reported separately from a general folder listing. If the
pipeline named the mesh differently, the listing is what tells us the real name to point the C++
at -- the class is not to be guessed at.

Note that `unreal.log()` does not reach stdout under `-run=pythonscript`; read the results from
~/Library/Logs/Unreal Engine/CRICKETGAMEEditor/CRICKETGAME.log and grep for C26_MHVERIFY.
"""
import unreal as u

PLAYER_DIR = '/Game/Cricket26/Characters/MetaHumans/Players/Player_001'
CHARACTER = PLAYER_DIR + '/MH_C26_Player_001'
EXPECTED_BODY = PLAYER_DIR + '/Body/SKM_MH_C26_Player_001'
COMMON_DIR = '/Game/Cricket26/Characters/MetaHumans/Common'

# Skeleton bones AC26MetaHumanPlayer::FSocketBones declares for equipment attachment. If the
# assembled body lacks these, equipment cannot be socketed onto it.
SOCKET_BONES = ['hand_r', 'hand_l', 'head', 'calf_r', 'calf_l']

LIB = u.EditorAssetLibrary
AR = u.AssetRegistryHelpers.get_asset_registry()


def report(label, value):
    u.log('C26_MHVERIFY %s = %s' % (label, value))


def list_dir(path, label):
    report('%s.exists' % label, LIB.does_directory_exist(path))
    if not LIB.does_directory_exist(path):
        return []
    try:
        assets = LIB.list_assets(path, recursive=True, include_folder=False)
    except Exception as e:
        report('%s.list_error' % label, e)
        return []
    report('%s.asset_count' % label, len(assets))
    for a in sorted(assets):
        report('%s.asset' % label, a)
    return assets


def main():
    u.log('C26_MHVERIFY ' + '=' * 70)
    report('character.exists', LIB.does_asset_exist(CHARACTER))
    report('character.loads', LIB.load_asset(CHARACTER) is not None)

    u.log('C26_MHVERIFY ' + '-' * 70)
    report('expected_body.exists', LIB.does_asset_exist(EXPECTED_BODY))

    u.log('C26_MHVERIFY ' + '-' * 70)
    assets = list_dir(PLAYER_DIR, 'player_dir')
    list_dir(COMMON_DIR, 'common_dir')

    # Find every skeletal mesh the build produced, wherever it landed, and inspect each one.
    meshes = [a for a in assets if AR.get_asset_by_object_path(a).asset_class_path.asset_name == 'SkeletalMesh']
    report('skeletal_mesh_count', len(meshes))

    for a in meshes:
        u.log('C26_MHVERIFY ---- %s ----' % a.rsplit('/', 1)[-1])
        m = LIB.load_asset(a)
        if not m:
            report(a + '.load', 'FAILED')
            continue
        report(a + '.path', m.get_path_name())
        try:
            skel = m.get_editor_property('skeleton')
            report(a + '.skeleton', skel.get_name() if skel else 'None')
        except Exception as e:
            report(a + '.skeleton', 'unreadable (%s)' % e)

        # Bone names matter because equipment sockets bind by name.
        bones = []
        try:
            bones = [str(b) for b in m.get_editor_property('bone_names')]
        except Exception as e:
            report(a + '.bone_names', 'unreadable (%s)' % e)
        report(a + '.bone_count', len(bones))
        if bones:
            missing = [b for b in SOCKET_BONES if b not in bones]
            report(a + '.socket_bones_present', not missing)
            if missing:
                report(a + '.socket_bones_missing', ', '.join(missing))

        # Materials: with bBakeMaterials off these are the pipeline's own full-fidelity set.
        try:
            mats = m.get_editor_property('materials')
            report(a + '.material_slot_count', len(mats))
            for i, sm in enumerate(mats):
                try:
                    report(a + '.slot_%d' % i,
                           '%s -> %s' % (sm.get_editor_property('material_slot_name'),
                                         sm.get_editor_property('material_interface').get_name()
                                         if sm.get_editor_property('material_interface') else 'None'))
                except Exception as e:
                    report(a + '.slot_%d' % i, 'unreadable (%s)' % e)
        except Exception as e:
            report(a + '.materials', 'unreadable (%s)' % e)

        # LOD count: the pipeline is supposed to emit several.
        try:
            report(a + '.lod_count', len(m.get_editor_property('lod_models')))
        except Exception as e:
            report(a + '.lod_count', 'unreadable (%s)' % e)

    u.log('C26_MHVERIFY_DONE')


main()
