import unreal as u
import os, glob

LIB = u.EditorAssetLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
ML = u.MaterialEditingLibrary

SRC_DIR = '/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players'
DEST_MESH_DIR = '/Game/Cricket26/Characters/Players'
DEST_MAT_DIR = '/Game/Cricket26/Materials/Players'

LIB.make_directory(DEST_MESH_DIR)
LIB.make_directory(DEST_MAT_DIR)

# Master material
master_mat = LIB.load_asset('/Game/Cricket26/Materials/M_Athlete_PBR')
assert master_mat, "Failed to load /Game/Cricket26/Materials/M_Athlete_PBR"

player_configs = [
    ('SM_C26_Player_Batter',     'MI_Player_Batter',     '/Game/Cricket26/Textures/T_Batter_Hero_D'),
    ('SM_C26_Player_Bowler',     'MI_Player_Bowler',     '/Game/Cricket26/Textures/T_Bowler_Hero_D'),
    ('SM_C26_Player_Keeper',     'MI_Player_Keeper',     '/Game/Cricket26/Textures/T_Keeper_Hero_D'),
    ('SM_C26_Player_Umpire',     'MI_Player_Umpire',     '/Game/Cricket26/Textures/T_Umpire_Hero_D'),
    ('SM_C26_Player_Fielder_01', 'MI_Player_Fielder_01', '/Game/Cricket26/Textures/T_Athlete_01_D'),
    ('SM_C26_Player_Fielder_02', 'MI_Player_Fielder_02', '/Game/Cricket26/Textures/T_Athlete_03_D'),
    ('SM_C26_Player_Fielder_03', 'MI_Player_Fielder_03', '/Game/Cricket26/Textures/T_Athlete_04_D'),
    ('SM_C26_Player_Fielder_04', 'MI_Player_Fielder_04', '/Game/Cricket26/Textures/T_Athlete_05_D'),
    ('SM_C26_Player_Fielder_05', 'MI_Player_Fielder_05', '/Game/Cricket26/Textures/T_Athlete_09_D'),
    ('SM_C26_Player_Fielder_06', 'MI_Player_Fielder_06', '/Game/Cricket26/Textures/T_Athlete_10_D'),
]

# 1. Create / Update Material Instances
created_instances = {}
for mesh_name, mi_name, tex_path in player_configs:
    mi_asset_path = DEST_MAT_DIR + '/' + mi_name
    tex = LIB.load_asset(tex_path)
    if not tex:
        u.log_error(f"Missing texture {tex_path}")
        continue
        
    mi = LIB.load_asset(mi_asset_path)
    if not mi:
        factory = u.MaterialInstanceConstantFactoryNew()
        mi = AT.create_asset(mi_name, DEST_MAT_DIR, u.MaterialInstanceConstant, factory)
        
    mi.set_editor_property('parent', master_mat)
    ML.set_material_instance_texture_parameter_value(mi, 'BaseTexture', tex)
    ML.set_material_instance_vector_parameter_value(mi, 'Tint', u.LinearColor(1.0, 1.0, 1.0, 1.0))
    ML.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.8)
    ML.set_material_instance_scalar_parameter_value(mi, 'Glow', 0.0)
    ML.update_material_instance(mi)
    LIB.save_loaded_asset(mi)
    created_instances[mesh_name] = mi
    u.log(f"Created/Updated Material Instance: {mi_asset_path} -> {tex.get_name()}")

# 2. Import Static Meshes
for mesh_name, mi_name, tex_path in player_configs:
    fbx_file = os.path.join(SRC_DIR, mesh_name + '.fbx')
    if not os.path.exists(fbx_file):
        u.log_warning(f"File not found: {fbx_file}, skipping")
        continue
        
    task = u.AssetImportTask()
    task.set_editor_property('filename', fbx_file)
    task.set_editor_property('destination_path', DEST_MESH_DIR)
    task.set_editor_property('destination_name', mesh_name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    
    opts = u.FbxImportUI()
    opts.set_editor_property('import_mesh', True)
    opts.set_editor_property('import_materials', False)
    opts.set_editor_property('import_textures', False)
    opts.set_editor_property('import_as_skeletal', False)
    
    sm_opts = opts.static_mesh_import_data
    sm_opts.set_editor_property('combine_meshes', True)
    sm_opts.set_editor_property('generate_lightmap_u_vs', True)
    sm_opts.set_editor_property('auto_generate_collision', True)
    
    task.set_editor_property('options', opts)
    AT.import_asset_tasks([task])
    
    # Assign Material Instance to Static Mesh
    mesh_asset_path = DEST_MESH_DIR + '/' + mesh_name
    sm = LIB.load_asset(mesh_asset_path)
    if sm and mesh_name in created_instances:
        sm.set_material(0, created_instances[mesh_name])
        LIB.save_loaded_asset(sm)
        u.log(f"Successfully imported and assigned material to {mesh_asset_path}")
    else:
        u.log_error(f"Failed to load or configure static mesh {mesh_asset_path}")

u.log("=== ALL HERO PLAYERS IMPORTED AND CONFIGURED ===")
