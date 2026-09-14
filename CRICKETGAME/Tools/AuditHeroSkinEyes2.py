"""Read-only deep audit: all candidate slots + face/body/eye parent chains."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
OUT.mkdir(parents=True, exist_ok=True)
ML = u.MaterialEditingLibrary


def prop(obj, key):
    try:
        return str(obj.get_editor_property(key))
    except Exception:
        return None


def chain(mat, depth=0):
    node = {'path': mat.get_path_name(), 'class': mat.get_class().get_name()}
    if isinstance(mat, u.MaterialInstanceConstant):
        node['scalars'] = len(ML.get_scalar_parameter_names(mat))
        node['vectors'] = len(ML.get_vector_parameter_names(mat))
        node['textures'] = sorted(str(x) for x in ML.get_texture_parameter_names(mat))
        node['switches'] = sorted(str(x) for x in ML.get_static_switch_parameter_names(mat))
        parent = mat.get_editor_property('parent')
        if parent and depth < 6:
            node['parent'] = chain(parent, depth + 1)
    else:
        node['properties'] = {k: prop(mat, k) for k in ('shading_model', 'blend_mode', 'two_sided', 'subsurface_profile')}
    return node


def full_material(mat):
    result = chain(mat)
    # texture values at the leaf only
    leaf_textures = {}
    for name in ML.get_texture_parameter_names(mat):
        value = ML.get_material_instance_texture_parameter_value(mat, name)
        leaf_textures[str(name)] = value.get_path_name() if value else None
    result['leaf_texture_values'] = leaf_textures
    return result


report = {}
for mesh_path in ('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate',
                  '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/SKM_MH_C26_Player_001_BodyMesh',
                  '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/SKM_MH_C26_Player_001_FaceMesh'):
    mesh = u.load_asset(mesh_path)
    slots = []
    for slot in mesh.get_editor_property('materials'):
        slots.append(str(slot.material_slot_name) + ' -> ' + (slot.material_interface.get_path_name() if slot.material_interface else 'None'))
    report[mesh_path] = slots

for mat_path in ('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1',
                 '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin',
                 '/Game/Cricket26/Characters/Materials/MI_C26_FaceGameplay',
                 '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Eye_Left',
                 '/Game/Cricket26/Characters/MetaHumans/Common/Lookdev_UHM/Eye/Materials/MI_eye_eyeball_unified_MH_preset_left',
                 '/Game/Cricket26/Characters/MetaHumans/Common/Lookdev_UHM/Skin/Materials/MI_skin_unified_MH_preset'):
    mat = u.load_asset(mat_path)
    report[mat_path] = full_material(mat) if mat else 'MISSING'

(OUT / 'audit-chains.json').write_text(json.dumps(report, indent=2))
u.log('C26_HERO_CHAINS_COMPLETE')
