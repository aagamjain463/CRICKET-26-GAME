"""Export the body albedo for offline tone sampling (read-only, no asset changes)."""
from pathlib import Path
import unreal as u

OUT = Path(u.Paths.project_dir()).resolve() / 'Artifacts/HeroSkinEyes/Sources'
OUT.mkdir(parents=True, exist_ok=True)
tex = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Textures/T_Body_Basecolor')
assert isinstance(tex, u.Texture2D), 'body basecolor missing'
task = u.AssetExportTask()
task.object = tex
task.filename = str(OUT / 'T_Body_Basecolor.tga')
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = u.TextureExporterTGA()
assert u.Exporter.run_asset_export_task(task), 'export failed'
u.log('C26_BODY_TONE_EXPORTED size=' + str(tex.blueprint_get_size_x()) + 'x' + str(tex.blueprint_get_size_y()))
