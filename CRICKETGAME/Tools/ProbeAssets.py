"""Read-only probe: material slot order and real sizes of the athlete assets."""
import unreal as u
LIB = u.EditorAssetLibrary
for p in ('/Game/Cricket26/Characters/SK_Cricketer_KitBase',):
    a = LIB.load_asset(p)
    bb = a.get_bounds()
    u.log('C26_PROBE SK %s box_extent=%s slots=%s' % (
        p.split('/')[-1], [round(v, 1) for v in (bb.box_extent * 2.0).to_tuple()],
        [str(s.material_slot_name) for s in a.get_editor_property('materials')]))
for n in ('SM_C26_Bat_Hero', 'SM_C26_Helmet_Hero', 'SM_C26_HelmetGrille_Hero', 'SM_C26_Cap_Hero',
          'SM_C26_Pad_L', 'SM_C26_Pad_R', 'SM_C26_Glove_L', 'SM_C26_Glove_R',
          'SM_C26_Shoe_L', 'SM_C26_Shoe_R'):
    a = LIB.load_asset('/Game/Cricket26/Equipment/' + n)
    bb = a.get_bounding_box()
    u.log('C26_PROBE SM %-26s min=%s max=%s slots=%s' % (
        n, [round(v, 1) for v in bb.min.to_tuple()], [round(v, 1) for v in bb.max.to_tuple()],
        [str(s.material_slot_name) for s in a.get_editor_property('static_materials')]))
