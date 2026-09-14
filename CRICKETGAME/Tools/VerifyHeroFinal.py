"""Final state proof: hero slot bindings + hero face/eye key values (read-only)."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
report = {'slots': {}, 'hero_face': {}, 'hero_eyes': {}}
mesh = u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
for slot in mesh.get_editor_property('materials'):
    label = str(slot.material_slot_name)
    if 'Skin' in label or 'Eye' in label or 'Body' in label:
        report['slots'][label] = slot.material_interface.get_path_name() if slot.material_interface else None
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
for key in ('Specular Base', 'Roughness Base', 'Micro Skin Normal Strength',
            'Makeup Lipstick Metallic', 'Makeup Lipstick Opacity', 'Specular Face Nasal Tip',
            'Roughness Face Nasal Tip', 'Roughness Face Temporal Beard'):
    report['hero_face'][key] = ML.get_material_instance_scalar_parameter_value(face, key)
for key in ('Makeup Foundation Color', 'Basecolor Global Multiply'):
    report['hero_face'][key] = str(ML.get_material_instance_vector_parameter_value(face, key))
for key in ('Use Animated Maps', 'Use Delta Maps'):
    report['hero_face'][key] = str(ML.get_material_instance_static_switch_parameter_value(face, key))
report['hero_face']['Basecolor'] = ML.get_material_instance_texture_parameter_value(face, 'Basecolor').get_path_name()
for eye_name in ('MI_C26_Hero_Eye_Left', 'MI_C26_Hero_Eye_Right'):
    eye = u.load_asset('/Game/Cricket26/Characters/Materials/' + eye_name)
    report['hero_eyes'][eye_name] = {
        key: ML.get_material_instance_scalar_parameter_value(eye, key)
        for key in ('Cornea Roughness', 'Pupil Dilation', 'Iris Global Saturation',
                    'Sclera Irritation Veins Opacity')}
    report['hero_eyes'][eye_name]['Sclera Color Multiply'] = str(
        ML.get_material_instance_vector_parameter_value(eye, 'Sclera Color Multiply'))
(OUT / 'final-state.json').write_text(json.dumps(report, indent=2))
u.log('C26_HERO_FINAL_VERIFIED')
