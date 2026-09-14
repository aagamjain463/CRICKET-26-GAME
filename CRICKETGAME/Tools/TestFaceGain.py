"""Bisection test: report face makeup state, then drop global gain to 0.15."""
import json
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
report = {'before': {}, 'after_gain': (0.15, 0.15, 0.15)}
for key in ('Makeup Foundation Opacity', 'Makeup Foundation Roughness', 'Makeup Concealer Opacity',
            'Makeup Blusher Opacity', 'Roughness Base', 'Specular Base'):
    report['before'][key] = ML.get_material_instance_scalar_parameter_value(face, key)
for key in ('Makeup Foundation Color', 'Makeup Concealer Color', 'Basecolor Global Multiply'):
    report['before'][key] = str(ML.get_material_instance_vector_parameter_value(face, key))
face.modify()
ML.set_material_instance_vector_parameter_value(face, 'Basecolor Global Multiply', u.LinearColor(0.15, 0.15, 0.15, 1.0))
LIB.save_loaded_asset(face, only_if_is_dirty=False)
(OUT / 'gain-test.json').write_text(json.dumps(report, indent=2))
u.log('C26_FACE_GAIN_TEST')
