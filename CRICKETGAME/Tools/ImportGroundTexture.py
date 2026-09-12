import unreal as u
import os

project = u.Paths.project_dir()
path = os.path.join(project, 'ArtSource', 'Generated', 'World', 'T_Eclipse_GroundColour.tga')
assert os.path.exists(path), 'Missing TGA: ' + path

task = u.AssetImportTask()
task.filename = path
task.destination_path = '/Game/Cricket26/Environment/Outfield'
task.destination_name = 'T_Eclipse_GroundColour'
task.automated = True
task.replace_existing = True
task.save = True

u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

lib = u.EditorAssetLibrary
tex = lib.load_asset('/Game/Cricket26/Environment/Outfield/T_Eclipse_GroundColour')
assert tex, 'Failed to import T_Eclipse_GroundColour'
tex.set_editor_property('srgb', False)
tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
tex.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
tex.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
lib.save_loaded_asset(tex)
u.log('C26_GROUND_TEXTURE_IMPORTED_SUCCESS')
