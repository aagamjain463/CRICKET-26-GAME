"""Explicitly persist each skeletal material slot; Unreal struct-array iteration returns copies."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
mesh=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate');assert mesh
manifest=json.loads((root/'ArtSource/Premium/Foundation/manifest.json').read_text());materials={}
for part in manifest['meshes']:
 for _,p in part['materials']:
  if p:materials[p.rsplit('.',1)[-1]]=p
cloth=u.load_asset('/Game/Cricket26/Materials/M_C26_Cloth');assets=u.AssetToolsHelpers.get_asset_tools()
for slot,color in [('Jersey',u.LinearColor(.035,.16,.29,1)),('Trousers',u.LinearColor(.028,.07,.13,1))]:
 path='/Game/Cricket26/Characters/Materials/MI_C26_'+slot
 mi=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('MI_C26_'+slot,'/Game/Cricket26/Characters/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
 u.MaterialEditingLibrary.set_material_instance_parent(mi,cloth);u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi,'Tint',color)
 for key,value in [('NormalTiling',24.),('NormalStrength',.12),('Detail',.06)]:u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi,key,value)
 u.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False);materials[slot]=mi.get_path_name()
# Shoe FBX slots have no saved materials: create explicit variants of the project's fabric master.
for label,color in [('Upper',(.79,.80,.78)),('Sole',(.13,.136,.148)),('Flash',(.04,.21,.35)),('Lace',(.86,.86,.84)),('Spike',(.34,.36,.39))]:
 name='MI_C26_Shoe'+label;path='/Game/Cricket26/Characters/Materials/'+name
 mi=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,'/Game/Cricket26/Characters/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
 u.MaterialEditingLibrary.set_material_instance_parent(mi,cloth)
 u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*color,1))
 for key,value in [('NormalStrength',0.),('Detail',0.)]:u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi,key,value)
 u.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False);materials['M_C26_Shoe'+label]=mi.get_path_name()
# The assembled face has no baked albedo. Use an explicit original skin-pigment material
# with its UV-correct MetaHuman normal map until the high-detail albedo is available.
name='MI_C26_FaceGameplay';path='/Game/Cricket26/Characters/Materials/'+name
mi=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,'/Game/Cricket26/Characters/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
u.MaterialEditingLibrary.set_material_instance_parent(mi,u.load_asset('/Game/Cricket26/Materials/M_C26_PlayerSkin'))
u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(.38,.22,.145,1))
for key,value in [('NormalTiling',1.),('NormalStrength',.6),('Detail',0.)]:u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi,key,value)
normal=u.load_asset('/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001/Face/Textures/T_Face_Normal')
u.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi,'Weave',normal)
u.EditorAssetLibrary.save_loaded_asset(mi,only_if_is_dirty=False);materials['MI_Face_Skin_LOD1']=mi.get_path_name()
# Material import names may include FBX suffixes, but each is matched by source material name.
mesh.modify();slots=list(mesh.get_editor_property('materials'));unmapped=[]
for i,slot in enumerate(slots):
 label=str(slot.material_slot_name)
 path=materials.get(label)
 if not path:
  for key,p in materials.items():
   if label.startswith(key):path=p;break
 if path:slot.set_editor_property('material_interface',u.load_asset(path))
 else:unmapped.append(label)
 slots[i]=slot
mesh.set_editor_property('materials',slots);u.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
height=mesh.get_bounds().box_extent.z*2
assert 150<height<210,'Wrong import scale '+str(height)
comp=u.new_object(u.SkeletalMeshComponent);comp.set_skeletal_mesh(mesh)
report={'mesh':mesh.get_path_name(),'height_cm':height,'skeleton':mesh.get_editor_property('skeleton').get_path_name(),'bones':[str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())],'unmapped_materials':unmapped,'materials':[x.material_interface.get_path_name() if x.material_interface else '' for x in mesh.get_editor_property('materials')],'approved_for_match':False}
(root/'Artifacts/CharacterAudit/imported-body.json').write_text(json.dumps(report,indent=2));u.log('C26_BODY_IMPORTED '+str({k:v for k,v in report.items() if k!='bones'}))
