"""Diff intermediate skin layers to find the face-whitening dial (read-only)."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
head = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Common/Materials/MI_Skin_Head_UI_LOD1')
body = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Common/Materials/MI_Skin_Body_MHC')
report = {'scalar_diffs': {}, 'vector_diffs': {}, 'texture_diffs': {}, 'switch_diffs': {}}
for name in ML.get_scalar_parameter_names(head):
    label = str(name)
    try:
        hv = ML.get_material_instance_scalar_parameter_value(head, label)
        bv = ML.get_material_instance_scalar_parameter_value(body, label)
    except Exception:
        continue
    if abs(hv - bv) > 1e-6:
        report['scalar_diffs'][label] = {'head_layer': hv, 'body_layer': bv}
for name in ML.get_vector_parameter_names(head):
    label = str(name)
    try:
        hv = ML.get_material_instance_vector_parameter_value(head, label)
        bv = ML.get_material_instance_vector_parameter_value(body, label)
    except Exception:
        continue
    if any(abs(a - b) > 1e-4 for a, b in ((hv.r, bv.r), (hv.g, bv.g), (hv.b, bv.b), (hv.a, bv.a))):
        report['vector_diffs'][label] = {'head_layer': str(hv), 'body_layer': str(bv)}
for name in ML.get_texture_parameter_names(head):
    label = str(name)
    try:
        hv = ML.get_material_instance_texture_parameter_value(head, label)
        bv = ML.get_material_instance_texture_parameter_value(body, label)
    except Exception:
        continue
    hp = hv.get_path_name() if hv else None
    bp = bv.get_path_name() if bv else None
    if hp != bp and ('asecolor' in label or 'catter' in label or 'lood' in label or 'lobal' in label):
        report['texture_diffs'][label] = {'head_layer': hp, 'body_layer': bp}
for name in ML.get_static_switch_parameter_names(head):
    label = str(name)
    try:
        hv = ML.get_material_instance_static_switch_parameter_value(head, label)
        bv = ML.get_material_instance_static_switch_parameter_value(body, label)
    except Exception:
        continue
    if hv != bv:
        report['switch_diffs'][label] = {'head_layer': str(hv), 'body_layer': str(bv)}
(OUT / 'layer-diff.json').write_text(json.dumps(report, indent=2))
u.log('C26_LAYER_DIFF scalar=' + str(len(report['scalar_diffs'])) + ' vector=' + str(len(report['vector_diffs'])))
