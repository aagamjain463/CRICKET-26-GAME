"""Build a textured skin material from the character textures already in the project."""
import unreal as u

lib = u.EditorAssetLibrary
path = '/Game/Cricket26/Materials/M_C26_PlayerSkin'
mat = lib.load_asset(path) if lib.does_asset_exist(path) else None
if not mat:
    mat = u.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_C26_PlayerSkin', '/Game/Cricket26/Materials', u.Material, u.MaterialFactoryNew())
edit = u.MaterialEditingLibrary
edit.delete_all_material_expressions(mat)
edit.set_material_usage(mat, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
for texture, sampler, prop in (
    ('Remy_Body_Diffuse', u.MaterialSamplerType.SAMPLERTYPE_COLOR, u.MaterialProperty.MP_BASE_COLOR),
    ('Remy_Body_Normal', u.MaterialSamplerType.SAMPLERTYPE_NORMAL, u.MaterialProperty.MP_NORMAL),
):
    node = edit.create_material_expression(mat, u.MaterialExpressionTextureSample)
    node.texture = lib.load_asset('/Game/Cricket26/Characters/' + texture)
    assert node.texture, texture
    node.sampler_type = sampler
    edit.connect_material_property(node, 'RGB', prop)
for value, prop in ((0.62, u.MaterialProperty.MP_ROUGHNESS), (0.32, u.MaterialProperty.MP_SPECULAR)):
    node = edit.create_material_expression(mat, u.MaterialExpressionConstant)
    node.r = value
    edit.connect_material_property(node, '', prop)
edit.recompile_material(mat)
lib.save_loaded_asset(mat)

mesh = lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer_Match')
u.log('C26_REFRESH mesh=%s skeleton=%s materials=%s' % (
    mesh.get_name(), mesh.skeleton.get_name(),
    [str(slot.material_slot_name) for slot in mesh.materials]))
for name in ('A_Run', 'A_Idle'):
    clip = lib.load_asset('/Game/Cricket26/Animations/' + name)
    u.log('C26_REFRESH clip=%s length=%.3f skeleton=%s' % (
        name, clip.sequence_length, clip.get_editor_property('skeleton').get_name()))
u.log('C26_REFRESH_ASSETS_PASS')
