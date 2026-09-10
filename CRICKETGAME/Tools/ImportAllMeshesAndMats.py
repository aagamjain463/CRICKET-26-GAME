import unreal as u
import os

AT = u.AssetToolsHelpers.get_asset_tools()
LIB = u.EditorAssetLibrary

players = [
    ("SM_C26_Player_Batter", "MI_Player_Batter"),
    ("SM_C26_Player_Bowler", "MI_Player_Bowler"),
    ("SM_C26_Player_Keeper", "MI_Player_Keeper"),
    ("SM_C26_Player_Umpire", "MI_Player_Umpire"),
    ("SM_C26_Player_Fielder_01", "MI_Player_Fielder_01"),
    ("SM_C26_Player_Fielder_02", "MI_Player_Fielder_02"),
    ("SM_C26_Player_Fielder_03", "MI_Player_Fielder_03"),
    ("SM_C26_Player_Fielder_04", "MI_Player_Fielder_04"),
    ("SM_C26_Player_Fielder_05", "MI_Player_Fielder_05"),
    ("SM_C26_Player_Fielder_06", "MI_Player_Fielder_06"),
]

fbx_dir = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players"
mesh_dest = "/Game/Cricket26/Characters/Players"

for mesh_name, mat_name in players:
    fbx_file = os.path.join(fbx_dir, f"{mesh_name}.fbx")
    if not os.path.exists(fbx_file):
        u.log_error(f"FBX missing: {fbx_file}")
        continue
        
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
    
    mesh_path = f"{mesh_dest}/{mesh_name}"
    mesh = LIB.load_asset(mesh_path)
    if mesh:
        mi = LIB.load_asset(f"/Game/Cricket26/Materials/Players/{mat_name}")
        if mi:
            mesh.set_material(0, mi)
            LIB.save_loaded_asset(mesh)
        b = mesh.get_bounds()
        u.log_warning(f"CHECK_BOUNDS [{mesh_name}]: Z_Ext={b.box_extent.z:.1f} cm (Total H={b.box_extent.z*2:.1f} cm), Origin_Z={b.origin.z:.1f} cm")

print("All hero meshes imported successfully.")
