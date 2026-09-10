import unreal as u
LIB = u.EditorAssetLibrary
m = LIB.load_asset('/Game/Cricket26/Materials/M_Surface')
if m:
    u.log('M_Surface loaded: %s' % m)
    # Check parameters
    for p in m.get_editor_property('parameters').scalar_parameters:
        u.log('  Scalar param: %s = %f' % (p.parameter_info.name, p.parameter_value))
    for p in m.get_editor_property('parameters').vector_parameters:
        u.log('  Vector param: %s = %s' % (p.parameter_info.name, p.parameter_value))
    for p in m.get_editor_property('parameters').texture_parameters:
        u.log('  Texture param: %s = %s' % (p.parameter_info.name, p.parameter_value))
