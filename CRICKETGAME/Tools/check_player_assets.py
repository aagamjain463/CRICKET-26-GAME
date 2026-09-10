
import unreal as u

assets = [
    'SM_C26_Player_Batter',
    'SM_C26_Player_Bowler',
    'SM_C26_Player_Keeper',
    'SM_C26_Player_Umpire',
    'SM_C26_Player_NonStriker',
    'SM_C26_Player_Fielder_01',
    'SM_C26_Player_Fielder_02',
    'SM_C26_Player_Fielder_03',
    'SM_C26_Player_Fielder_04',
    'SM_C26_Player_Fielder_05',
    'SM_C26_Player_Fielder_06',
]

for name in assets:
    p = f"/Game/Cricket26/Characters/Players/{name}.{name}"
    m = u.load_object(None, p)
    if m:
        mats = [str(mat.material_slot_name) for mat in m.static_materials]
        u.log(f"ASSET_CHECK: {name} valid, num_mats={len(mats)}, slots={mats}")
    else:
        u.log_error(f"ASSET_CHECK: {name} NOT FOUND")
