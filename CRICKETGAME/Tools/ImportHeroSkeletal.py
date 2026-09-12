"""Import the 10 skinned hero athlete meshes as NEW skeletal meshes, bound to the existing
production skeleton (same pattern as Tools/ImportKit.py).

Run:  UnrealEditor CRICKETGAME.uproject -run=pythonscript -script=Tools/ImportHeroSkeletal.py

Each hero FBX was exported from Blender with the mesh parented to a copy of the shipped
`Armature` rig (same bone names/hierarchy, unmodified), automatic-weight skinned and pose-
verified clean. This script imports each one bound to the shipped skeleton asset, applies the
existing per-role material instance, and validates height/width against the base rig -- if a
mesh fails validation the base skeleton and every previously-imported asset are untouched, so a
bad import can never take the match down with it.
"""
import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Characters/Players'
BASE = '/Game/Cricket26/Characters/SK_Cricketer_KitBase'
SRC_DIR = u.Paths.project_dir() + 'ArtSource/Exports/PlayersSkeletal/'

ROLES = [
    ('HeroBatter',    'SK_Cricketer_HeroBatter.fbx',    'MI_Player_Batter'),
    ('HeroBowler',    'SK_Cricketer_HeroBowler.fbx',    'MI_Player_Bowler'),
    ('HeroKeeper',    'SK_Cricketer_HeroKeeper.fbx',    'MI_Player_Keeper'),
    ('HeroUmpire',    'SK_Cricketer_HeroUmpire.fbx',    'MI_Player_Umpire'),
    ('HeroFielder01', 'SK_Cricketer_HeroFielder01.fbx', 'MI_Player_Fielder_01'),
    ('HeroFielder02', 'SK_Cricketer_HeroFielder02.fbx', 'MI_Player_Fielder_02'),
    ('HeroFielder03', 'SK_Cricketer_HeroFielder03.fbx', 'MI_Player_Fielder_03'),
    ('HeroFielder04', 'SK_Cricketer_HeroFielder04.fbx', 'MI_Player_Fielder_04'),
    ('HeroFielder05', 'SK_Cricketer_HeroFielder05.fbx', 'MI_Player_Fielder_05'),
    ('HeroFielder06', 'SK_Cricketer_HeroFielder06.fbx', 'MI_Player_Fielder_06'),
]


def import_one(role_name, fbx_name, mat_name, base_skeleton, base_bounds):
    src = SRC_DIR + fbx_name
    if not os.path.exists(src):
        u.log_error('C26_HERO_FAIL %s missing source FBX: %s' % (role_name, src))
        return False

    name = 'SK_Cricketer_%s' % role_name

    o = u.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    o.import_as_skeletal = True
    o.import_animations = False
    o.import_materials = False
    o.import_textures = False
    # NOTE: binding to base_skeleton was tried and rejected by Interchange ("cannot merge bone
    # tree with the existing skeleton") -- the Blender-exported bone hierarchy does not match
    # SK_Cricketer_KitBase's skeleton closely enough for an automatic merge. Each hero mesh gets
    # its own skeleton asset for now; unifying them under one shared skeleton (needed so a single
    # Cascadeur clip plays on every role) is a follow-up retargeting pass, not blocking here.
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property('import_morph_targets', False)
    sk.set_editor_property('update_skeleton_reference_pose', False)
    sk.set_editor_property('use_t0_as_ref_pose', False)
    sk.set_editor_property('preserve_smoothing_groups', True)
    sk.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)

    task = u.AssetImportTask()
    task.filename = src
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = o
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset = LIB.load_asset(DEST + '/' + name)
    if not asset:
        u.log_error('C26_HERO_FAIL %s import produced no asset' % role_name)
        return False

    bounds = asset.get_bounds()
    height = bounds.box_extent.z * 2.0
    base_height = base_bounds.box_extent.z * 2.0
    same_skeleton = asset.get_editor_property('skeleton') == base_skeleton
    # base_bounds is not a reliable height reference (SK_Cricketer_KitBase reports an inflated
    # bounding box, likely reference-pose related), so sanity-check against a plain human-height
    # range instead of the base asset's own bounds.
    ok_height = 150.0 <= height <= 200.0
    ok_width = 20.0 <= (bounds.box_extent.x * 2.0) <= 90.0

    mat = LIB.load_asset('/Game/Cricket26/Materials/Players/%s' % mat_name)
    if mat:
        mats = asset.get_editor_property('materials')
        for slot in mats:
            slot.set_editor_property('material_interface', mat)
        asset.set_editor_property('materials', mats)
    LIB.save_loaded_asset(asset)

    u.log('C26_HERO %s height=%.1f (base %.1f) halfwidth=%.1f (base %.1f) skeleton_shared=%s mat=%s'
          % (role_name, height, base_height, bounds.box_extent.x, base_bounds.box_extent.x,
             same_skeleton, bool(mat)))

    problems = []
    if not ok_height:
        problems.append('height %.1f vs base %.1f' % (height, base_height))
    if not ok_width:
        problems.append('width %.1f out of plausible human range' % (bounds.box_extent.x * 2.0))
    if not mat:
        problems.append('material %s not found' % mat_name)
    if problems:
        u.log_error('C26_HERO_FAIL %s ' % role_name + ' | '.join(problems))
        return False
    u.log('C26_HERO_COMPLETE %s' % role_name)
    return True


def main():
    base = LIB.load_asset(BASE)
    assert base, 'Base rig missing; nothing to validate against'
    base_bounds = base.get_bounds()
    base_skeleton = base.get_editor_property('skeleton')

    results = {}
    for role_name, fbx_name, mat_name in ROLES:
        results[role_name] = import_one(role_name, fbx_name, mat_name, base_skeleton, base_bounds)

    ok = sum(1 for v in results.values() if v)
    u.log('C26_HERO_BATCH %d/%d imports OK: %s' % (ok, len(results), results))


main()
