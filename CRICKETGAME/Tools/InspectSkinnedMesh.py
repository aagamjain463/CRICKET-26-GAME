import unreal as u
LIB = u.EditorAssetLibrary
sk = LIB.load_asset('/Game/Cricket26/Characters/SK_Cricketer_Match')
if sk:
    u.log('SK_Cricketer_Match materials: %d' % len(sk.get_editor_property('materials')))
    for i, m in enumerate(sk.get_editor_property('materials')):
        u.log('  Material [%d]: slot=%s, name=%s' % (i, m.material_slot_name, m.material_interface.get_name() if m.material_interface else 'None'))
else:
    u.log_error('Failed to load SK_Cricketer_Match')
