"""Diagnose + fix chalk-white hero face. Writes file proof of every readback."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
report = {}

tex = u.load_asset('/Game/Cricket26/Characters/Materials/T_C26_Hero_Face_Albedo')
report['tex_props'] = {k: str(tex.get_editor_property(k)) for k in ('srgb', 'compression_settings', 'lod_group')}
if not tex.get_editor_property('srgb'):
    tex.set_editor_property('srgb', True)
    LIB.save_loaded_asset(tex)
    report['tex_srgb_fixed'] = True

body = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin')
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
for label, mi in (('body', body), ('face', face)):
    switches = {}
    for name in ML.get_static_switch_parameter_names(mi):
        if 'nimated' in str(name) or 'aked' in str(name) or 'lood' in str(name):
            switches[str(name)] = str(ML.get_material_instance_static_switch_parameter_value(mi, name))
    report[label + '_switches'] = switches

# Cover whichever basecolor path is live: all non-VT basecolor slots get albedo.
face.modify()
covered, skipped = [], []
for name in ML.get_texture_parameter_names(face):
    label = str(name)
    if 'asecolor' in label and 'Underwear' not in label and 'VT' not in label:
        try:
            ML.set_material_instance_texture_parameter_value(face, label, tex)
            covered.append(label)
        except Exception as ex:
            skipped.append(label + ' ' + str(ex)[:80])
for switch in ('Use Animated Maps', 'Use Delta Maps'):
    ML.set_material_instance_static_switch_parameter_value(face, switch, False)
LIB.save_loaded_asset(face, only_if_is_dirty=False)

verify = {}
for name in ML.get_texture_parameter_names(face):
    label = str(name)
    if 'asecolor' in label and 'Underwear' not in label and 'VT' not in label:
        value = ML.get_material_instance_texture_parameter_value(face, label)
        verify[label] = value.get_path_name() if value else None
report['covered'] = covered
report['skipped'] = skipped
report['verify'] = verify
report['animated_off'] = str(ML.get_material_instance_static_switch_parameter_value(face, 'Use Animated Maps'))
(OUT / 'fix-white.json').write_text(json.dumps(report, indent=2))
u.log('C26_FACE_WHITE_FIX covered=' + str(len(covered)))
