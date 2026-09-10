import unreal as u
import os

LIB = u.EditorAssetLibrary
DEST = '/Game/Cricket26/Textures'

TEXTURE_MAP = {
    'T_Athlete_01_D': '01_04aa892e',
    'T_Umpire_Hero_D': '02_4260cecc',
    'T_Athlete_03_D': '03_f4e20b7b',
    'T_Athlete_04_D': '04_2beed08e',
    'T_Athlete_05_D': '05_0434293e',
    'T_Batter_Hero_D': '06_d8114a2f',
    'T_Keeper_Hero_D': '07_7d3019fc',
    'T_Bowler_Hero_D': '08_37929429',
    'T_Athlete_09_D': '09_19b08966',
    'T_Athlete_10_D': '10_7996719e',
}

tasks = []
for tex_name, folder in sorted(TEXTURE_MAP.items()):
    src = os.path.join(u.Paths.project_dir(), 'ArtSource', 'Raw', folder, 'texture_pbr_20250901.png')
    if not os.path.exists(src):
        u.log_warning('Source not found: ' + src)
        continue
    t = u.AssetImportTask()
    t.filename = src
    t.destination_path = DEST
    t.destination_name = tex_name
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)

u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
u.log('C26_ALL_TEX_COMPLETE imported %d textures' % len(tasks))
