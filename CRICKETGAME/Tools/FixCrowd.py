"""Targeted repair: rebuild M_Crowd as a plain tinted material (no WPO chain).

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
The Time/Sine world-position animation previously added by BuildContent.py never
connected its Sine input through the Python API, so M_Crowd failed to compile on
Metal and the whole crowd fell back to the default grey material. Celebration is
now driven through the standard Glow param by AC26Stadium::UpdateAtmosphere.
"""
import unreal as u

ROOT = '/Game/Cricket26'
lib = u.EditorAssetLibrary
ML = u.MaterialEditingLibrary

mat = lib.load_asset(ROOT + '/Materials/M_Crowd')
if not mat:
    u.log_warning('C26 missing material ' + ROOT + '/Materials/M_Crowd')
else:
    ML.delete_all_material_expressions(mat)
    tint = ML.create_material_expression(mat, u.MaterialExpressionVectorParameter, -550, 0)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', u.LinearColor(.08, .2, .24, 1))
    ML.connect_material_property(tint, '', u.MaterialProperty.MP_BASE_COLOR)
    rough = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -300, 400)
    rough.set_editor_property('parameter_name', 'Roughness')
    rough.set_editor_property('default_value', .95)
    ML.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    glow = ML.create_material_expression(mat, u.MaterialExpressionScalarParameter, -400, 600)
    glow.set_editor_property('parameter_name', 'Glow')
    glow.set_editor_property('default_value', .15)
    multiply = ML.create_material_expression(mat, u.MaterialExpressionMultiply, -100, 600)
    ML.connect_material_expressions(tint, '', multiply, 'A')
    ML.connect_material_expressions(glow, '', multiply, 'B')
    ML.connect_material_property(multiply, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    ML.recompile_material(mat)
    lib.save_loaded_asset(mat)
    u.log('C26 CROWD FIX COMPLETE')
