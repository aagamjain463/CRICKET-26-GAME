"""Read-only Unreal material/lighting audit for the Player_001 full-body hero."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
OUT.mkdir(parents=True, exist_ok=True)
ML = u.MaterialEditingLibrary
MESH = '/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate'


def prop(obj, key):
    try:
        return str(obj.get_editor_property(key))
    except Exception:
        return None


def material(mat):
    result = {'path': mat.get_path_name(), 'class': mat.get_class().get_name()}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        values = {}
        for name in getattr(ML, 'get_' + kind + '_parameter_names')(mat):
            prefix = 'get_material_instance_' if isinstance(mat, u.MaterialInstanceConstant) else 'get_material_default_'
            value = getattr(ML, prefix + kind + '_parameter_value')(mat, name)
            if kind == 'texture' and value:
                values[str(name)] = {'path': value.get_path_name(), **{k: prop(value, k) for k in ('srgb', 'compression_settings', 'lod_group', 'max_texture_size')}}
            else:
                values[str(name)] = str(value)
        result[kind] = values
    if isinstance(mat, u.MaterialInstanceConstant):
        result['overrides'] = prop(mat, 'base_property_overrides')
        parent = mat.get_editor_property('parent')
        result['parent'] = material(parent) if parent else None
    else:
        result['properties'] = {k: prop(mat, k) for k in ('shading_model', 'blend_mode', 'two_sided', 'subsurface_profile', 'use_material_attributes')}
    return result


mesh = u.load_asset(MESH)
assert mesh
report = {'mesh': MESH, 'slots': []}
for slot in mesh.get_editor_property('materials'):
    label = str(slot.material_slot_name)
    if any(x in label.lower() for x in ('skin', 'body', 'eye', 'cornea')):
        report['slots'].append({'slot': label, 'material': material(slot.material_interface)})
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
report['lighting'] = {}
for path in ('/Game/Cricket26/Characters/Debug/L_C26_CharacterReview', '/Game/Cricket26/Maps/L_SuperOver'):
    levels.load_level(path)
    lights = []
    for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        comp = actor.get_component_by_class(u.LightComponent)
        if comp:
            lights.append({'actor': actor.get_actor_label(), 'class': actor.get_class().get_name(), 'rotation': str(actor.get_actor_rotation()), **{k: prop(comp, k) for k in ('intensity', 'light_color', 'source_angle', 'cast_shadows')}})
    report['lighting'][path] = lights
(OUT / 'audit-before.json').write_text(json.dumps(report, indent=2))
u.log('C26_HERO_MATERIAL_AUDIT_COMPLETE ' + str(OUT))
