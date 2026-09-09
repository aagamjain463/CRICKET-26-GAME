"""Creates the two blended materials the broadcast presentation needs.

Everything the venue and the athletes draw today goes through M_Surface, which is opaque. That is
fine for turf, seats and kit, but it cannot express the three things that most separate a night
cricket broadcast from a flat prototype render: a contact shadow under a player, a floodlight
shaft hanging in the air, and a streak behind a ball travelling at 140 km/h.

Both materials here take their falloff from the mesh's own vertex colour alpha rather than from a
texture or a procedural noise chain. That is deliberate: BuildContent.py's `textured=True` chain
(WorldPosition -> Multiply -> Noise) silently fails to resolve on this renderer and drops the whole
material back to engine default grey -- see AI_HANDOFF BUG B. VertexColor is a single primitive node
with no such problem, and UProceduralMeshComponent already carries a per-vertex FLinearColor
channel, so the C++ side can author any falloff it likes as geometry.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/BuildPresentationAssets.py
"""
import unreal as u

ROOT = '/Game/Cricket26'
assets = u.AssetToolsHelpers.get_asset_tools()
lib = u.EditorAssetLibrary
ML = u.MaterialEditingLibrary


def blended(name, blend, colour, glow):
    """Unlit material whose opacity is vertex-colour alpha times an Opacity parameter."""
    path = ROOT + '/Materials'
    full = path + '/' + name
    mat = lib.load_asset(full) if lib.does_asset_exist(full) else None
    if not mat:
        mat = assets.create_asset(name, path, u.Material, u.MaterialFactoryNew())
    ML.delete_all_material_expressions(mat)

    tint = ML.create_material_expression(mat, u.MaterialExpressionVectorParameter, -700, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', u.LinearColor(*colour, 1))

    gain = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -700, 190)
    gain.set_editor_property('parameter_name', 'Glow')
    gain.set_editor_property('default_value', glow)

    vertex = ML.create_material_expression(mat, u.MaterialExpressionVertexColor, -700, 380)

    # Emissive = Tint * Glow. Unlit means this is the whole visible colour.
    emissive = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -420, 60)
    ML.connect_material_expressions(tint, '', emissive, 'A')
    ML.connect_material_expressions(gain, '', emissive, 'B')

    # Additive throws away the opacity channel on some paths, so fold the falloff into the colour
    # as well. Translucent uses it once through Opacity; additive gets it through both and reads
    # as a soft-edged beam either way.
    shaped = emissive
    if blend == u.BlendMode.BLEND_ADDITIVE:
        shaped = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -220, 60)
        ML.connect_material_expressions(emissive, '', shaped, 'A')
        ML.connect_material_expressions(vertex, 'A', shaped, 'B')
    ML.connect_material_property(shaped, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

    strength = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -420, 470)
    strength.set_editor_property('parameter_name', 'Opacity')
    strength.set_editor_property('default_value', 1.0)
    opacity = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -220, 420)
    ML.connect_material_expressions(vertex, 'A', opacity, 'A')
    ML.connect_material_expressions(strength, '', opacity, 'B')
    ML.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY)

    mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', blend)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    # A contact shadow that wrote depth would z-fight the turf it sits on and would occlude the
    # shafts behind it. Neither of these ever needs to be an occluder.
    mat.set_editor_property('allow_translucent_custom_depth_writes', False)
    ML.recompile_material(mat)
    lib.save_loaded_asset(mat)
    u.log('C26_ASSET built %s blend=%s' % (name, blend))
    return mat


# Contact shadow: near-black, alpha-blended, darkens the turf under a player.
blended('M_Shade', u.BlendMode.BLEND_TRANSLUCENT, (0.006, 0.010, 0.008), 1.0)
# Floodlight shafts, ball streaks and impact flashes: additive, so they only ever add light.
blended('M_Beam', u.BlendMode.BLEND_ADDITIVE, (0.62, 0.72, 0.95), 1.0)

u.log('C26_ASSET presentation materials complete')
