"""Dump hero face switches + every basecolor-ish texture binding (read-only)."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
report = {'switches': {}, 'basecolor_slots': {}}
for name in ML.get_static_switch_parameter_names(face):
    report['switches'][str(name)] = str(ML.get_material_instance_static_switch_parameter_value(face, name))
for name in ML.get_texture_parameter_names(face):
    label = str(name)
    if 'asecolor' in label or 'aseColor' in label:
        value = ML.get_material_instance_texture_parameter_value(face, label)
        report['basecolor_slots'][label] = value.get_path_name() if value else None
(OUT / 'audit-face-slots.json').write_text(json.dumps(report, indent=2))
u.log('C26_FACE_SLOTS_DUMPED')
