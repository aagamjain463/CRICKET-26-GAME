"""Repair generated material usage flags and report athlete reference scale.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.

Materials created by BuildContent.py were applied at runtime to hierarchical instanced static
meshes and to the skinned cricketer. Without the matching usage flags the renderer silently
substitutes the default grey material, which is what flattened the whole venue and every kit.
"""
import unreal as u

ROOT = '/Game/Cricket26'
lib = u.EditorAssetLibrary

INSTANCED = ['M_Surface', 'M_Grass', 'M_Pitch', 'M_Light', 'M_Navy', 'M_Teal',
             'M_Coral', 'M_White', 'M_Willow', 'M_Crowd']
SKELETAL = ['M_Surface']

for name in INSTANCED:
    path = ROOT + '/Materials/' + name
    mat = lib.load_asset(path)
    if not mat:
        u.log_warning('C26 missing material ' + path)
        continue
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    if name in SKELETAL:
        mat.set_editor_property('used_with_skeletal_mesh', True)
    u.MaterialEditingLibrary.recompile_material(mat)
    lib.save_loaded_asset(mat)
    u.log('C26 usage flags set: %s instanced=%s skeletal=%s' % (
        name,
        mat.get_editor_property('used_with_instanced_static_meshes'),
        mat.get_editor_property('used_with_skeletal_mesh')))

mesh = lib.load_asset(ROOT + '/Characters/SK_Cricketer')
if mesh:
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    u.log('C26 SK_Cricketer box extent = %.2f, %.2f, %.2f  (height = %.2f)' % (
        extent.x, extent.y, extent.z, extent.z * 2))
    skeleton = mesh.get_editor_property('skeleton')
    u.log('C26 SK_Cricketer skeleton = %s' % skeleton.get_name())
u.log('C26 MATERIAL FIX COMPLETE')
