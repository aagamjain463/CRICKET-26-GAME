"""Fix hero face albedo: solid scan-matched tone + body-mirrored makeup stack.

The unified face shader needs a real mid-tone albedo; the inherited flat-grey
placeholder blew out to white under the match key. The solid tone is the
median of the Player_001 body albedo, so the face enters the identical blood /
foundation / scatter stack with the identical input family. Makeup scalars are
copied from the body instance for head/body coherence; lip restraint stays.
"""
import json
import os
import struct
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
ASSETS = u.AssetToolsHelpers.get_asset_tools()

TONE_SRGB = (118, 119, 124)  # median of T_Body_Basecolor (sRGB 8-bit)
SIZE = 16
FOLDER_FS = ROOT / 'SourceAssets/Generated/Players'
FOLDER_FS.mkdir(parents=True, exist_ok=True)
NAME = 'T_C26_Hero_Face_Albedo'
FS_PATH = FOLDER_FS / (NAME + '.tga')
with open(FS_PATH, 'wb') as handle:
    handle.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 24, 32))
    handle.write(bytes((TONE_SRGB[2], TONE_SRGB[1], TONE_SRGB[0]) * SIZE * SIZE))

DEST = '/Game/Cricket26/Characters/Materials'
task = u.AssetImportTask()
task.filename = str(FS_PATH)
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.replace_existing = True
task.save = True
ASSETS.import_asset_tasks([task])
tex = LIB.load_asset(DEST + '/' + NAME)
assert tex, 'albedo import failed'
tex.set_editor_property('srgb', True)
tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
try:
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_CHARACTER_DIFFUSE)
except Exception:
    pass
LIB.save_loaded_asset(tex)

body = u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin')
face = u.load_asset('/Game/Cricket26/Characters/Materials/MI_C26_Hero_Face')
assert body and face
face.modify()
copied = []
for name in ML.get_scalar_parameter_names(body):
    label = str(name)
    if label.startswith('Makeup '):
        ML.set_material_instance_scalar_parameter_value(
            face, label, ML.get_material_instance_scalar_parameter_value(body, label))
        copied.append(label)
for name in ML.get_vector_parameter_names(body):
    label = str(name)
    if label.startswith('Makeup '):
        ML.set_material_instance_vector_parameter_value(
            face, label, ML.get_material_instance_vector_parameter_value(body, label))
        copied.append(label)
# Lip restraint re-applied after the body copy (lips must not be metallic/vinyl).
for key, value in (('Makeup Lipstick Opacity', 0.35), ('Makeup Lipstick Roughness', 0.42),
                   ('Makeup Lipstick Specular', 0.55), ('Makeup Lipstick Metallic', 0.0),
                   ('Makeup Blusher Opacity', 0.35)):
    ML.set_material_instance_scalar_parameter_value(face, key, value)
ML.set_material_instance_texture_parameter_value(face, 'Basecolor', tex)
for key, rgb in (('Basecolor Noise Low', (0.965, 0.945, 0.925)),
                 ('Basecolor Noise Mid', (0.98, 0.965, 0.95))):
    ML.set_material_instance_vector_parameter_value(face, key, u.LinearColor(*rgb, 1.0))
LIB.save_loaded_asset(face, only_if_is_dirty=False)

errors = []
got_tex = ML.get_material_instance_texture_parameter_value(face, 'Basecolor')
if not got_tex or got_tex.get_path_name() != tex.get_path_name():
    errors.append('basecolor texture not set')
for key, value in (('Makeup Lipstick Metallic', 0.0), ('Makeup Lipstick Opacity', 0.35)):
    if abs(ML.get_material_instance_scalar_parameter_value(face, key) - value) > 1e-4:
        errors.append(key + ' restraint lost')
assert not errors, '; '.join(errors)
(OUT / 'applied-albedo.json').write_text(json.dumps(
    {'albedo': tex.get_path_name(), 'tone_srgb': TONE_SRGB, 'makeup_copied': copied}, indent=2))
u.log('C26_HERO_ALBEDO_APPLIED ' + tex.get_path_name())
