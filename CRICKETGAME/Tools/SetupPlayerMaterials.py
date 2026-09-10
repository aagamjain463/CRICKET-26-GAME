import unreal as u

LIB = u.EditorAssetLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
ML = u.MaterialEditingLibrary

DEST_MESH_DIR = '/Game/Cricket26/Characters/Players'
DEST_MAT_DIR = '/Game/Cricket26/Materials/Players'

LIB.make_directory(DEST_MAT_DIR)

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

for mesh_name, mi_name, tex_path in player_configs:
    mi_asset_path = DEST_MAT_DIR + '/' + mi_name
    tex = LIB.load_asset(tex_path)
    if not tex:
        u.log_error(f"Missing texture {tex_path}")
        continue
        
    if LIB.does_asset_exist(mi_asset_path):
        mi = LIB.load_asset(mi_asset_path)
    else:
        factory = u.MaterialInstanceConstantFactoryNew()
        mi = AT.create_asset(mi_name, DEST_MAT_DIR, u.MaterialInstanceConstant, factory)
        
    mi.set_editor_property('parent', master_mat)
    ML.set_material_instance_texture_parameter_value(mi, 'BaseTexture', tex)
    ML.set_material_instance_vector_parameter_value(mi, 'Tint', u.LinearColor(1.0, 1.0, 1.0, 1.0))
    ML.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.8)
    ML.set_material_instance_scalar_parameter_value(mi, 'Glow', 0.0)
    ML.update_material_instance(mi)
    LIB.save_loaded_asset(mi)
    u.log(f"Configured Material Instance: {mi_asset_path}")
    
    # Assign to static mesh
    mesh_path = DEST_MESH_DIR + '/' + mesh_name
    sm = LIB.load_asset(mesh_path)
    if sm:
        sm.set_material(0, mi)
        LIB.save_loaded_asset(sm)
        u.log(f"Assigned {mi_name} to {mesh_name}")
    else:
        u.log_error(f"Missing static mesh {mesh_path}")

u.log("=== ALL 10 PLAYER MATERIAL INSTANCES CONFIGURED AND BOUND ===")
