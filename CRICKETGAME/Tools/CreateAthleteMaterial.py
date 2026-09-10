import unreal as u

LIB = u.EditorAssetLibrary
ML = u.MaterialEditingLibrary
DEST = '/Game/Cricket26/Materials'
MAT_NAME = 'M_Athlete_PBR'
PATH = DEST + '/' + MAT_NAME

# Remove existing if any
if LIB.does_asset_exist(PATH):
    LIB.delete_asset(PATH)

factory = u.MaterialFactoryNew()
mat = u.AssetToolsHelpers.get_asset_tools().create_asset(MAT_NAME, DEST, u.Material, factory)
assert mat, 'Failed to create material'

# Base Texture Parameter
tex_param = ML.create_material_expression(mat, u.MaterialExpressionTextureSampleParameter2D, -500, -100)
tex_param.set_editor_property('parameter_name', 'BaseTexture')
default_tex = LIB.load_asset('/Game/Cricket26/Textures/T_Batter_Hero_D')
if default_tex:
    tex_param.set_editor_property('texture', default_tex)

# Tint Vector Parameter
tint_param = ML.create_material_expression(mat, u.MaterialExpressionVectorParameter, -500, 150)
tint_param.set_editor_property('parameter_name', 'Tint')
tint_param.set_editor_property('default_value', u.LinearColor(1.0, 1.0, 1.0, 1.0))

# Multiply Base Texture and Tint
mul = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -200, 0)
ML.connect_material_expressions(tex_param, 'RGB', mul, 'A')
ML.connect_material_expressions(tint_param, 'RGB', mul, 'B')
ML.connect_material_property(mul, '', u.MaterialProperty.MP_BASE_COLOR)

# Roughness Parameter
rough_param = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -300, 300)
rough_param.set_editor_property('parameter_name', 'Roughness')
rough_param.set_editor_property('default_value', 0.75)
ML.connect_material_property(rough_param, '', u.MaterialProperty.MP_ROUGHNESS)

# Glow / Emissive Parameter
glow_param = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -500, 450)
glow_param.set_editor_property('parameter_name', 'Glow')
glow_param.set_editor_property('default_value', 0.0)

glow_mul = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -200, 400)
ML.connect_material_expressions(mul, '', glow_mul, 'A')
ML.connect_material_expressions(glow_param, '', glow_mul, 'B')
ML.connect_material_property(glow_mul, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_skeletal_mesh', True)
mat.set_editor_property('used_with_instanced_static_meshes', True)

ML.recompile_material(mat)
LIB.save_loaded_asset(mat)
u.log('C26_MAT_PBR created and compiled successfully: %s' % PATH)
