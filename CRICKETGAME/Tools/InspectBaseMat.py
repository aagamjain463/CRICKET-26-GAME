import unreal as u
mat = u.EditorAssetLibrary.load_asset('/Game/Cricket26/Materials/Players/M_Athlete')
if mat:
    u.log_warning("Loaded M_Athlete successfully")
else:
    u.log_error("Failed to load M_Athlete")
