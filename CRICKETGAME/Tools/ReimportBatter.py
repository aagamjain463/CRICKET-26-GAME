import unreal as u
import os

AT = u.AssetToolsHelpers.get_asset_tools()
LIB = u.EditorAssetLibrary

fbx_file = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players/SM_C26_Player_Batter.fbx"
dest_path = "/Game/Cricket26/Characters/Players"

task = u.AssetImportTask()
task.set_editor_property('filename', fbx_file)
task.set_editor_property('destination_path', dest_path)
task.set_editor_property('destination_name', 'SM_C26_Player_Batter')
task.set_editor_property('replace_existing', True)
task.set_editor_property('automated', True)
task.set_editor_property('save', True)

options = u.FbxImportUI()
options.set_editor_property('import_mesh', True)
options.set_editor_property('import_as_skeletal', False)
options.set_editor_property('import_materials', False)
options.set_editor_property('import_textures', False)
options.static_mesh_import_data.set_editor_property('combine_meshes', True)
options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', True)

task.set_editor_property('options', options)
AT.import_asset_tasks([task])

mesh = LIB.load_asset('/Game/Cricket26/Characters/Players/SM_C26_Player_Batter')
if mesh:
    mi = LIB.load_asset('/Game/Cricket26/Materials/Players/MI_Player_Batter')
    if mi:
        mesh.set_material(0, mi)
        LIB.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    u.log_warning(f"REIMPORTED_BOUNDS: Origin={b.origin}, BoxExtent={b.box_extent}, Radius={b.sphere_radius}")
