"""Import the CRICKET 26 hero equipment set into /Game/Cricket26/Equipment.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=/absolute/path/Tools/ImportEquipment.py
"""
import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Equipment'
SRC = u.Paths.project_dir() + 'ArtSource/Exports/Equipment/'
SURFACE = '/Game/Cricket26/Materials/M_Surface.M_Surface'

# name -> (expected longest dimension in cm, tolerance cm)
EXPECTED = {
    'SM_C26_Bat_Hero': (87.3, 1.5),
    'SM_C26_Helmet_Hero': (29.7, 1.5),
    'SM_C26_HelmetGrille_Hero': (20.3, 1.5),
    'SM_C26_Cap_Hero': (28.5, 1.5),
    'SM_C26_Pad_L': (53.6, 1.5), 'SM_C26_Pad_R': (53.6, 1.5),
    'SM_C26_Glove_L': (19.0, 1.5), 'SM_C26_Glove_R': (19.0, 1.5),
    'SM_C26_Shoe_L': (27.5, 1.5), 'SM_C26_Shoe_R': (27.5, 1.5),
    'SM_C26_Ball_Hero': (7.2, 1.0),
    'SM_C26_Wicket_Stumps': (71.1, 2.5),
    'SM_C26_Wicket_Bails': (22.9, 2.5),
}


def options():
    o = u.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    o.import_as_skeletal = False
    o.import_animations = False
    o.import_materials = False
    o.import_textures = False
    sm = o.static_mesh_import_data
    sm.set_editor_property('combine_meshes', True)
    sm.set_editor_property('generate_lightmap_u_vs', False)
    sm.set_editor_property('auto_generate_collision', False)
    sm.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return o


def main():
    sms = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) if hasattr(u, 'StaticMeshEditorSubsystem') else None
    surface = LIB.load_asset(SURFACE)
    assert surface, 'M_Surface missing'
    tasks = []
    for name in sorted(EXPECTED):
        path = SRC + name + '.fbx'
        assert os.path.exists(path), 'Missing source FBX: ' + path
        t = u.AssetImportTask()
        t.filename = path
        t.destination_path = DEST
        t.destination_name = name
        t.automated = True
        t.save = True
        t.replace_existing = True
        t.options = options()
        tasks.append(t)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    failures = []
    for name, (want, tol) in sorted(EXPECTED.items()):
        asset = LIB.load_asset(DEST + '/' + name)
        if not asset:
            failures.append('%s: import produced no asset' % name)
            continue
        ext = asset.get_bounding_box().max - asset.get_bounding_box().min
        got = max(ext.x, ext.y, ext.z)
        ok = abs(got - want) <= tol
        mats = asset.get_editor_property('static_materials')
        for slot in mats:
            slot.set_editor_property('material_interface', surface)
        asset.set_editor_property('static_materials', mats)
        
        tris = 0
        try:
            opts = u.StaticMeshReductionOptions(
                auto_compute_lod_screen_size=True,
                reduction_settings=[
                    u.StaticMeshReductionSettings(percent_triangles=1.0),
                    u.StaticMeshReductionSettings(percent_triangles=0.45),
                    u.StaticMeshReductionSettings(percent_triangles=0.18)])
            if sms:
                sms.set_lods(asset, opts)
                tris = sms.get_number_triangles(asset, 0)
            else:
                u.EditorStaticMeshLibrary.set_lods(asset, opts)
        except Exception as exc:
            u.log_warning('C26_EQUIP LOD build skipped for %s: %s' % (name, exc))
        LIB.save_loaded_asset(asset)
        u.log('C26_EQUIP %-26s size=%6.1f cm want=%6.1f %s slots=%d lod0tris=%d lods=%d' % (
            name, got, want, 'OK' if ok else 'SCALE-FAIL', len(mats), tris,
            asset.get_num_lods()))
        if not ok:
            failures.append('%s: %.1f cm, expected %.1f' % (name, got, want))

    if failures:
        u.log_error('C26_EQUIP_FAIL ' + ' | '.join(failures))
    else:
        u.log('C26_EQUIP_COMPLETE %d assets imported and verified' % len(EXPECTED))


main()
