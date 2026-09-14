"""Diff leaf body vs leaf face skin instances: every switch + scalar (read-only)."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
face = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1')
body = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin')
report = {'switch_diffs': {}, 'scalar_diffs': {}, 'vector_diffs': {}, 'texture_diffs': {}}
for name in ML.get_static_switch_parameter_names(face):
    label = str(name)
    try:
        fv = ML.get_material_instance_static_switch_parameter_value(face, label)
        bv = ML.get_material_instance_static_switch_parameter_value(body, label)
    except Exception:
        continue
    if fv != bv:
        report['switch_diffs'][label] = {'face': str(fv), 'body': str(bv)}
for name in ML.get_scalar_parameter_names(face):
    label = str(name)
    try:
        fv = ML.get_material_instance_scalar_parameter_value(face, label)
        bv = ML.get_material_instance_scalar_parameter_value(body, label)
    except Exception:
        continue
    if abs(fv - bv) > 1e-6:
        report['scalar_diffs'][label] = {'face': fv, 'body': bv}
for name in ML.get_vector_parameter_names(face):
    label = str(name)
    if 'PT' in label or 'Scatter' in label or 'Blood' in label or 'Fuzz' in label or 'Makeup' in label or 'Global' in label or 'Dirt' in label or 'Wet' in label:
        try:
            fv = ML.get_material_instance_vector_parameter_value(face, label)
            bv = ML.get_material_instance_vector_parameter_value(body, label)
        except Exception:
            continue
        if any(abs(a - b) > 1e-4 for a, b in ((fv.r, bv.r), (fv.g, bv.g), (fv.b, bv.b), (fv.a, bv.a))):
            report['vector_diffs'][label] = {'face': str(fv), 'body': str(bv)}
(OUT / 'leaf-diff.json').write_text(json.dumps(report, indent=2))
u.log('C26_LEAF_DIFF switches=' + str(len(report['switch_diffs'])) + ' scalars=' + str(len(report['scalar_diffs'])))
