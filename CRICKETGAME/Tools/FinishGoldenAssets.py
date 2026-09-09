"""Preserve original material bindings on the derived kit mesh and build soft dust."""
import runpy
import unreal as u

lib = u.EditorAssetLibrary
# Reimport only the derived asset after exporting corrected smoothing. The original stays intact.
task = u.AssetImportTask()
task.filename = u.Paths.project_dir() + 'ArtSource/Exports/C26_KitBase_v001.fbx'
task.destination_path = '/Game/Cricket26/Characters'
task.destination_name = 'SK_Cricketer_KitBase'
task.automated = True
task.replace_existing = True
task.save = True
options = u.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal = True
options.import_animations = options.import_materials = options.import_textures = False
task.options = options
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
original = lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer')
derived = lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer_KitBase')
bindings = {str(s.material_slot_name): s.material_interface
            for s in original.get_editor_property('materials')}
slots = derived.get_editor_property('materials')
for slot in slots:
    name = str(slot.material_slot_name).split('.')[0]
    assert name in bindings, 'Missing original material: ' + name
    slot.set_editor_property('material_interface', bindings[name])
    u.log('C26_KIT_BINDING %s -> %s' % (name, bindings[name]))
derived.set_editor_property('materials', slots)
lib.save_loaded_asset(derived)
runpy.run_path(u.Paths.project_dir() + 'Tools/BuildEffectAssets.py')
u.log('C26_GOLDEN_ASSETS_COMPLETE')
