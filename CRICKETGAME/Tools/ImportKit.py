"""Import the re-kitted cricketer as a NEW skeletal mesh, bound to the existing skeleton.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ImportKit.py

The base character wears a street top, street trousers and trainers. This build replaces the top
and trousers with garments derived from the body mesh itself (ArtSource/Blender/Characters/
build_kit.py), so they fit exactly and inherit the body's skin weights, and drops the trainers in
favour of the authored cricket shoe attached to the foot bones.

SK_Cricketer_KitBase is left untouched. If anything below fails the game keeps running on it, so
a bad kit import can never take the match down with it.
"""
import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Characters'
NAME = 'SK_Cricketer_KitCricket'
BASE = DEST + '/SK_Cricketer_KitBase'
SRC = u.Paths.project_dir() + 'ArtSource/Exports/C26_KitCricket_v002.fbx'
SURFACE = '/Game/Cricket26/Materials/M_Surface.M_Surface'


def main():
    assert os.path.exists(SRC), 'Missing source FBX: ' + SRC
    base = LIB.load_asset(BASE)
    assert base, 'Base rig missing; nothing to validate against'
    base_bounds = base.get_bounds()
    base_skeleton = base.get_editor_property('skeleton')

    o = u.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    o.import_as_skeletal = True
    o.import_animations = False
    o.import_materials = False
    o.import_textures = False
    # Reuse the existing skeleton. Bone names and hierarchy are unchanged -- only the garment
    # meshes are new -- so binding to the shipped skeleton keeps every asset that references it
    # valid and proves the rig really did round-trip unmodified.
    o.skeleton = base_skeleton
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property('import_morph_targets', False)
    sk.set_editor_property('update_skeleton_reference_pose', False)
    sk.set_editor_property('use_t0_as_ref_pose', False)
    sk.set_editor_property('preserve_smoothing_groups', True)
    sk.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)

    task = u.AssetImportTask()
    task.filename = SRC
    task.destination_path = DEST
    task.destination_name = NAME
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = o
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    kit = LIB.load_asset(DEST + '/' + NAME)
    if not kit:
        u.log_error('C26_KIT_FAIL import produced no asset')
        return
    bounds = kit.get_bounds()
    slots = [str(s.material_slot_name) for s in kit.get_editor_property('materials')]
    height = bounds.box_extent.z * 2.0
    base_height = base_bounds.box_extent.z * 2.0
    same_skeleton = kit.get_editor_property('skeleton') == base_skeleton
    # Orientation and scale must be identical to the shipped rig or every authored IK target,
    # attach transform and contact measurement in AC26Athlete silently moves.
    ok_height = abs(height - base_height) <= 8.0
    ok_width = abs(bounds.box_extent.x - base_bounds.box_extent.x) <= 12.0

    surface = LIB.load_asset(SURFACE)
    mats = kit.get_editor_property('materials')
    for slot in mats:
        slot.set_editor_property('material_interface', surface)
    kit.set_editor_property('materials', mats)
    LIB.save_loaded_asset(kit)

    u.log('C26_KIT height=%.1f (base %.1f) halfwidth=%.1f (base %.1f) skeleton_shared=%s slots=%s'
          % (height, base_height, bounds.box_extent.x, base_bounds.box_extent.x,
             same_skeleton, slots))
    problems = []
    if not ok_height:
        problems.append('height %.1f vs base %.1f' % (height, base_height))
    if not ok_width:
        problems.append('halfwidth %.1f vs base %.1f' % (bounds.box_extent.x, base_bounds.box_extent.x))
    if not same_skeleton:
        problems.append('bound to a different skeleton')
    for want in ('Jersey', 'Trouser', 'Body'):
        if not any(want.lower() in s.lower() for s in slots):
            problems.append('missing %s material slot' % want)
    if problems:
        u.log_error('C26_KIT_FAIL ' + ' | '.join(problems))
    else:
        u.log('C26_KIT_COMPLETE %s verified against %s' % (NAME, 'SK_Cricketer_KitBase'))


main()
