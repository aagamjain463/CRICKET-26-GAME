import unreal as u
import os

AT = u.AssetToolsHelpers.get_asset_tools()
LIB = u.EditorAssetLibrary

fbx_file = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players/SM_C26_Player_NonStriker.fbx"
mesh_dest = "/Game/Cricket26/Characters/Players"
mesh_name = "SM_C26_Player_NonStriker"

task = u.AssetImportTask()
task.set_editor_property('filename', fbx_file)
task.set_editor_property('destination_path', mesh_dest)
task.set_editor_property('destination_name', mesh_name)
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

mesh = LIB.load_asset(f"{mesh_dest}/{mesh_name}")
if mesh:
    mi = LIB.load_asset("/Game/Cricket26/Materials/Players/MI_Player_Batter")
    if mi:
        mesh.set_material(0, mi)
        LIB.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    u.log_warning(f"CHECK_BOUNDS [SM_C26_Player_NonStriker]: Z_Ext={b.box_extent.z:.1f} cm (Total H={b.box_extent.z*2:.1f} cm)")
