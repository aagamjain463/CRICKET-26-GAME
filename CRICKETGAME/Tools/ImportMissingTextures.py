import unreal as u
import os

AT = u.AssetToolsHelpers.get_asset_tools()
LIB = u.EditorAssetLibrary

missing_tex = [
    ("T_Athlete_02_D", "02_4260cecc", "MI_Player_Umpire"),
    ("T_Athlete_06_D", "06_d8114a2f", "MI_Player_Batter"),
    ("T_Athlete_07_D", "07_7d3019fc", "MI_Player_NonStriker"),
    ("T_Athlete_08_D", "08_37929429", "MI_Player_Bowler"),
]

raw_base = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Raw"
dest_path = "/Game/Cricket26/Textures"

tasks = []
for tex_name, folder, mat_name in missing_tex:
    tex_path = os.path.join(raw_base, folder, "texture_pbr_20250901.png")
    if not os.path.exists(tex_path):
        u.log_error(f"Missing texture file: {tex_path}")
        continue
    t = u.AssetImportTask()
    t.set_editor_property('filename', tex_path)
    t.set_editor_property('destination_path', dest_path)
    t.set_editor_property('destination_name', tex_name)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', True)
    tasks.append(t)

AT.import_asset_tasks(tasks)

for tex_name, folder, mat_name in missing_tex:
    mi_path = f"/Game/Cricket26/Materials/Players/{mat_name}"
    mi = LIB.load_asset(mi_path)
    tex = LIB.load_asset(f"{dest_path}/{tex_name}")
    if mi and tex:
        u.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseColor", tex)
        u.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "Diffuse", tex)
        LIB.save_loaded_asset(mi)
        u.log_warning(f"Successfully hooked {tex_name} -> {mi_path}")
    else:
        u.log_error(f"Could not hook {tex_name} -> {mat_name}: mi={mi!=None}, tex={tex!=None}")
