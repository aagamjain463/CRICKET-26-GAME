"""Premium cloth and skin materials for the athletes.

Run: UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/BuildKitMaterials.py

Until this existed every square centimetre of a player's clothing returned one flat colour with
one flat roughness, so a shirt, a trouser leg and a painted wall were the same surface under the
floodlights. Cricket whites in particular have nothing else to read by: no pattern, no texture,
just the way light breaks over folds and the grazing sheen along a sleeve.

M_C26_Kit keeps the exact parameter names M_Surface uses -- Tint, Roughness, Glow -- so every
UMaterialInstanceDynamic the athlete already creates works unchanged, and adds:
  Weave      normal map (per-instance, so shirt and trousers can differ)
  Cloth      albedo whose luminance carries the fold and seam shading
  Detail     how much of that shading reaches the base colour
  Sheen      grazing-angle specular, which is what makes cloth look like cloth
"""
import unreal as u

lib = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
CHARS = '/Game/Cricket26/Characters/'


def texture(name):
    asset = lib.load_asset(CHARS + name)
    assert asset, 'missing texture ' + name
    return asset


def material(name):
    path = '/Game/Cricket26/Materials/' + name
    mat = lib.load_asset(path) if lib.does_asset_exist(path) else None
    if not mat:
        mat = u.AssetToolsHelpers.get_asset_tools().create_asset(
            name, '/Game/Cricket26/Materials', u.Material, u.MaterialFactoryNew())
    assert mat, 'failed to create ' + name
    edit.delete_all_material_expressions(mat)
    edit.set_material_usage(mat, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    return mat


def node(mat, kind, x=0, y=0):
    return edit.create_material_expression(mat, kind, x, y)


def scalar(mat, param, value, x=0, y=0):
    n = node(mat, u.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', param)
    n.set_editor_property('default_value', value)
    return n


def sample(mat, param, name, sampler, x=0, y=0):
    n = node(mat, u.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property('parameter_name', param)
    n.set_editor_property('texture', texture(name))
    n.set_editor_property('sampler_type', sampler)
    return n


def build(name, cloth, normal, rough, spec, sheen, detail):
    mat = material(name)
    tint = node(mat, u.MaterialExpressionVectorParameter, -900, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', u.LinearColor(1.0, 1.0, 1.0, 1.0))

    weave = sample(mat, 'Cloth', cloth, u.MaterialSamplerType.SAMPLERTYPE_COLOR, -900, 300)
    grey = node(mat, u.MaterialExpressionDesaturation, -700, 300)
    edit.connect_material_expressions(weave, 'RGB', grey, '')
    # The source albedo averages well below mid grey, so its luminance is rescaled to sit around
    # 1.0 before it modulates the team colour: this must add fold and seam shading, not darken
    # the kit. Detail is how much of it survives.
    gain = node(mat, u.MaterialExpressionMultiply, -560, 300)
    edit.connect_material_expressions(grey, '', gain, 'A')
    gain.set_editor_property('const_b', 2.35)
    flat = node(mat, u.MaterialExpressionConstant, -560, 430)
    flat.set_editor_property('r', 1.0)
    amount = scalar(mat, 'Detail', detail, -560, 520)
    folds = node(mat, u.MaterialExpressionLinearInterpolate, -380, 360)
    edit.connect_material_expressions(flat, '', folds, 'A')
    edit.connect_material_expressions(gain, '', folds, 'B')
    edit.connect_material_expressions(amount, '', folds, 'Alpha')

    base = node(mat, u.MaterialExpressionMultiply, -200, 100)
    edit.connect_material_expressions(tint, 'RGB', base, 'A')
    edit.connect_material_expressions(folds, '', base, 'B')
    edit.connect_material_property(base, '', u.MaterialProperty.MP_BASE_COLOR)

    bump = sample(mat, 'Weave', normal, u.MaterialSamplerType.SAMPLERTYPE_NORMAL, -900, 620)
    edit.connect_material_property(bump, 'RGB', u.MaterialProperty.MP_NORMAL)

    edit.connect_material_property(scalar(mat, 'Roughness', rough, -200, 700), '',
                                   u.MaterialProperty.MP_ROUGHNESS)

    # Cloth sheen. Fabric is a mat of fibres, so it lights up along every silhouette edge and down
    # the side of every fold facing away from the key light. A constant specular cannot do that,
    # and it is most of what separates a jersey from painted plastic at broadcast distance.
    rim = node(mat, u.MaterialExpressionFresnel, -560, 840)
    rim.set_editor_property('exponent', 4.2)
    rim.set_editor_property('base_reflect_fraction', 0.04)
    lift = node(mat, u.MaterialExpressionMultiply, -380, 840)
    edit.connect_material_expressions(rim, '', lift, 'A')
    edit.connect_material_expressions(scalar(mat, 'Sheen', sheen, -560, 960), '', lift, 'B')
    total = node(mat, u.MaterialExpressionAdd, -200, 840)
    edit.connect_material_expressions(lift, '', total, 'A')
    total.set_editor_property('const_b', spec)
    edit.connect_material_property(total, '', u.MaterialProperty.MP_SPECULAR)

    glow = node(mat, u.MaterialExpressionMultiply, -200, 1050)
    edit.connect_material_expressions(base, '', glow, 'A')
    edit.connect_material_expressions(scalar(mat, 'Glow', 0.0, -380, 1120), '', glow, 'B')
    edit.connect_material_property(glow, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

    edit.recompile_material(mat)
    lib.save_loaded_asset(mat)
    u.log('C26_KITMAT built=%s params=%s' % (
        name, [str(p) for p in u.MaterialEditingLibrary.get_scalar_parameter_names(mat)]))
    return mat


# Cricket clothing: high roughness, low base specular, strong sheen, folds well in evidence.
build('M_C26_Kit', 'Remy_Top_Diffuse', 'Remy_Top_Normal', 0.86, 0.26, 0.55, 0.62)
# Skin keeps its own albedo detail but is now tintable, so eleven players stop sharing one face
# value. Lower roughness, real specular, a little sheen for floodlit sweat.
build('M_C26_PlayerSkin', 'Remy_Body_Diffuse', 'Remy_Body_Normal', 0.58, 0.34, 0.22, 1.0)
u.log('C26_KITMAT_COMPLETE')
