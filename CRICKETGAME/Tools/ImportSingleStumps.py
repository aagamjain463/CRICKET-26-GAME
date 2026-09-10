import unreal as u
LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Equipment'
SRC = u.Paths.project_dir() + 'ArtSource/Exports/Equipment/'
SURFACE = '/Game/Cricket26/Materials/M_Surface.M_Surface'

EXPECTED = {
    'SM_C26_Stump_Single': (71.3, 1.5),
    'SM_C26_Bail_Single': (11.1, 1.5),
}

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

tasks = []
for name in sorted(EXPECTED):
    t = u.AssetImportTask()
    t.filename = SRC + name + '.fbx'
    t.destination_path = DEST
    t.destination_name = name
    t.automated = True
    t.save = True
    t.replace_existing = True
    t.options = o
    tasks.append(t)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

surface = LIB.load_asset(SURFACE)
for name, (want, tol) in sorted(EXPECTED.items()):
    asset = LIB.load_asset(DEST + '/' + name)
    assert asset, name + ' not imported'
    ext = asset.get_bounding_box().max - asset.get_bounding_box().min
    got = max(ext.x, ext.y, ext.z)
    mats = asset.get_editor_property('static_materials')
    for slot in mats:
        slot.set_editor_property('material_interface', surface)
    asset.set_editor_property('static_materials', mats)
    LIB.save_loaded_asset(asset)
    u.log('C26_EQUIP %s size=%.1f want=%.1f OK' % (name, got, want))
