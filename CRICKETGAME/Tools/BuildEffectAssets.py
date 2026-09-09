"""Original unlit translucent particle material for the C26 effects system.

Run inside UnrealEditor-Cmd with -run=pythonscript -script=<absolute path>.

Vertex colour carries per-particle tint in RGB and per-particle fade in A, so the whole effects
system draws as one mesh section with one material and needs no Niagara or Cascade asset.
Blend mode and shading model are set before any pin is connected, because MP_OPACITY is not an
active material property on an opaque material and connecting to it would silently fail.
Every connection is asserted; a partly-built graph must never be saved as if it were complete.
"""
import unreal as u

ROOT = '/Game/Cricket26'
ML = u.MaterialEditingLibrary
lib = u.EditorAssetLibrary
assets = u.AssetToolsHelpers.get_asset_tools()


def node(mat, cls, **props):
    result = ML.create_material_expression(mat, cls)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result


def link(a, out, b, pin):
    # Output pin names differ per node type on this engine: a TextureSample calls its colour output
    # 'RGB' while a VertexColor calls the same thing '' (the unnamed first output). Only those two
    # spellings are interchangeable. A single-channel request such as 'A' must never quietly fall
    # back to the full colour output -- that wires the wrong data and still looks like a success.
    candidates = ('', 'RGB') if out in ('', 'RGB') else (out,)
    for candidate in candidates:
        if ML.connect_material_expressions(a, candidate, b, pin):
            return
    raise AssertionError((a.get_name(), out, b.get_name(), pin))


def output(a, pin, prop):
    assert ML.connect_material_property(a, pin, prop), (a.get_name(), pin, str(prop))


name = 'M_Particle'
path = ROOT + '/Materials/' + name
mat = lib.load_asset(path) if lib.does_asset_exist(path) else assets.create_asset(
    name, ROOT + '/Materials', u.Material, u.MaterialFactoryNew())
ML.delete_all_material_expressions(mat)

# Set the domain before wiring: opacity is only an active property once the material is translucent.
mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)

vertex = node(mat, u.MaterialExpressionVertexColor)
tint = node(mat, u.MaterialExpressionVectorParameter, parameter_name='Tint',
            default_value=u.LinearColor(1, 1, 1, 1))
colour = node(mat, u.MaterialExpressionMultiply)
link(vertex, '', colour, 'A')
link(tint, '', colour, 'B')
output(colour, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

fade = node(mat, u.MaterialExpressionScalarParameter, parameter_name='Opacity', default_value=1.0)
alpha = node(mat, u.MaterialExpressionMultiply)
link(vertex, 'A', alpha, 'A')
link(fade, '', alpha, 'B')
output(alpha, '', u.MaterialProperty.MP_OPACITY)

ML.layout_material_expressions(mat)
ML.recompile_material(mat)
lib.save_loaded_asset(mat)
u.log('C26_EFFECT_BUILT ' + name)
u.log('C26_EFFECTS_COMPLETE')
