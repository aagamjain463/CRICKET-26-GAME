import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Textures'
SRC = u.Paths.project_dir() + 'ArtSource/Raw/06_d8114a2f/texture_pbr_20250901.png'

assert os.path.exists(SRC), 'Missing source PNG: ' + SRC

task = u.AssetImportTask()
task.filename = SRC
task.destination_path = DEST
task.destination_name = 'T_Batter_Hero_D'
task.automated = True
task.save = True
task.replace_existing = True

u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

tex = LIB.load_asset(DEST + '/T_Batter_Hero_D')
if tex:
    u.log('C26_TEX imported successfully: %s (%dx%d)' % (tex.get_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
else:
    u.log_error('C26_TEX import failed')
