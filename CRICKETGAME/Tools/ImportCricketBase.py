"""Unreal commandlet: import a new, recoverable kit base; retain original assets."""
import unreal as u

lib = u.EditorAssetLibrary
dest = '/Game/Cricket26/Characters/SK_Cricketer_KitBase'
assert not lib.does_asset_exist(dest), 'Inspect existing asset; do not overwrite'
options = u.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal = True
options.import_animations = False
options.import_materials = False
options.import_textures = False
task = u.AssetImportTask()
task.filename = u.Paths.project_dir() + 'ArtSource/Exports/C26_KitBase_v001.fbx'
task.destination_path = '/Game/Cricket26/Characters'
task.destination_name = 'SK_Cricketer_KitBase'
task.automated = True
task.save = True
task.replace_existing = False
task.options = options
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = lib.load_asset(dest)
assert mesh, 'Import failed'
slots = mesh.get_editor_property('materials')
original = lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer')
bindings = {str(s.material_slot_name): s.material_interface
            for s in original.get_editor_property('materials')}
for slot in slots:
    # Blender duplicate material IDs may append .001; reuse the existing legal textures.
    name = str(slot.material_slot_name).split('.')[0]
    assert name in bindings, 'Missing original material: ' + name
    slot.set_editor_property('material_interface', bindings[name])
mesh.set_editor_property('materials', slots)
lib.save_loaded_asset(mesh)
u.log('C26_KIT_IMPORTED bounds=%s slots=%s' % (mesh.get_bounds(), [str(s.material_slot_name) for s in slots]))
