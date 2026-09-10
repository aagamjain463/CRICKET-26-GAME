import unreal as u
sk = u.EditorAssetLibrary.load_asset('/Game/Cricket26/Characters/SK_Cricketer_KitBase')
if sk:
    u.log(f"SK_Cricketer_KitBase loaded successfully!")
    ref_skel = sk.get_editor_property('skeleton')
    u.log(f"Skeleton: {ref_skel.get_name() if ref_skel else 'None'}")
else:
    u.log_error("Failed to load SK_Cricketer_KitBase")
