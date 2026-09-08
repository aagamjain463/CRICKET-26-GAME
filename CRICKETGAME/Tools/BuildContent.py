"""Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Idempotent: existing imported content is retained. Generated materials are updated.
"""
import unreal as u
import os
import json
import math

ROOT = '/Game/Cricket26'
SOURCE = '/Users/aagamjain/Desktop/CRICKET-26/Assets/_Project'
assets = u.AssetToolsHelpers.get_asset_tools()
lib = u.EditorAssetLibrary
for folder in ['Characters','Animations','Stadium','Equipment','Materials','Audio','Maps','Data','UI']:
    lib.make_directory(ROOT+'/'+folder)

def import_asset(path, dest, name, options=None):
    existing = lib.load_asset(dest+'/'+name) if lib.does_asset_exist(dest+'/'+name) else None
    if existing:
        return existing
    if not os.path.exists(path):
        u.log_warning('C26 source unavailable: '+path)
        return None
    task=u.AssetImportTask()
    task.filename=path;task.destination_path=dest;task.destination_name=name
    task.automated=True;task.save=True;task.replace_existing=False
    if options: task.options=options
    assets.import_asset_tasks([task])
    return lib.load_asset(dest+'/'+name) if lib.does_asset_exist(dest+'/'+name) else None

# Original low-poly crowd volume: 36 triangles, rather than a 960-triangle engine sphere.
generated=os.path.join(u.Paths.project_dir(),'SourceAssets','Generated')
os.makedirs(generated,exist_ok=True)
vertices=[];faces=[]
for ring in range(4):
    latitude=-math.pi/2+.10+ring*(math.pi-.20)/3
    for side in range(6):
        angle=side*2*math.pi/6
        vertices.append((50*math.cos(latitude)*math.cos(angle),50*math.sin(latitude),50*math.cos(latitude)*math.sin(angle)))
for ring in range(3):
    for side in range(6):
        a=ring*6+side;b=ring*6+(side+1)%6
        faces.extend([(a+1,b+1,a+7),(b+1,b+7,a+7)])
obj=os.path.join(generated,'SM_CrowdVolume.obj')
with open(obj,'w') as out:
    out.write('# Original C26 crowd volume\n')
    for v in vertices:out.write('v %.6f %.6f %.6f\n'%v)
    for index,v in enumerate(vertices):out.write('vt %.6f %.6f\n'%((index%6)/6,(index//6)/3))
    for f in faces:out.write('f %d/%d %d/%d %d/%d\n'%(f[0],f[0],f[1],f[1],f[2],f[2]))
staticopts=u.FbxImportUI()
staticopts.set_editor_property('import_materials',False)
staticopts.set_editor_property('import_textures',False)
staticopts.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH)
staticopts.set_editor_property('automated_import_should_detect_type',False)
staticopts.static_mesh_import_data.set_editor_property('combine_meshes',True)
staticopts.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
import_asset(obj,ROOT+'/Stadium','SM_CrowdVolume',staticopts)

opts=u.FbxImportUI()
opts.set_editor_property('automated_import_should_detect_type',False)
opts.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_SKELETAL_MESH)
opts.set_editor_property('import_as_skeletal',True)
opts.set_editor_property('import_animations',False)
opts.set_editor_property('import_materials',True)
opts.set_editor_property('import_textures',True)
opts.skeletal_mesh_import_data.set_editor_property('import_uniform_scale',1.0)
player=import_asset(SOURCE+'/Art/Characters/Base/Cricket26_BasePlayer.fbx',ROOT+'/Characters','SK_Cricketer',opts)
if player:
    skeleton=player.get_editor_property('skeleton')
    for name in ['Idle','Run','FielderThrow','Catch']:
        animopts=u.FbxImportUI()
        animopts.set_editor_property('automated_import_should_detect_type',False)
        animopts.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_ANIMATION)
        animopts.set_editor_property('import_mesh',False)
        animopts.set_editor_property('import_animations',True)
        animopts.set_editor_property('skeleton',skeleton)
        import_asset(SOURCE+'/Art/Animations/Mixamo/Cricket_'+name+'.fbx',ROOT+'/Animations','A_'+name,animopts)

for name in ['bat_sweet_spot','bat_edge','bat_defensive','ball_bounce','stump_hit','crowd_ambience','crowd_four','crowd_six','wicket_roar','ui_button_click','keeper_catch','ui_result_sting','fielder_gather','crowd_anticipation']:
    sound=import_asset(SOURCE+'/Audio/Clips/'+name+'.wav',ROOT+'/Audio',name)
    if sound and name=='crowd_ambience': sound.set_editor_property('looping',True);lib.save_loaded_asset(sound)

ML=u.MaterialEditingLibrary
def material(name,color,roughness=.75,emissive=0.0,textured=False):
    path=ROOT+'/Materials'
    mat=lib.load_asset(path+'/'+name) if lib.does_asset_exist(path+'/'+name) else None
    if not mat: mat=assets.create_asset(name,path,u.Material,u.MaterialFactoryNew())
    ML.delete_all_material_expressions(mat)
    tint=ML.create_material_expression(mat,u.MaterialExpressionVectorParameter,-550,0)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',u.LinearColor(*color,1))
    base=tint
    if textured:
        pos=ML.create_material_expression(mat,u.MaterialExpressionWorldPosition,-950,220)
        scale=ML.create_material_expression(mat,u.MaterialExpressionMultiply,-770,220)
        scale.set_editor_property('const_b',.025)
        ML.connect_material_expressions(pos,'',scale,'A')
        noise=ML.create_material_expression(mat,u.MaterialExpressionNoise,-600,220)
        noise.set_editor_property('quality',1);noise.set_editor_property('levels',2)
        noise.set_editor_property('output_min',.72);noise.set_editor_property('output_max',1.13)
        ML.connect_material_expressions(scale,'',noise,'Position')
        mul=ML.create_material_expression(mat,u.MaterialExpressionMultiply,-260,0)
        ML.connect_material_expressions(tint,'',mul,'A');ML.connect_material_expressions(noise,'',mul,'B')
        base=mul
    ML.connect_material_property(base,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=ML.create_material_expression(mat,u.MaterialExpressionScalarParameter,-300,400)
    rough.set_editor_property('parameter_name','Roughness');rough.set_editor_property('default_value',roughness)
    ML.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    glow=ML.create_material_expression(mat,u.MaterialExpressionScalarParameter,-400,600)
    glow.set_editor_property('parameter_name','Glow');glow.set_editor_property('default_value',emissive)
    multiply=ML.create_material_expression(mat,u.MaterialExpressionMultiply,-100,600)
    ML.connect_material_expressions(tint,'',multiply,'A');ML.connect_material_expressions(glow,'',multiply,'B')
    ML.connect_material_property(multiply,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    if name=='M_Surface': mat.set_editor_property('used_with_skeletal_mesh',True)
    ML.recompile_material(mat);lib.save_loaded_asset(mat)
    return mat

material('M_Surface',(.3,.36,.43))
material('M_Grass',(.055,.145,.063),.94,0,True)
material('M_Pitch',(.48,.405,.275),.93,0,True)
material('M_Light',(.7,.84,1),.45,12)
material('M_Navy',(.012,.027,.049),.7,.08)
material('M_Teal',(.015,.41,.43),.6,.3)
material('M_Coral',(.9,.13,.07),.6,.15)
material('M_White',(.87,.9,.86),.7,.05)
material('M_Willow',(.66,.49,.27),.7,0,True)
crowd=material('M_Crowd',(.08,.2,.24),.95,.15)
# NOTE: no world-position animation chain here. The previous Time/PerInstanceRandom/Sine
# graph failed to compile on Metal (Sine input never connects via the Python API), which
# forced the whole crowd to the default grey material. Celebration is driven through the
# standard Glow param by AC26Stadium::UpdateAtmosphere instead.

if u.load_class(None,'/Script/CRICKETGAME.C26MatchGameMode'):
    level_path=ROOT+'/Maps/L_SuperOver'
    level_system=u.get_editor_subsystem(u.LevelEditorSubsystem)
    if lib.does_asset_exist(level_path): level_system.load_level(level_path)
    else: level_system.new_level(level_path)
    actor_system=u.get_editor_subsystem(u.EditorActorSubsystem)
    actors=actor_system.get_all_level_actors()
    stadium_class=u.load_class(None,'/Script/CRICKETGAME.C26Stadium')
    if not any(a.get_class()==stadium_class for a in actors):
        venue=actor_system.spawn_actor_from_class(stadium_class,u.Vector(0,0,0))
        venue.set_actor_label('Eclipse Oval — CRICKET 26')
    world=level_system.get_current_level().get_outer()
    world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/CRICKETGAME.C26MatchGameMode'))
    level_system.save_current_level()
lib.save_directory(ROOT,only_if_is_dirty=True,recursive=True)
u.log('C26 CONTENT BUILD COMPLETE')
