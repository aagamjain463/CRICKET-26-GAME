"""Print static-material slot names for batter equipment meshes."""
import unreal as u

for asset in ['SM_C26_Helmet_Hero', 'SM_C26_HelmetGrille_Hero', 'SM_C26_Bat_Hero',
              'SM_C26_Pad_L', 'SM_C26_Glove_L']:
    mesh = u.load_asset('/Game/Cricket26/Equipment/' + asset)
    if not mesh:
        u.log_error('C26_SLOT_MISSING_MESH ' + asset)
        continue
    names = [str(s.material_slot_name) for s in mesh.get_editor_property('static_materials')]
    u.log('C26_SLOTS %s -> %s' % (asset, names))
