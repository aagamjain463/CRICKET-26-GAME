
import unreal as u

for i in range(1, 7):
    p = f'/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_0{i}.SM_C26_Player_Fielder_0{i}'
    asset = u.load_object(None, p)
    if asset:
        b = asset.get_bounds()
        u.log(f"FIELDER_{i}: origin=({b.origin.x:.1f},{b.origin.y:.1f},{b.origin.z:.1f}), ext=({b.box_extent.x:.1f},{b.box_extent.y:.1f},{b.box_extent.z:.1f}), Z={b.box_extent.z*2:.1f}")
    else:
        u.log_warning(f"NOT FOUND: {p}")
