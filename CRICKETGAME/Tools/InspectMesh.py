import unreal
mesh = unreal.EditorAssetLibrary.load_asset('/Game/Cricket26/Characters/Players/SM_C26_Player_Batter')
if mesh:
    bounds = mesh.get_bounds()
    unreal.log_warning(f"C26_BOUNDS: Origin={bounds.origin}, BoxExtent={bounds.box_extent}, SphereRadius={bounds.sphere_radius}")
    for i, m in enumerate(mesh.static_materials):
        unreal.log_warning(f"C26_MAT slot {i}: {m.material_slot_name} -> {m.material_interface.get_name() if m.material_interface else 'None'}")
else:
    unreal.log_error("C26_BOUNDS: MESH NOT FOUND")
