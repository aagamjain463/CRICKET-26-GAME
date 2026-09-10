import unreal as u

# Player visual pairing repair (rescue/player-role-visual-fix).
# - Creates MI_Player_NonStriker bound to T_Keeper_Hero_D: the NonStriker mesh is the
#   raw07 bake, so only the raw07 texture fits its UVs. Pairing it with the batter
#   texture (raw06 UVs) rendered the non-striker as camouflage noise.
# - Rebinds MI_Player_Fielder_06 to T_Athlete_01_D: the Fielder_06 mesh file is the
#   raw01 bake, so only the raw01 texture fits. The raw10 texture made it camouflage.
# - Re-asserts every runtime pairing and logs mesh heights for the scale check.

LIB = u.EditorAssetLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
ML = u.MaterialEditingLibrary

DEST_MAT = '/Game/Cricket26/Materials/Players'
DEST_MESH = '/Game/Cricket26/Characters/Players'

master = LIB.load_asset('/Game/Cricket26/Materials/M_Athlete_PBR')
assert master, "M_Athlete_PBR missing"


def ensure_mi(mi_name, tex_path):
    tex = LIB.load_asset(tex_path)
    if not tex:
        u.log_error('C26_PAIR_FIX missing texture ' + tex_path)
        return None
    mi_path = DEST_MAT + '/' + mi_name
    mi = LIB.load_asset(mi_path)
    if not mi:
        fac = u.MaterialInstanceConstantFactoryNew()
        mi = AT.create_asset(mi_name, DEST_MAT, u.MaterialInstanceConstant, fac)
        u.log('C26_PAIR_FIX created ' + mi_path)
    mi.set_editor_property('parent', master)
    ML.set_material_instance_texture_parameter_value(mi, 'BaseTexture', tex)
    ML.set_material_instance_vector_parameter_value(mi, 'Tint', u.LinearColor(1.0, 1.0, 1.0, 1.0))
    ML.set_material_instance_scalar_parameter_value(mi, 'Roughness', 0.8)
    ML.set_material_instance_scalar_parameter_value(mi, 'Glow', 0.0)
    ML.update_material_instance(mi)
    LIB.save_loaded_asset(mi)
    return mi


ensure_mi('MI_Player_NonStriker', '/Game/Cricket26/Textures/T_Keeper_Hero_D')
ensure_mi('MI_Player_Fielder_06', '/Game/Cricket26/Textures/T_Athlete_01_D')

pairs = [
    ('SM_C26_Player_Batter', 'MI_Player_Batter', 'T_Batter_Hero_D'),
    ('SM_C26_Player_NonStriker', 'MI_Player_NonStriker', 'T_Keeper_Hero_D'),
    ('SM_C26_Player_Fielder_05', 'MI_Player_Fielder_05', 'T_Athlete_09_D'),
    ('SM_C26_Player_Fielder_01', 'MI_Player_Fielder_01', 'T_Athlete_01_D'),
    ('SM_C26_Player_Umpire', 'MI_Player_Umpire', 'T_Umpire_Hero_D'),
    ('SM_C26_Player_Fielder_06', 'MI_Player_Fielder_06', 'T_Athlete_01_D'),
]

failed = 0
for mesh_name, mi_name, tex_short in pairs:
    sm = LIB.load_asset(DEST_MESH + '/' + mesh_name)
    mi = LIB.load_asset(DEST_MAT + '/' + mi_name)
    if not sm:
        u.log_error('C26_PAIR missing mesh ' + mesh_name)
        failed += 1
        continue
    if not mi:
        u.log_error('C26_PAIR missing mi ' + mi_name)
        failed += 1
        continue
    sm.set_material(0, mi)
    LIB.save_loaded_asset(sm)
    b = sm.get_bounds()
    u.log('C26_PAIR %s + %s height=%.1fcm' % (mesh_name, mi_name, b.box_extent.z * 2.0))

u.log('C26_PAIR_FIX_DONE failed=%d' % failed)
