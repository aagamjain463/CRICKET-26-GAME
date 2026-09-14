"""Hero skin+eye material pass for ONE player (Player_001 full-body candidate).

What it does (parameter-only, no new shaders, textures or geometry):
- MI_C26_Hero_Face: child of the Player_001 unified face skin. Gives the face
  the same subsurface skin shader the body already uses (replacing the cheap
  flat default-lit pigment), drives its missing albedo from the foundation
  channel matched to the body's baked tone, and replaces the uniform 0.9
  specular with restrained oily/dry separation.
- MI_C26_Hero_Body: child of the Player_001 unified body skin with the same
  broad-zone specular/roughness restraint so head and limbs stay coherent.
- MI_C26_Hero_Eye_Left/Right: children of the Player_001 eye materials with
  dimmer sclera, visible-but-subtle veins, softer cornea, daylight pupil and
  natural iris saturation.
- Repoints only the SK_C26_FullBody_Candidate face/body/eye slots at them.

Run: UnrealEditor-Cmd <proj> -run=pythonscript -script=<abs>/Tools/ApplyHeroSkinEyes.py
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Artifacts/HeroSkinEyes'
OUT.mkdir(parents=True, exist_ok=True)
ML = u.MaterialEditingLibrary
LIB = u.EditorAssetLibrary
ASSETS = u.AssetToolsHelpers.get_asset_tools()
FOLDER = '/Game/Cricket26/Characters/Materials'
TONE = (0.419608, 0.192157, 0.125490)  # body baked tone (sRGB makeup channels)

FACE_SCALARS = {
    'Makeup Foundation Opacity': 1.0, 'Makeup Foundation Roughness': 0.68,
    'Makeup Foundation Specular': 0.55,
    'Makeup Concealer Roughness': 0.66, 'Makeup Concealer Specular': 0.55,
    'Makeup Lipstick Opacity': 0.35, 'Makeup Lipstick Roughness': 0.42,
    'Makeup Lipstick Specular': 0.55, 'Makeup Lipstick Metallic': 0.0,
    'Makeup Blusher Opacity': 0.35,
    'Specular Base': 0.55,
    'Specular Face Frontal Main': 0.62, 'Specular Face Frontal BetweenBrows': 0.66,
    'Specular Face Orbital Main': 0.62, 'Specular Face Orbital Brows': 0.60,
    'Specular Face Orbital Eyelids Upper': 0.66, 'Specular Face Orbital Eyelids Lower': 0.66,
    'Specular Face Orbital Eyelids Edge': 0.66, 'Specular Face Orbital Eyebags': 0.62,
    'Specular Face Orbital Crowsfeet': 0.62,
    'Specular Face Nasal Main': 0.62, 'Specular Face Nasal Bridge': 0.64,
    'Specular Face Nasal Nostrils': 0.62, 'Specular Face Nasal Tip': 0.68,
    'Specular Face Oral Main': 0.62, 'Specular Face Oral Mustache': 0.60,
    'Specular Face Oral Lip Upper': 0.60, 'Specular Face Oral Lip Lower': 0.60,
    'Specular Face Auricular Main': 0.62, 'Specular Face Auricular Inner': 0.62,
    'Specular Face Auricular Outer': 0.62,
    'Specular Face Temporal Main': 0.62, 'Specular Face Temporal Beard': 0.58,
    'Specular Face Temporal Temples': 0.62, 'Specular Face Temporal Chin': 0.62,
    'Specular Face Temporal Cheeks': 0.62, 'Specular Face Scalp': 0.62,
    'Specular Noise Mid Multiply': 0.55, 'Specular Noise High Multiply': 0.40,
    'Roughness Base': 0.70,
    'Roughness Face Frontal Main': 0.64, 'Roughness Face Frontal BetweenBrows': 0.62,
    'Roughness Face Orbital Main': 0.70, 'Roughness Face Orbital Brows': 0.70,
    'Roughness Face Orbital Eyelids Upper': 0.60, 'Roughness Face Orbital Eyelids Lower': 0.60,
    'Roughness Face Orbital Eyebags': 0.64, 'Roughness Face Orbital Crowsfeet': 0.68,
    'Roughness Face Nasal Main': 0.58, 'Roughness Face Nasal Bridge': 0.57,
    'Roughness Face Nasal Nostrils': 0.52, 'Roughness Face Nasal Tip': 0.50,
    'Roughness Face Oral Main': 0.70, 'Roughness Face Oral Mustache': 0.78,
    'Roughness Face Oral Lip Upper': 0.58, 'Roughness Face Oral Lip Lower': 0.56,
    'Roughness Face Auricular Inner': 0.45,
    'Roughness Face Temporal Main': 0.66, 'Roughness Face Temporal Beard': 0.72,
    'Roughness Face Temporal Temples': 0.67, 'Roughness Face Temporal Chin': 0.69,
    'Roughness Face Temporal Cheeks': 0.68, 'Roughness Face Scalp': 0.64,
    'Roughness Noise Mid Multiply': 0.80, 'Roughness Noise High Multiply': 0.60,
    'Micro Skin Normal Strength': 0.50,
}
FACE_VECTORS = {
    'Makeup Foundation Color': u.LinearColor(*TONE, 1.0),
    'Makeup Concealer Color': u.LinearColor(*TONE, 1.0),
    'Makeup Lipstick Color': u.LinearColor(0.234375, 0.028076, 0.037746, 1.0),
}
BODY_SCALARS = dict(FACE_SCALARS)
BODY_SCALARS.update({
    'Specular Body': 0.62, 'Specular Body Neck Main': 0.62, 'Specular Body Neck Throat': 0.62,
    'Specular Body Neck Back': 0.62, 'Specular Body Chest': 0.62,
    'Specular Body Arms Main': 0.62, 'Specular Body Arms Top': 0.62,
    'Specular Body Hands Main': 0.62, 'Specular Body Hands Palm': 0.62,
    'Specular Body Legs': 0.62, 'Specular Body Feet Main': 0.62, 'Specular Body Feet Sole': 0.62,
    'Roughness Body': 0.66, 'Roughness Body Neck Back': 0.68, 'Roughness Body Chest': 0.72,
    'Roughness Body Arms Main': 0.68, 'Roughness Body Arms Top': 0.68,
    'Roughness Body Hands Main': 0.70, 'Roughness Body Hands Palm': 0.72,
    'Roughness Body Legs': 0.68, 'Roughness Body Feet Main': 0.70,
})
EYE_SCALARS = {
    'Cornea Roughness': 0.12, 'Pupil Dilation': 0.55,
    'Iris Global Saturation': 1.35, 'Sclera Irritation Veins Opacity': 0.25,
    'Iris Normal Strength': 0.8, 'Sclera Normal Strength': 0.8,
}
EYE_VECTORS = {'Sclera Color Multiply': u.LinearColor(0.82, 0.79, 0.775, 1.0)}


def make_child(name, parent_path, scalars, vectors):
    parent = u.load_asset(parent_path)
    assert parent, 'missing parent ' + parent_path
    path = FOLDER + '/' + name
    mi = u.load_asset(path)
    if mi is None:
        mi = ASSETS.create_asset(name, FOLDER, u.MaterialInstanceConstant,
                                 u.MaterialInstanceConstantFactoryNew())
    assert mi, 'create failed ' + path
    ML.set_material_instance_parent(mi, parent)
    for key, value in scalars.items():
        ML.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in vectors.items():
        ML.set_material_instance_vector_parameter_value(mi, key, value)
    LIB.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


report = {}
face = make_child('MI_C26_Hero_Face',
                  '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Skin_LOD1',
                  FACE_SCALARS, FACE_VECTORS)
body = make_child('MI_C26_Hero_Body',
                  '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Body/Materials/MI_Body_Skin',
                  BODY_SCALARS, {})
eye_base = '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Materials/MI_Face_Eye_'
eye_l = make_child('MI_C26_Hero_Eye_Left', eye_base + 'Left', EYE_SCALARS, EYE_VECTORS)
eye_r = make_child('MI_C26_Hero_Eye_Right', eye_base + 'Right', EYE_SCALARS, EYE_VECTORS)
report['instances'] = [x.get_path_name() for x in (face, body, eye_l, eye_r)]

mesh = u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
assert mesh
targets = {'MI_Face_Skin_LOD1': face, 'MI_Body_Skin': body,
           'MI_Face_Eye_Left': eye_l, 'MI_Face_Eye_Right': eye_r}
mesh.modify()
slots = list(mesh.get_editor_property('materials'))
repointed = []
for i, slot in enumerate(slots):
    label = str(slot.material_slot_name)
    if label in targets:
        slot.set_editor_property('material_interface', targets[label])
        repointed.append(label)
        slots[i] = slot
assert sorted(repointed) == sorted(targets), 'missing slots ' + str(repointed)
mesh.set_editor_property('materials', slots)
LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
report['repointed_slots'] = repointed

# Read-back verification: every intended scalar must round-trip.
errors = []
for mi, values in ((face, FACE_SCALARS), (body, BODY_SCALARS), (eye_l, EYE_SCALARS), (eye_r, EYE_SCALARS)):
    for key, want in values.items():
        got = ML.get_material_instance_scalar_parameter_value(mi, key)
        if abs(got - want) > 1e-4:
            errors.append(mi.get_name() + ' ' + key + ' want=' + str(want) + ' got=' + str(got))
got_vec = ML.get_material_instance_vector_parameter_value(face, 'Makeup Foundation Color')
if abs(got_vec.r - TONE[0]) > 1e-4:
    errors.append('foundation tone mismatch ' + str(got_vec))
assert not errors, '; '.join(errors)
report['verified_scalars'] = sum(len(v) for _, v in ((face, FACE_SCALARS), (body, BODY_SCALARS), (eye_l, EYE_SCALARS), (eye_r, EYE_SCALARS)))
(OUT / 'applied.json').write_text(json.dumps(report, indent=2))
u.log('C26_HERO_SKIN_APPLIED ' + str(report))
