
import unreal as u

for path in [
    '/Game/Cricket26/Characters/SK_Cricketer_KitBase',
    '/Game/Cricket26/Characters/SK_Cricketer_Match',
    '/Game/Cricket26/Characters/SK_Cricketer_KitCricket',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_Batter',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_Bowler',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_Keeper',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_Umpire',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_01',
    '/Game/Cricket26/Characters/Players/SM_C26_Player_NonStriker',
    '/Game/Cricket26/Equipment/SM_C26_Bat_Hero',
    '/Game/Cricket26/Equipment/SM_C26_Pad_L',
    '/Game/Cricket26/Equipment/SM_C26_Pad_R',
    '/Game/Cricket26/Equipment/SM_C26_Shoe_L',
    '/Game/Cricket26/Equipment/SM_C26_Shoe_R',
    '/Game/Cricket26/Equipment/SM_C26_Helmet_Hero',
]:
    asset = u.EditorAssetLibrary.load_asset(path)
    if not asset:
        u.log_warning(f"NOT FOUND: {path}")
        continue
    bounds = asset.get_bounds()
    origin = bounds.origin
    box_ext = bounds.box_extent
    sphere_rad = bounds.sphere_radius
    u.log(f"BOUNDS_CHECK: {path}: origin=({origin.x:.1f}, {origin.y:.1f}, {origin.z:.1f}), ext=({box_ext.x:.1f}, {box_ext.y:.1f}, {box_ext.z:.1f}), total_Z={box_ext.z*2:.1f}cm, radius={sphere_rad:.1f}")
