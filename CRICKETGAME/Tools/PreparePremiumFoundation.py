"""Export assembled MetaHuman for full-body clothing work; build explicit retarget chains.
Only creates assets under Characters/IK and exports under ArtSource/Premium.
No automatic match approval or mass retarget of unreviewed sports motion.
"""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();dest=root/'ArtSource/Premium/Foundation';dest.mkdir(parents=True,exist_ok=True)
base='/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001'
body=u.load_asset(base+'/Body/SKM_MH_C26_Player_001_BodyMesh');face=u.load_asset(base+'/Face/SKM_MH_C26_Player_001_FaceMesh')
assert body and face,'Assemble and SAVE both parts first'
report={'meshes':[]}
for label,mesh in [('Body',body),('Face',face)]:
 task=u.AssetExportTask();task.object=mesh;task.filename=str(dest/(label+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True
 task.exporter=u.SkeletalMeshExporterFBX();task.options=u.FbxExportOption();task.options.ascii=False;task.options.level_of_detail=False;task.options.collision=False
 assert u.Exporter.run_asset_export_task(task),'FBX export failed '+label
 comp=u.new_object(u.SkeletalMeshComponent);comp.set_skeletal_mesh(mesh)
 report['meshes'].append({'label':label,'path':mesh.get_path_name(),'skeleton':mesh.get_editor_property('skeleton').get_path_name(),'height_cm':mesh.get_bounds().box_extent.z*2,'bones':[str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())],'lods':u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem).get_lod_count(mesh),'materials':[(str(m.material_slot_name),m.material_interface.get_path_name() if m.material_interface else '') for m in mesh.get_editor_property('materials')]})
# Record this before rig creation; an API failure never loses successful audit/export results.
(dest/'manifest.json').write_text(json.dumps(report,indent=2))
source=u.load_asset('/Game/Cricket26/Characters/SK_Cricketer_Match')
assets=u.AssetToolsHelpers.get_asset_tools();folder='/Game/Cricket26/Characters/IK'
def create(name,cls,factory):
 return u.load_asset(folder+'/'+name) if u.EditorAssetLibrary.does_asset_exist(folder+'/'+name) else assets.create_asset(name,folder,cls,factory)
def rig(name,mesh,mixamo):
 obj=create(name,u.IKRigDefinition,u.IKRigDefinitionFactory());c=u.IKRigController.get_controller(obj);assert c.set_skeletal_mesh(mesh)
 comp=u.new_object(u.SkeletalMeshComponent);comp.set_skeletal_mesh(mesh);bones={str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())}
 def bone(n):
  if not mixamo:return n
  for prefix in ['mixamorig:','mixamorig_','']:
   if prefix+n in bones:return prefix+n
  raise AssertionError('Missing source bone '+n)
 chains=[('Root','Hips','Hips','root','root'),('Pelvis','Hips','Hips','pelvis','pelvis'),
 ('Spine','Spine','Spine2','spine_01','spine_05'),('Neck','Neck','Neck','neck_01','neck_02'),('Head','Head','Head','head','head')]
 for side,suffix in [('Left','l'),('Right','r')]:
  chains += [(side+'Arm',side+'Arm',side+'Hand','upperarm_'+suffix,'hand_'+suffix),
   (side+'Hand',side+'Hand',side+'Hand','hand_'+suffix,'hand_'+suffix),
   (side+'Leg',side+'UpLeg',side+'Foot','thigh_'+suffix,'foot_'+suffix),
   (side+'Foot',side+'Foot',side+'ToeBase','foot_'+suffix,'ball_'+suffix)]
  for finger in ['Thumb','Index','Middle','Ring','Pinky']:
   chains.append((side+finger,side+'Hand'+finger+'1',side+'Hand'+finger+'3',finger.lower()+'_01_'+suffix,finger.lower()+'_03_'+suffix))
 existing={str(ch.chain_name) for ch in c.get_retarget_chains()}
 for name,a,b,x,y in chains:
  start,end=bone(a) if mixamo else x,bone(b) if mixamo else y
  assert start in bones and end in bones,(name,start,end)
  if name not in existing:c.add_retarget_chain(name,start,end,'')
 assert c.set_retarget_root(bone('Hips') if mixamo else 'pelvis')
 u.EditorAssetLibrary.save_loaded_asset(obj);return obj
src=rig('IK_C26_LegacySource',source,True);target=rig('IK_C26_MetaHuman',body,False)
rt=create('RTG_C26_LegacyToMetaHuman',u.IKRetargeter,u.IKRetargetFactory())
c=u.IKRetargeterController.get_controller(rt)
c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,src);c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,target)
c.set_preview_mesh(u.RetargetSourceOrTarget.SOURCE,source);c.set_preview_mesh(u.RetargetSourceOrTarget.TARGET,body)
c.add_default_ops();c.auto_map_chains(u.AutoMapChainType.EXACT,True)
u.EditorAssetLibrary.save_loaded_asset(rt)
u.log('C26_FOUNDATION_PREPARED: explicit chains; retarget pose still requires visual review before bulk processing')
