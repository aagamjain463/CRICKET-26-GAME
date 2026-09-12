"""Dump what the player meshes and character materials are actually wired to.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ProbePlayerMaterials.py

Context: the MetaHuman route is blocked on this machine (Epic's auto-rig service times out at
300s -> 'Server Error', and Content/Optional/ -- the neural texture-synthesis model -- is not
installed). The plugin's own SKM_Body/SKM_Face archetypes turned out to be untextured gray
scaffolds (single M_GrayTexture_* slot), so they are not a usable fallback either.

What IS present locally and apparently unused is a complete high-resolution PBR character texture
set -- Remy_Body/Bottom/Hair/Shoes/Top x Diffuse/Normal/Gloss/Specular/Opacity -- plus five
character materials (Bodymat, Bottommat, Hairmat, Shoesmat, Eyelashmat).

This probe answers the only question that matters before touching anything:
are those Remy maps already driving the player meshes, or are the meshes sitting on flat/placeholder
materials with 26 MB of real PBR data going unused?

Output goes to the engine log, not stdout -- grep C26_MATPROBE.
"""
import unreal as u

MESHES = [
    '/Game/Cricket26/Characters/SK_Cricketer_Match',
    '/Game/Cricket26/Characters/SK_Cricketer',
    '/Game/Cricket26/Characters/SK_Cricketer_KitBase',
    '/Game/Cricket26/Characters/SK_Cricketer_KitCricket',
]

MATERIALS = [
    '/Game/Cricket26/Characters/Bodymat',
    '/Game/Cricket26/Characters/Bottommat',
    '/Game/Cricket26/Characters/Hairmat',
    '/Game/Cricket26/Characters/Shoesmat',
    '/Game/Cricket26/Characters/Eyelashmat',
]

LIB = u.EditorAssetLibrary


def report(label, value):
    u.log('C26_MATPROBE %s = %s' % (label, value))


def dump_mesh(path):
    short = path.rsplit('/', 1)[-1]
    u.log('C26_MATPROBE ---- MESH %s ----' % short)
    m = LIB.load_asset(path)
    if not m:
        report(short + '.load', 'FAILED (missing)')
        return
    try:
        skel = m.get_editor_property('skeleton')
        report(short + '.skeleton', skel.get_path_name() if skel else 'None')
    except Exception as e:
        report(short + '.skeleton', 'unreadable (%s)' % e)

    try:
        mats = m.get_editor_property('materials')
        report(short + '.slot_count', len(mats))
        for i, sm in enumerate(mats):
            try:
                name = sm.get_editor_property('material_slot_name')
                mi = sm.get_editor_property('material_interface')
                report(short + '.slot_%d' % i,
                       '%s -> %s' % (name, mi.get_path_name() if mi else 'NONE'))
            except Exception as e:
                report(short + '.slot_%d' % i, 'unreadable (%s)' % e)
    except Exception as e:
        report(short + '.materials', 'unreadable (%s)' % e)


def dump_material(path):
    short = path.rsplit('/', 1)[-1]
    u.log('C26_MATPROBE ---- MATERIAL %s ----' % short)
    m = LIB.load_asset(path)
    if not m:
        report(short + '.load', 'FAILED (missing)')
        return
    report(short + '.class', m.get_class().get_name())
    # Walk every texture parameter the material actually references. This is the ground truth for
    # "is the Remy PBR set wired in or not".
    try:
        names = m.get_editor_property('texture_parameter_values')
        report(short + '.texture_param_count', len(names))
        for p in names:
            try:
                info = p.get_editor_property('parameter_info')
                val = p.get_editor_property('parameter_value')
                report(short + '.param',
                       '%s -> %s' % (info.get_editor_property('name'),
                                     val.get_path_name() if val else 'None'))
            except Exception as e:
                report(short + '.param', 'unreadable (%s)' % e)
    except Exception as e:
        report(short + '.texture_parameters', 'unreadable (%s)' % e)


def main():
    u.log('C26_MATPROBE ' + '=' * 70)
    for p in MESHES:
        dump_mesh(p)
    for p in MATERIALS:
        dump_material(p)
    u.log('C26_MATPROBE_DONE')


main()
