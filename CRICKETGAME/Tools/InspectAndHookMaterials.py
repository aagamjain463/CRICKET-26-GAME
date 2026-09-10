import unreal as u

LIB = u.EditorAssetLibrary

players = [
    ("Batter", "T_Athlete_06_D"),
    ("Bowler", "T_Athlete_08_D"),
    ("Keeper", "T_Athlete_10_D"),
    ("Umpire", "T_Athlete_02_D"),
    ("Fielder_01", "T_Athlete_01_D"),
    ("Fielder_02", "T_Athlete_03_D"),
    ("Fielder_03", "T_Athlete_04_D"),
    ("Fielder_04", "T_Athlete_05_D"),
    ("Fielder_05", "T_Athlete_09_D"),
    ("Fielder_06", "T_Athlete_01_D"),
]

for role, tex_name in players:
    mi_path = f"/Game/Cricket26/Materials/Players/MI_Player_{role}"
    mi = LIB.load_asset(mi_path)
    if not mi:
        u.log_warning(f"Material instance not found: {mi_path}")
        continue
    tex = LIB.load_asset(f"/Game/Cricket26/Textures/{tex_name}")
    if tex:
        u.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseColor", tex)
        u.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "Diffuse", tex)
        LIB.save_loaded_asset(mi)
        u.log_warning(f"Set texture {tex_name} on {mi_path}")
    else:
        u.log_warning(f"Texture not found: {tex_name}")
