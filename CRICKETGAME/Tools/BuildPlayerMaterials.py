"""Premium player materials: cloth, protective-gear leather, hard shell and skin.

Run: UnrealEditor-Cmd <abs>/CRICKETGAME.uproject -run=pythonscript -script=<abs>/Tools/BuildPlayerMaterials.py

Why this exists
---------------
Measured with Tools/ProbePlayers.py, every athlete surface except the shirt and the bat was a
`M_Surface` instance: a flat `Tint` vector and one `Roughness` scalar, with no texture of any
kind. So trousers, pads, gloves, helmet, shoes and buckles all returned painted plastic -- one
constant albedo and one constant highlight each -- and the only thing separating a pad from a
helmet from a trouser leg at broadcast distance was a slightly different flat colour. Cricket
whites have nothing else to read by: no pattern, no print, just how light breaks over a weave and
grazes along a fold.

`M_C26_PlayerSkin` was worse than flat: it had no parameters at all, so every
`SetVectorParameterValue(TEXT("Tint"), ...)` the athlete made against it was silently discarded
and all eleven players rendered the same raw imported body texture.

Design rules, learned the hard way in this repo
-----------------------------------------------
* Every graph here samples a real `TextureSampleParameter2D`. The materials built with a
  `WorldPosition -> Multiply -> Noise` chain (`M_Grass`, `M_Pitch`, `M_Crowd`) silently fall back
  to the engine default grey on this renderer. Nothing here uses a Noise node.
* `two_sided=True` on everything, matching `M_Surface`. The authored equipment is single-sided in
  places, so a one-sided material makes pad and glove faces vanish from behind.
* Parameter names stay compatible with `M_Surface` (`Tint`, `Roughness`, `Glow`) so every existing
  `UMaterialInstanceDynamic` call in `AC26Athlete` keeps working, and the extra parameters
  (`Detail`, `Sheen`, `NormalStrength`, `NormalTiling`) are additive.
* Both skeletal-mesh and static-mesh usage flags are set, because the same instance family is used
  on the skinned body and on the authored static equipment.

Textures are generated deterministically into SourceAssets/Generated/Players and imported, so no
downloaded image is involved. Cloth and skin reuse the already-imported base-character maps, which
are the only ones in the project with genuine fold and pore detail.
"""
import math
import os
import random
import struct

import unreal as u

ROOT = '/Game/Cricket26'
ML = u.MaterialEditingLibrary
lib = u.EditorAssetLibrary
assets = u.AssetToolsHelpers.get_asset_tools()
FOLDER = os.path.join(u.Paths.project_dir(), 'SourceAssets', 'Generated', 'Players')
os.makedirs(FOLDER, exist_ok=True)
SIZE = 512

REPORT = []


def note(msg):
    REPORT.append(str(msg))


def make_texture(name, kind, normal=False):
    """Deterministic tileable height field -> TGA -> imported Texture2D.

    kind 0: leather grain, a coarse cell structure with fine cracks through it.
    kind 1: hard shell, a very fine isotropic micro-grain.
    kind 2: skin, fine pores with a slower broad undulation so it never reads as sandpaper.

    Every texture this produces is bright and near-uniform, because the graph below multiplies
    the tint by the texture's luminance rescaled to sit around 1.0. Feeding a dark, non-tiling
    photographic albedo in here is what turned the whole team black on the first attempt: the
    base character's street-top diffuse is a full garment atlas with large black regions in it,
    so tiling it 18 times across a shirt sampled mostly the dark parts and dragged the team
    colour down with them.
    """
    rng = random.Random(9174 + kind)
    heights = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            if kind == 0:
                # Cells about 24 px across with a soft ridge between them, then fine cracking.
                cell = (math.sin(x * math.pi / 12.0) * math.sin(y * math.pi / 12.0)) * .5
                value = .5 + .30 * cell + rng.uniform(-.10, .10)
                value += .06 * math.sin(x * .9 + y * .31)
            elif kind == 2:
                value = .5 + rng.uniform(-.055, .055) + .05 * math.sin(x * .06) * math.sin(y * .055)
            else:
                value = .5 + rng.uniform(-.14, .14) + .05 * math.sin(x * .41) * math.sin(y * .37)
            row.append(value)
        heights.append(row)
    data = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            if normal:
                dx = (heights[y][(x + 1) % SIZE] - heights[y][(x - 1) % SIZE]) * .38
                dy = (heights[(y + 1) % SIZE][x] - heights[(y - 1) % SIZE][x]) * .38
                length = math.sqrt(dx * dx + dy * dy + 1)
                rgb = (int((.5 - dx / length * .5) * 255),
                       int((.5 - dy / length * .5) * 255),
                       int((.5 + 1 / length * .5) * 255))
            else:
                v = max(0, min(255, int(180 + heights[y][x] * 70)))
                rgb = (v, v, v)
            data.extend((rgb[2], rgb[1], rgb[0]))
    path = os.path.join(FOLDER, name + '.tga')
    with open(path, 'wb') as handle:
        handle.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 24, 32))
        handle.write(data)
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = ROOT + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    assets.import_asset_tasks([task])
    tex = lib.load_asset(ROOT + '/Textures/' + name)
    assert tex, 'import failed ' + name
    tex.set_editor_property('srgb', False)
    tex.set_editor_property(
        'compression_settings',
        u.TextureCompressionSettings.TC_NORMALMAP if normal else u.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('never_stream', False)
    tex.set_editor_property(
        'lod_group',
        u.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if normal else u.TextureGroup.TEXTUREGROUP_WORLD)
    lib.save_loaded_asset(tex)
    return tex


def node(mat, cls, **props):
    result = ML.create_material_expression(mat, cls)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result


def link(a, out, b, pin):
    names = ML.get_material_expression_input_names(b)
    if pin in ('UVs', 'Coordinates') and pin not in names:
        pin = names[0]
    assert ML.connect_material_expressions(a, out, b, pin), \
        (a.get_name(), out, b.get_name(), pin, names)
    return b


def output(a, pin, prop):
    assert ML.connect_material_property(a, pin, prop), (a.get_name(), pin, str(prop))


def scalar(mat, name, value):
    return node(mat, u.MaterialExpressionScalarParameter, parameter_name=name,
                default_value=value)


def load_texture(path):
    tex = lib.load_asset(path)
    assert tex, 'missing texture ' + path
    return tex


def build(name, cloth_path, normal_path, rough, spec, sheen, detail, tiling,
          normal_strength, tint=(1.0, 1.0, 1.0)):
    """One material. Tint x (albedo detail) -> base colour, sampled normal -> normal,
    Fresnel sheen + constant specular, and a parameterised roughness."""
    path = ROOT + '/Materials/' + name
    # Delete and recreate rather than reusing + delete_all_material_expressions: that call
    # asserts (!IsRooted) on a freshly created material in this 5.8 commandlet and takes the whole
    # editor down. Deleting the asset first and building on an empty material is the pattern
    # Tools/CreateAthleteMaterial.py already uses successfully here.
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mat = assets.create_asset(name, ROOT + '/Materials', u.Material, u.MaterialFactoryNew())
    assert mat, 'failed to create ' + name
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)

    tint = node(mat, u.MaterialExpressionVectorParameter, parameter_name='Tint',
                default_value=u.LinearColor(tint[0], tint[1], tint[2], 1.0))

    # UVs. The equipment meshes are Blender-authored with their own 0-1 layout; the skinned body
    # has a body atlas. Tiling is a parameter so one material serves both without a rebuild.
    uv = node(mat, u.MaterialExpressionTextureCoordinate)
    tiles = scalar(mat, 'NormalTiling', tiling)
    uv = link(uv, '', node(mat, u.MaterialExpressionMultiply), 'A')
    link(tiles, '', uv, 'B')

    weave = node(mat, u.MaterialExpressionTextureSampleParameter2D,
                 parameter_name='Cloth', texture=load_texture(cloth_path),
                 sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(uv, '', weave, '')
    # The source albedo averages well below mid grey, so its luminance is rescaled to sit around
    # 1.0 before it modulates the tint: this has to add fold and seam shading, not darken the kit.
    grey = link(weave, 'RGB', node(mat, u.MaterialExpressionDesaturation), '')
    gain = node(mat, u.MaterialExpressionMultiply, const_b=2.35)
    link(grey, '', gain, 'A')
    flat = node(mat, u.MaterialExpressionConstant, r=1.0)
    amount = scalar(mat, 'Detail', detail)
    folds = node(mat, u.MaterialExpressionLinearInterpolate)
    link(flat, '', folds, 'A')
    link(gain, '', folds, 'B')
    link(amount, '', folds, 'Alpha')
    base = node(mat, u.MaterialExpressionMultiply)
    link(tint, 'RGB', base, 'A')
    link(folds, '', base, 'B')
    output(base, '', u.MaterialProperty.MP_BASE_COLOR)

    # Normal. NormalStrength lets a hard shell stay crisp while a matte pad face is softened,
    # from the same texture, per instance.
    bump = node(mat, u.MaterialExpressionTextureSampleParameter2D,
                parameter_name='Weave', texture=load_texture(normal_path),
                sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(uv, '', bump, '')
    strength = scalar(mat, 'NormalStrength', normal_strength)
    # A flat normal is (0,0,1); flattening means lerping the sampled tangent normal toward it.
    flat_normal = node(mat, u.MaterialExpressionConstant3Vector,
                       constant=u.LinearColor(0.0, 0.0, 1.0, 0.0))
    blend = node(mat, u.MaterialExpressionLinearInterpolate)
    link(flat_normal, '', blend, 'A')
    link(bump, 'RGB', blend, 'B')
    link(strength, '', blend, 'Alpha')
    output(blend, '', u.MaterialProperty.MP_NORMAL)

    output(scalar(mat, 'Roughness', rough), '', u.MaterialProperty.MP_ROUGHNESS)

    # Cloth sheen. Fabric is a mat of fibres, so it lights up along every silhouette edge and down
    # the side of every fold facing away from the key. A constant specular cannot do that, and it
    # is most of what separates a jersey from painted plastic at broadcast distance.
    rim = node(mat, u.MaterialExpressionFresnel, exponent=4.2, base_reflect_fraction=0.04)
    lift = node(mat, u.MaterialExpressionMultiply)
    link(rim, '', lift, 'A')
    link(scalar(mat, 'Sheen', sheen), '', lift, 'B')
    total = node(mat, u.MaterialExpressionAdd, const_b=spec)
    link(lift, '', total, 'A')
    output(total, '', u.MaterialProperty.MP_SPECULAR)

    glow = node(mat, u.MaterialExpressionMultiply)
    link(base, '', glow, 'A')
    link(scalar(mat, 'Glow', 0.0), '', glow, 'B')
    output(glow, '', u.MaterialProperty.MP_EMISSIVE_COLOR)

    ML.layout_material_expressions(mat)
    ML.recompile_material(mat)
    lib.save_loaded_asset(mat)
    note('BUILT %s params=%s' % (name, [str(p) for p in ML.get_scalar_parameter_names(mat)]))
    return mat


CHARS = ROOT + '/Characters/'
TEX = ROOT + '/Textures/'

# Generate the grain textures. Leather is coarse-celled with fine cracking; shell is a very fine
# micro-grain that reads as moulded plastic at 20 cm and as nothing at 9 m; skin is pore-scale.
make_texture('T_C26_Leather_Detail', 0)
make_texture('T_C26_Leather_Normal', 0, normal=True)
make_texture('T_C26_Shell_Detail', 1)
make_texture('T_C26_Shell_Normal', 1, normal=True)
make_texture('T_C26_Skin_Detail', 2)
make_texture('T_C26_Skin_Normal', 2, normal=True)

# Woven kit. Reuses the tileable cloth maps M_Fabric already renders on this exact mesh, so the
# weave is a known-good surface rather than a new one. Rough, low specular, strong sheen.
build('M_C26_Cloth', TEX + 'T_Fabric_Detail', TEX + 'T_Fabric_Normal',
      rough=0.88, spec=0.20, sheen=0.62, detail=0.70, tiling=24.0, normal_strength=1.0)

# Protective gear. Leather and webbing are matte with a coarse grain; the grain is what stops a
# pad face reading as a slab of white plastic. Higher specular than cloth, much lower sheen.
build('M_C26_Gear', TEX + 'T_C26_Leather_Detail', TEX + 'T_C26_Leather_Normal',
      rough=0.78, spec=0.30, sheen=0.16, detail=0.55, tiling=6.0, normal_strength=0.9)

# Hard shell: helmet, buckles, grille bars, shoe sole. Low roughness, crisp micro-grain, real
# specular, no sheen. This is the piece that has to return a hard highlight under the floodlights.
build('M_C26_Shell', TEX + 'T_C26_Shell_Detail', TEX + 'T_C26_Shell_Normal',
      rough=0.26, spec=0.44, sheen=0.05, detail=0.28, tiling=10.0, normal_strength=0.65)

# Skin. This one is a rebuild of an existing material that had NO parameters at all, so every
# Tint and Roughness the athlete set against it was thrown away and all eleven players rendered
# one identical face value. The imported body diffuse is deliberately NOT used as the albedo:
# it renders almost black on a vertical torso under this night rig, which is why the athlete
# code was already overriding it with a flat readable tone. That tone stays as the Tint, and it
# now carries pore detail and a pore normal on top instead of being perfectly flat.
build('M_C26_PlayerSkin', TEX + 'T_C26_Skin_Detail', TEX + 'T_C26_Skin_Normal',
      rough=0.58, spec=0.34, sheen=0.22, detail=0.32, tiling=44.0, normal_strength=0.75)

note('C26_PLAYERMAT_COMPLETE')
with open(os.path.join(u.Paths.project_dir(), 'Artifacts', 'player_materials.txt'), 'w') as handle:
    handle.write('\n'.join(REPORT) + '\n')
