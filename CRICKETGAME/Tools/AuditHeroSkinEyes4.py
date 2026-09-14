"""List tunable scalar/vector names for foundation, makeup, micro, lips."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
mat = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1')
ML = u.MaterialEditingLibrary
scalars = sorted(str(x) for x in ML.get_scalar_parameter_names(mat))
vectors = sorted(str(x) for x in ML.get_vector_parameter_names(mat))
keep_s = [x for x in scalars if any(k in x for k in ('Foundation', 'Concealer', 'Lipstick', 'Blusher', 'Micro', 'Normal Strength', 'Cavity Amount', 'Cavity', 'Blood', 'Scatter Distance', 'Fuzz Opacity', 'Roughness Base', 'Specular Base', 'Wet'))]
keep_v = [x for x in vectors if any(k in x for k in ('Foundation', 'Concealer', 'Lipstick', 'Blusher', 'Multiply', 'Fuzz Color', 'Blood Color', 'Dirt'))]
vals = {}
for n in keep_s:
    vals[n] = str(ML.get_material_instance_scalar_parameter_value(mat, n))
(OUT / 'audit-tunables.json').write_text(json.dumps({'scalars': vals, 'vectors': keep_v}, indent=2))
u.log('C26_HERO_TUNABLES_COMPLETE ' + str(len(vals)))
