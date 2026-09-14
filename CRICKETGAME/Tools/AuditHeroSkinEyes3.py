"""Dump key skin/eye scalars to pick restrained hero tuning values."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
WANT_SCALAR = [x for x in ML.get_scalar_parameter_names(u.load_asset(
    '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1'))
    if any(k in str(x) for k in ('Roughness Base', 'Roughness Face', 'Roughness Body', 'Specular Base', 'Specular Face', 'Specular Body',
                                 'Blood ', 'Scatter Distance Base', 'Fuzz ', 'Noise'))]
WANT_VECTOR = ['Basecolor_Multiply', 'Basecolor Multiply', 'Skin Tone', 'Albedo']
report = {}
for mat_path in ('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1',
                 '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin'):
    mat = u.load_asset(mat_path)
    scalars = {str(n): str(ML.get_material_instance_scalar_parameter_value(mat, n)) for n in ML.get_scalar_parameter_names(mat) if str(n) in [str(w) for w in WANT_SCALAR]}
    vectors = {str(n): str(ML.get_material_instance_vector_parameter_value(mat, n)) for n in ML.get_vector_parameter_names(mat)}
    textures = {}
    for n in ML.get_texture_parameter_names(mat):
        v = ML.get_material_instance_texture_parameter_value(mat, n)
        if 'asecolor' in str(n) or str(n) == 'Normal' or str(n) == 'Cavity':
            textures[str(n)] = v.get_path_name() if v else None
    report[mat_path] = {'scalars': scalars, 'vectors': vectors, 'key_textures': textures,
                        'all_vectors': sorted(str(x) for x in ML.get_vector_parameter_names(mat))}
(OUT / 'audit-params.json').write_text(json.dumps(report, indent=2))
u.log('C26_HERO_PARAMS_COMPLETE')
