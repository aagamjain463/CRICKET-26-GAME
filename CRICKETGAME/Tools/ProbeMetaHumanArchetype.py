"""Probe the MetaHuman archetype meshes that ship with the plugin.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ProbeMetaHumanArchetype.py

Epic's cloud auto-rig service is timing out (HTTP 300s -> 'Server Error') and the local texture
synthesis model is not installed (Content/Optional/ absent), so Tools/MetaHumanBuildPlayer001.py
cannot assemble a custom MetaHuman right now. These archetype meshes ship inside the plugin and
need neither -- they are the MetaHuman identity templates the pipeline itself starts from.

Question this answers: can they serve as the player visual, i.e. do they have a real skeleton with
the equipment bone names AC26MetaHumanPlayer::FSocketBones expects, usable materials, and a sane
height?

Output goes to the engine log, not stdout -- grep C26_MHARCH.
"""
import unreal as u

CANDIDATES = [
    '/MetaHumanCharacter/Body/IdentityTemplate/SKM_Body',
    '/MetaHumanCharacter/Body/IdentityTemplate/SKM_Body_DNA',
    '/MetaHumanCharacter/Face/SKM_Face',
    '/MetaHumanCharacter/Face/SKM_Face_DNA',
]

# Equipment bones AC26MetaHumanPlayer::FSocketBones declares.
SOCKET_BONES = ['hand_r', 'hand_l', 'head', 'calf_r', 'calf_l']
# MetaHuman skeleton landmarks worth confirming the naming convention against.
LANDMARKS = ['root', 'pelvis', 'spine_01', 'neck_01', 'clavicle_r', 'upperarm_r', 'lowerarm_r',
             'thigh_r', 'calf_r', 'foot_r']


def report(label, value):
    u.log('C26_MHARCH %s = %s' % (label, value))


def main():
    u.log('C26_MHARCH ' + '=' * 70)
    for path in CANDIDATES:
        short = path.rsplit('/', 1)[-1]
        u.log('C26_MHARCH ---- %s ----' % short)

        # Plugin content is NOT in the asset registry: EditorAssetLibrary.load_asset returns None.
        # unreal.load_object(None, path) is the way in.
        obj = u.load_object(None, path)
        if obj is None:
            report(short + '.load_object', 'None (does not exist?)')
            continue
        report(short + '.class', obj.get_class().get_name())

        if obj.get_class().get_name() != 'SkeletalMesh':
            report(short + '.note', 'not a SkeletalMesh, skipping mesh checks')
            continue

        report(short + '.path', obj.get_path_name())

        # Skeleton + bone names -- the whole point of this probe.
        try:
            skel = obj.get_editor_property('skeleton')
            report(short + '.skeleton', skel.get_name() if skel else 'None')
            if skel:
                report(short + '.skeleton_path', skel.get_path_name())
        except Exception as e:
            report(short + '.skeleton', 'unreadable (%s)' % e)

        bones = []
        try:
            bones = [str(b) for b in obj.get_editor_property('bone_names')]
        except Exception as e:
            report(short + '.bone_names', 'unreadable (%s)' % e)
        report(short + '.bone_count', len(bones))
        if bones:
            report(short + '.bone_sample', ', '.join(bones[:12]))
            missing = [b for b in SOCKET_BONES if b not in bones]
            report(short + '.socket_bones_present', not missing)
            if missing:
                report(short + '.socket_bones_missing', ', '.join(missing))
            found = [b for b in LANDMARKS if b in bones]
            report(short + '.landmarks_present', ', '.join(found))

        # Materials -- without texture synthesis these must already carry usable textures.
        try:
            mats = obj.get_editor_property('materials')
            report(short + '.material_slot_count', len(mats))
            for i, sm in enumerate(mats):
                try:
                    mi = sm.get_editor_property('material_interface')
                    report(short + '.slot_%d' % i,
                           '%s -> %s' % (sm.get_editor_property('material_slot_name'),
                                         mi.get_path_name() if mi else 'None'))
                except Exception as e:
                    report(short + '.slot_%d' % i, 'unreadable (%s)' % e)
        except Exception as e:
            report(short + '.materials', 'unreadable (%s)' % e)

        # Height matters: the project's rig convention is +Z up, and AC26Athlete applies a 0.48
        # ground scale when a bound mesh is taller than 250 units.
        try:
            b = obj.get_editor_property('bounds')
            report(short + '.bounds', b)
        except Exception as e:
            report(short + '.bounds', 'unreadable (%s)' % e)
        try:
            report(short + '.imported_bounds_min', obj.get_bounds().min if hasattr(obj, 'get_bounds') else 'n/a')
        except Exception as e:
            report(short + '.imported_bounds', 'unreadable (%s)' % e)

        try:
            report(short + '.lod_count', len(obj.get_editor_property('lod_models')))
        except Exception as e:
            report(short + '.lod_count', 'unreadable (%s)' % e)

    u.log('C26_MHARCH_DONE')


main()
