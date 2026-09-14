"""Point the hero face at the live albedo/normal path (parameter-only).

The inherited animated-wrinkle path sums unbaked white placeholder maps, which
blew the face out to chalk. The wrinkle deltas are flat placeholders anyway,
so disabling the animated path loses nothing and drops texture samples.
"""
import unreal as u

ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
assert face
face.modify()
for switch in ('Use Animated Maps', 'Use Delta Maps'):
    ML.set_material_instance_static_switch_parameter_value(face, switch, False)
LIB.save_loaded_asset(face, only_if_is_dirty=False)
assert str(ML.get_material_instance_static_switch_parameter_value(face, 'Use Animated Maps')) == 'False'
u.log('C26_HERO_FACE_PATH_FIXED animated=off basecolor=live')
