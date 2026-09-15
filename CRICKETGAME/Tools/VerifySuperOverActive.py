import unreal

profile = unreal.load_object(None, '/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer')
unreal.log(f'DA_C26_DefaultPlayer loaded: {profile is not None}')
if profile:
    unreal.log(f'ApprovedForMatch: {profile.get_editor_property("ApprovedForMatch")}')
    unreal.log(f'Body: {profile.get_editor_property("Body").get_name()}')
    unreal.log(f'Skeleton: {profile.get_editor_property("Skeleton").get_name()}')
    unreal.log(f'Equipment items: {len(profile.get_editor_property("Equipment"))}')
    unreal.log(f'Clips: {len(profile.get_editor_property("Clips"))}')
    for role_name in ['BATTER', 'NON_STRIKER', 'BOWLER', 'FIELDER', 'KEEPER', 'UMPIRE']:
        role_enum = getattr(unreal.C26VisualRole, role_name)
        errs = profile.inspect_role(role_enum)
        unreal.log(f'InspectRole({role_name}): {"PASS" if len(errs) == 0 else errs}')