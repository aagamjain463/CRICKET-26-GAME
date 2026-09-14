"""Calibrate hero face albedo to the近く body tone (warm mid-brown, gain back to 1)."""
import json
import struct
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
ASSETS = u.AssetToolsHelpers.get_asset_tools()

TONE_SRGB = (100, 60, 38)
SIZE = 16
FS_PATH = ROOT / 'SourceAssets/Generated/Players/T_C26_Hero_Face_Albedo.tga'
with open(FS_PATH, 'wb') as handle:
    handle.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 24, 32))
    handle.write(bytes((TONE_SRGB[2], TONE_SRGB[1], TONE_SRGB[0]) * SIZE * SIZE))

task = u.AssetImportTask()
task.filename = str(FS_PATH)
task.destination_path = '/Game/Cricket26/Characters/Materials'
task.destination_name = 'T_C26_Hero_Face_Albedo'
task.automated = True
task.replace_existing = True
task.save = True
ASSETS.import_asset_tasks([task])
tex = LIB.load_asset('/Game/Cricket26/Characters/Materials/T_C26_Hero_Face_Albedo')
assert tex
tex.set_editor_property('srgb', True)
LIB.save_loaded_asset(tex)

face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
face.modify()
ML.set_material_instance_vector_parameter_value(face, 'Basecolor Global Multiply', u.LinearColor(1, 1, 1, 1))
LIB.save_loaded_asset(face, only_if_is_dirty=False)
(OUT / 'calibrate.json').write_text(json.dumps({'tone_srgb': TONE_SRGB, 'gain': 1.0}, indent=2))
u.log('C26_FACE_CALIBRATED')
