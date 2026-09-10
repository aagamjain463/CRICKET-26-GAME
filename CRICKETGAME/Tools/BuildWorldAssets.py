"""Import original Blender venue modules and build the continuous cricket-ground material.

UE 5.8 editor commandlet. All source is deterministic and original. Existing athlete,
animation, map, rules and material assets are retained. Three texture reads on turf;
no shader noise, layered grass geometry, tessellation or runtime texture generation.
"""
import math
import os
import struct
import unreal as u

ROOT='/Game/Cricket26/Environment'
ML=u.MaterialEditingLibrary
lib=u.EditorAssetLibrary
assets=u.AssetToolsHelpers.get_asset_tools()
project=u.Paths.project_dir()


def node(mat,cls,**props):
    n=ML.create_material_expression(mat,cls)
    for k,v in props.items(): n.set_editor_property(k,v)
    return n


def link(a,b,pin,output=''):
    names=ML.get_material_expression_input_names(b)
    if pin in ('UVs','Coordinates') and pin not in names: pin=names[0]
    assert ML.connect_material_expressions(a,output,b,pin),(a.get_name(),b.get_name(),pin,names)


def result(n,prop,output=''):
    assert ML.connect_material_property(n,output,prop)


def material(name):
    path=ROOT+'/Materials'
    lib.make_directory(path)
    m=lib.load_asset(path+'/'+name) if lib.does_asset_exist(path+'/'+name) else assets.create_asset(name,path,u.Material,u.MaterialFactoryNew())
    ML.delete_all_material_expressions(m)
    m.set_editor_property('two_sided',False)
    return m


def finish(m):
    ML.layout_material_expressions(m)
    ML.recompile_material(m)
    lib.save_loaded_asset(m)
    u.log('C26_WORLD_MATERIAL '+m.get_path_name())


def mix(a,b,t):return tuple(x+(y-x)*t for x,y in zip(a,b))
def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)))
    return t*t*(3-2*t)


# One low-frequency colour map covers the ground. Tiny detail is a separate tiled
# normal/detail pair. The strip and square are colour transitions within ONE mesh.
size=2048
data=bytearray()
for iy in range(size):
    y=(iy/(size-1)-.5)*18800
    for ix in range(size):
        x=(ix/(size-1)-.5)*18000
        broad=math.sin(x*.0017+math.sin(y*.002))*math.sin(y*.0021)
        grain=math.sin(x*.137+y*.071)*math.sin(y*.193-x*.071)
        # Mower stripes. A groundsman cuts the outfield in one direction and the blades lie toward
        # or away from the camera, so alternate 6 m bands genuinely differ in brightness. At the
        # previous 5.2 percent they survived neither the tonemapper nor a phone screen: sampling
        # the render showed the outfield reading as one flat sheet of green from the batting camera.
        mow=math.tanh(math.sin((y+x*.22)*math.pi/640)*3.2)
        # The square is shaved flat in a single pass, so the stripes stop dead at its edge. That
        # contrast -- striped outfield against a plain, paler square -- is what actually identifies
        # a cricket ground, and it was being lost in a 65 cm fade that smeared the two together.
        insq=(1-smooth(690,742,abs(x)))*(1-smooth(1180,1242,abs(y)))
        v=1+.092*mow*(1-insq)+.035*broad+.011*grain
        c=tuple(a*v for a in (.068,.205,.048))
        c=mix(c,(.091*v,.186*v,.055*v),insq*.72)
        # Prepared strips: 3.05 m of rolled, closely shaved turf each, with a defined shoulder and
        # its own age. Overlapping soft falloffs turned the whole square into one pale stain.
        for s in (-2,-1,1,2):
            d=abs(x-s*305)
            if d<155:
                rest=(1-smooth(130,149,d))*(1-smooth(1080,1180,abs(y)))
                age=.88+.13*((s*7)%3)
                c=mix(c,(.118*v*age,.183*v*age,.066*v*age),rest*.55)
        edge=abs(x)+grain*2.5
        pitch=(1-smooth(143,154,edge))*(1-smooth(1153,1185,abs(y)+grain*4))
        if pitch>0:
            # Footmark ellipses and diffuse baked abrasion, no rectangular decals.
            wear=0
            for side in [-1,1]:
                for wx,wy,rx,ry in [(-35,900,38,76),(26,832,26,40),(-28,1040,24,55)]:
                    d=((x-wx)/rx)**2+((y-side*wy)/ry)**2
                    wear=max(wear,(1-smooth(.05,1.8,d))*.75)
            dry=1+.065*broad+.032*grain+.022*math.sin(y*.039+x*.025)
            soil=(.278*dry,.215*dry,.131*dry)
            soil=mix(soil,(.178,.146,.091),wear)
            # Sparse remaining grass at the margins breaks the pale board outline.
            coverage=smooth(106,148,abs(x))*(.24+.10*math.sin(y*.15))
            soil=mix(soil,(.13,.178,.063),coverage)
            c=mix(c,soil,pitch)
        r=math.sqrt((x/6550)**2+(y/7200)**2)
        c=mix(c,(.075*v,.157*v,.057*v),smooth(.973,.994,r)*.45)
        c=mix(c,(.074,.106,.053),smooth(1.009,1.018,r))
        data.extend(tuple(max(0,min(255,round(q*255))) for q in (c[2],c[1],c[0])))
folder=os.path.join(project,'ArtSource/Generated/World')
os.makedirs(folder,exist_ok=True)
path=os.path.join(folder,'T_Eclipse_GroundColour.tga')
with open(path,'wb') as f:
    f.write(struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,size,size,24,32))
    f.write(data)
task=u.AssetImportTask()
task.filename=path;task.destination_path=ROOT+'/Outfield';task.destination_name='T_Eclipse_GroundColour'
task.automated=True;task.replace_existing=True;task.save=True
assets.import_asset_tasks([task])
tex=lib.load_asset(ROOT+'/Outfield/T_Eclipse_GroundColour')
assert tex
tex.set_editor_property('srgb',False)
tex.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_WORLD)
tex.set_editor_property('address_x',u.TextureAddress.TA_CLAMP)
tex.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
lib.save_loaded_asset(tex)

m=material('M_Eclipse_PlayingSurface')
world=node(m,u.MaterialExpressionWorldPosition)
xy=node(m,u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False)
link(world,xy,'')
scale=node(m,u.MaterialExpressionConstant2Vector,r=1/18000,g=1/18800)
mul=node(m,u.MaterialExpressionMultiply);link(xy,mul,'A');link(scale,mul,'B')
uv=node(m,u.MaterialExpressionAdd,const_b=.5);link(mul,uv,'A')
sample=node(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='GroundColour',texture=tex,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
link(uv,sample,'UVs')
tint=node(m,u.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=u.LinearColor(1,1,1,1))
base=node(m,u.MaterialExpressionMultiply);link(sample,base,'A','RGB');link(tint,base,'B')
result(base,u.MaterialProperty.MP_BASE_COLOR)
detail_uv=node(m,u.MaterialExpressionMultiply,const_b=.008);link(xy,detail_uv,'A')
normal_tex=lib.load_asset('/Game/Cricket26/Textures/T_Grass_Normal')
assert normal_tex
normal=node(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='GrassNormal',texture=normal_tex,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
link(detail_uv,normal,'UVs')
flat=node(m,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(0,0,1,1))
strength=node(m,u.MaterialExpressionScalarParameter,parameter_name='NormalIntensity',default_value=.48)
norm=node(m,u.MaterialExpressionLinearInterpolate);link(flat,norm,'A');link(normal,norm,'B','RGB');link(strength,norm,'Alpha')
result(norm,u.MaterialProperty.MP_NORMAL)
detail=node(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='MicroVariation',texture=lib.load_asset('/Game/Cricket26/Textures/T_Grass_Detail'),sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
link(detail_uv,detail,'UVs')
micro=node(m,u.MaterialExpressionMultiply,const_b=.65);link(detail,micro,'A','R')
lift=node(m,u.MaterialExpressionAdd,const_b=.46);link(micro,lift,'A')
colour=node(m,u.MaterialExpressionMultiply);link(base,colour,'A');link(lift,colour,'B')
result(colour,u.MaterialProperty.MP_BASE_COLOR)
rough=node(m,u.MaterialExpressionScalarParameter,parameter_name='Roughness',default_value=.96)
rough_mul=node(m,u.MaterialExpressionMultiply);link(detail,rough_mul,'A','R');link(rough,rough_mul,'B')
result(rough_mul,u.MaterialProperty.MP_ROUGHNESS)
spec=node(m,u.MaterialExpressionScalarParameter,parameter_name='Specular',default_value=.16)
result(spec,u.MaterialProperty.MP_SPECULAR)
finish(m)

m=material('M_Eclipse_Crowd')
m.set_editor_property('used_with_instanced_static_meshes',True)
vc=node(m,u.MaterialExpressionVertexColor)
random=node(m,u.MaterialExpressionPerInstanceRandom)
ca=node(m,u.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=u.LinearColor(.09,.18,.24,1))
cb=node(m,u.MaterialExpressionVectorParameter,parameter_name='Accent',default_value=u.LinearColor(.48,.39,.26,1))
shirt=node(m,u.MaterialExpressionLinearInterpolate);link(ca,shirt,'A');link(cb,shirt,'B');link(random,shirt,'Alpha')
base=node(m,u.MaterialExpressionLinearInterpolate);link(vc,base,'A','');link(shirt,base,'B');link(vc,base,'Alpha','A')
result(base,u.MaterialProperty.MP_BASE_COLOR)
rough=node(m,u.MaterialExpressionConstant,r=.96);result(rough,u.MaterialProperty.MP_ROUGHNESS)
spec=node(m,u.MaterialExpressionConstant,r=.08);result(spec,u.MaterialProperty.MP_SPECULAR)
# A crowd that never moves is scenery. Every spectator rises on his own beat -- per-instance random
# phases the sine, so twenty thousand of them never pulse in unison -- and the amplitude is driven
# from the match's own reaction level, so a six lifts the ground and a dot ball does not. This is
# vertex motion in the shader on geometry that is already instanced: no ticking actors, no skeletal
# meshes, no per-instance CPU work, which is the only way a crowd this size is affordable on a phone.
excite=node(m,u.MaterialExpressionScalarParameter,parameter_name='Excitement',default_value=0.)
clock=node(m,u.MaterialExpressionTime)
phase=node(m,u.MaterialExpressionMultiply,const_b=6.283);link(random,phase,'A')
beat=node(m,u.MaterialExpressionMultiply,const_b=7.4);link(clock,beat,'A')
sum_=node(m,u.MaterialExpressionAdd);link(beat,sum_,'A');link(phase,sum_,'B')
wave=node(m,u.MaterialExpressionSine);link(sum_,wave,'')
rise=node(m,u.MaterialExpressionSaturate);link(wave,rise,'')
amp=node(m,u.MaterialExpressionMultiply,const_b=29.);link(excite,amp,'A')
idle=node(m,u.MaterialExpressionAdd,const_b=1.2);link(amp,idle,'A')
lift=node(m,u.MaterialExpressionMultiply);link(rise,lift,'A');link(idle,lift,'B')
up=node(m,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(0,0,1,0))
offset=node(m,u.MaterialExpressionMultiply);link(up,offset,'A');link(lift,offset,'B')
result(offset,u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
finish(m)

sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sub, 'Run with -ExecutePythonScript (full editor required for mesh LOD authoring)'
for name,category in [('FloodlightTower','Floodlights'),('LampHousing','Floodlights'),('LampArray','Floodlights'),
                      ('SeatedSpectator','Crowd'),('StadiumSeat','Stadium'),('BoundaryCushion','Boundary')]:
    asset='SM_Eclipse_'+name
    opts=u.FbxImportUI()
    opts.automated_import_should_detect_type=False
    opts.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    opts.import_materials=False;opts.import_textures=False
    opts.static_mesh_import_data.combine_meshes=True
    opts.static_mesh_import_data.auto_generate_collision=False
    opts.static_mesh_import_data.vertex_color_import_option=u.VertexColorImportOption.REPLACE
    task=u.AssetImportTask()
    task.filename=os.path.join(project,'ArtSource/Exports/Stadium',asset+'.fbx')
    task.destination_path=ROOT+'/'+category;task.destination_name=asset
    task.automated=True;task.replace_existing=True;task.save=True;task.options=opts
    assets.import_asset_tasks([task])
    mesh=lib.load_asset(task.destination_path+'/'+asset)
    assert mesh,asset
    settings=u.StaticMeshReductionOptions()
    settings.auto_compute_lod_screen_size=False
    lods=[]
    for percent,screen in [(1.0,1.0),(.55,.15),(.28,.035)]:
        lod=u.StaticMeshReductionSettings()
        lod.percent_triangles=percent;lod.screen_size=screen
        lods.append(lod)
    settings.reduction_settings=lods
    assert sub.set_lods(mesh,settings)==3,asset
    lib.save_loaded_asset(mesh)
    u.log('C26_WORLD_MESH %s bounds=%s lods=%s'%(asset,mesh.get_bounds(),sub.get_lod_count(mesh)))
u.log('C26_WORLD_ASSETS_COMPLETE')
# -ExecutePythonScript exits after the editor tick safely unwinds.
