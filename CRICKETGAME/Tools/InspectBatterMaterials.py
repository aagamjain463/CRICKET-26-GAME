"""Inspect the materials actually driving the batter's on-field appearance: the skeletal kit
mesh and both pad static meshes, so the hip/pad blotch defect can be traced to a specific asset
instead of guessed from screenshots."""
import unreal as u

LIB = u.EditorAssetLibrary


def dump_material(mat, label):
    if not mat:
        u.log('C26_INSPECT %s: no material' % label)
        return
    u.log('C26_INSPECT %s = %s (class %s)' % (label, mat.get_path_name(), mat.get_class().get_name()))
    # Walk texture parameters if this is a MaterialInstance
    if hasattr(mat, 'texture_parameter_values'):
        for tp in mat.get_editor_property('texture_parameter_values'):
            tex = tp.get_editor_property('parameter_value')
            info = tp.get_editor_property('parameter_info')
            u.log('  texparam %s -> %s' % (info.get_editor_property('name'), tex.get_path_name() if tex else None))


def dump_static_mesh(path, label):
    m = LIB.load_asset(path)
    if not m:
        u.log_error('C26_INSPECT %s missing: %s' % (label, path))
        return
    mats = m.get_editor_property('static_materials') if m.get_class().get_name() == 'StaticMesh' else m.get_editor_property('materials')
    for i, slot in enumerate(mats):
        mat = slot.get_editor_property('material_interface')
        dump_material(mat, '%s slot%d' % (label, i))


def dump_skeletal_mesh(path, label):
    m = LIB.load_asset(path)
    if not m:
        u.log_error('C26_INSPECT %s missing: %s' % (label, path))
        return
    for i, slot in enumerate(m.get_editor_property('materials')):
        mat = slot.get_editor_property('material_interface')
        dump_material(mat, '%s slot%d' % (label, i))


dump_skeletal_mesh('/Game/Cricket26/Characters/SK_Cricketer_KitBase', 'KitBase')
dump_static_mesh('/Game/Cricket26/Equipment/SM_C26_Pad_L', 'PadL')
dump_static_mesh('/Game/Cricket26/Equipment/SM_C26_Pad_R', 'PadR')
