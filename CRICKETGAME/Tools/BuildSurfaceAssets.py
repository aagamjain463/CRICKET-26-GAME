"""Original, tileable mobile surface assets; no downloaded images or procedural Noise shader.

Run inside UnrealEditor-Cmd with -run=pythonscript -script=<absolute path>.
Texture generation is deterministic. Existing source characters, animations and map are untouched.
Every material connection is checked; failed graphs must never be silently saved as complete.
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
folder = os.path.join(u.Paths.project_dir(), 'SourceAssets', 'Generated', 'Surfaces')
os.makedirs(folder, exist_ok=True)
lib.make_directory(ROOT + '/Textures')
SIZE = 512


def texture(name, kind, normal=False):
    rng = random.Random(2626 + kind)
    heights = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            if kind == 2:  # Longitudinal willow grain.
                value = .5 + .22 * math.sin(x*.34 + math.sin(y*.024)*.7) + rng.uniform(-.065, .065)
            elif kind == 3:  # Woven cloth, backed off by mips at playing distance.
                value = .5 + .18*math.sin(x*math.pi/2)*math.sin(y*math.pi/2)
            else:
                value = .5 + rng.uniform(-.20, .20) + .10*math.sin(x*.073)*math.sin(y*.061)
                if kind == 0:
                    value += .07*math.sin(x*1.8 + y*.16)
            row.append(value)
        heights.append(row)
    data = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            if normal:
                dx = (heights[y][(x+1)%SIZE]-heights[y][(x-1)%SIZE])*.38
                dy = (heights[(y+1)%SIZE][x]-heights[(y-1)%SIZE][x])*.38
                length = math.sqrt(dx*dx+dy*dy+1)
                rgb = (int((.5-dx/length*.5)*255), int((.5-dy/length*.5)*255), int((.5+1/length*.5)*255))
            else:
                v = max(0, min(255, int(180+heights[y][x]*70)))
                rgb = (v, v, v)
            data.extend((rgb[2], rgb[1], rgb[0]))
    path = os.path.join(folder, name+'.tga')
    with open(path, 'wb') as output:
        output.write(struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,SIZE,SIZE,24,32))
        output.write(data)
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = ROOT + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    assets.import_asset_tasks([task])
    tex = lib.load_asset(ROOT+'/Textures/'+name)
    assert tex, name
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if normal else u.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('never_stream', False)
    tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if normal else u.TextureGroup.TEXTUREGROUP_WORLD)
    lib.save_loaded_asset(tex)
    return tex


def node(mat, cls, **props):
    result = ML.create_material_expression(mat, cls)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result


def link(a, output, b, pin):
    names = ML.get_material_expression_input_names(b)
    # Read reflected pin names from this installed engine; display labels differ from member names.
    if pin in ('UVs', 'Coordinates') and pin not in names:
        pin = names[0]
    assert ML.connect_material_expressions(a, output, b, pin), (a.get_name(), output, b.get_name(), pin, names)


def output(a, pin, prop):
    assert ML.connect_material_property(a, pin, prop), (a.get_name(), pin, str(prop))


def material(name, color, roughness, kind=None, worldscale=.012, unlit=False):
    path = ROOT+'/Materials/'+name
    mat = lib.load_asset(path) if lib.does_asset_exist(path) else assets.create_asset(name, ROOT+'/Materials', u.Material, u.MaterialFactoryNew())
    ML.delete_all_material_expressions(mat)
    tint = node(mat, u.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=u.LinearColor(*color,1))
    base = tint
    if kind is not None:
        detail = texture('T_'+name[2:]+'_Detail', kind)
        normal = texture('T_'+name[2:]+'_Normal', kind, True)
        sample = node(mat, u.MaterialExpressionTextureSampleParameter2D, parameter_name='Detail', texture=detail)
        normal_sample = node(mat, u.MaterialExpressionTextureSampleParameter2D, parameter_name='NormalDetail', texture=normal,
                             sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if kind < 2:
            world = node(mat, u.MaterialExpressionWorldPosition)
            mask = node(mat, u.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
            link(world, '', mask, '')
            uv = node(mat, u.MaterialExpressionMultiply, const_b=worldscale)
            link(mask, '', uv, 'A')
        else:
            uv = node(mat, u.MaterialExpressionTextureCoordinate, u_tiling=1 if kind == 2 else 24, v_tiling=1 if kind == 2 else 24)
        link(uv, '', sample, '')
        link(uv, '', normal_sample, '')
        base = node(mat, u.MaterialExpressionMultiply)
        link(tint, '', base, 'A')
        link(sample, 'RGB', base, 'B')
        output(normal_sample, 'RGB', u.MaterialProperty.MP_NORMAL)
    output(base, '', u.MaterialProperty.MP_BASE_COLOR)
    rough = node(mat, u.MaterialExpressionScalarParameter, parameter_name='Roughness', default_value=roughness)
    output(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    spec = node(mat, u.MaterialExpressionScalarParameter, parameter_name='Specular', default_value=.22)
    output(spec, '', u.MaterialProperty.MP_SPECULAR)
    glow = node(mat, u.MaterialExpressionScalarParameter, parameter_name='Glow', default_value=1 if unlit else 0)
    emission = node(mat, u.MaterialExpressionMultiply)
    link(tint, '', emission, 'A')
    link(glow, '', emission, 'B')
    output(emission, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('used_with_skeletal_mesh', name == 'M_Fabric')
    if unlit:
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    ML.layout_material_expressions(mat)
    ML.recompile_material(mat)
    lib.save_loaded_asset(mat)
    u.log('C26_SURFACE_BUILT '+name)


material('M_Grass', (.045,.13,.035), .95, 0, .009)
material('M_Pitch', (.24,.19,.115), .93, 1, .018)
material('M_Willow', (.52,.38,.19), .72, 2)
material('M_Fabric', (.02,.24,.28), .9, 3)
material('M_Sky', (.004,.009,.023), 1, unlit=True)
u.log('C26_SURFACES_COMPLETE')
