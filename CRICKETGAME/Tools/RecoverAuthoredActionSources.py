"""Recover exact source bind skeletons from combined mesh+motion FBX; editor reference only."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();lib=u.EditorAssetLibrary;assets=u.AssetToolsHelpers.get_asset_tools();records=[]
target=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate');target_rig=u.load_asset('/Game/Cricket26/Characters/IK/IK_C26_FullBody')
for name in ['A_C26_BattingDrive','A_C26_BowlingPace']:
 folder='/Game/Cricket26/Characters/Debug/SourceRigs/'+name
 opt=u.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;opt.import_as_skeletal=True;opt.import_mesh=True;opt.import_animations=True;opt.import_materials=False;opt.import_textures=False
 task=u.AssetImportTask();task.filename=str(root/'ArtSource/Premium/AnimationSources'/f'{name}_Reference.fbx');task.destination_path=folder;task.destination_name='Source_'+name;task.automated=True;task.save=True;task.replace_existing=True;task.options=opt
 assets.import_asset_tasks([task]);objects=[u.load_asset(p) for p in lib.list_assets(folder)]
 mesh=next(o for o in objects if isinstance(o,u.SkeletalMesh));clip=next(o for o in objects if isinstance(o,u.AnimSequence));assert mesh.get_editor_property('skeleton')==clip.get_editor_property('skeleton')
 rigpath=folder+'/IK_Source';rig=lib.load_asset(rigpath) if lib.does_asset_exist(rigpath) else lib.duplicate_asset('/Game/Cricket26/Characters/IK/IK_C26_LegacySource',rigpath)
 r=u.IKRigController.get_controller(rig);r.set_skeletal_mesh(mesh);bones=[str(k) for k in u.C26CharacterProfile.bind_pose(mesh)]
 def bone(old):
  clean=str(old).split(':')[-1].removeprefix('mixamorig_').lower()
  return next(x for x in bones if x.split(':')[-1].removeprefix('mixamorig_').lower()==clean)
 for ch in r.get_retarget_chains():r.set_retarget_chain_start_bone(ch.chain_name,bone(ch.start_bone));r.set_retarget_chain_end_bone(ch.chain_name,bone(ch.end_bone))
 r.set_retarget_root(bone('Hips'));lib.save_loaded_asset(rig,only_if_is_dirty=False)
 path=folder+'/RTG_SourceToFullBody';rt=lib.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset('/Game/Cricket26/Characters/IK/RTG_C26_RunToFullBody',path)
 c=u.IKRetargeterController.get_controller(rt);c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,rig);c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,target_rig);c.set_preview_mesh(u.RetargetSourceOrTarget.SOURCE,mesh);c.set_preview_mesh(u.RetargetSourceOrTarget.TARGET,target)
 c.remove_all_ops();c.add_default_ops();c.auto_map_chains(u.AutoMapChainType.EXACT,True)
 mode=u.RetargetSourceOrTarget.TARGET;pose=c.get_current_retarget_pose_name(mode);c.reset_retarget_pose(pose,[],mode);c.auto_align_all_bones(mode,u.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
 for chain in ['Root','Pelvis']:c.set_source_chain('',chain)
 c.reset_retarget_pose(pose,['root','pelvis'],mode)
 for i in range(c.get_num_retarget_ops()):
  if str(c.get_op_name(i))=='Root Motion':
   op=c.get_op_controller(i);s=op.get_settings();s.root_motion_source=u.RootMotionSource.GENERATE_FROM_TARGET_PELVIS;s.root_height_source=u.RootMotionHeightSource.SNAP_TO_GROUND;s.rotate_with_pelvis=False;op.set_settings(s)
 lib.save_loaded_asset(rt,only_if_is_dirty=False)
 a=u.IKRetargetBatchOperationInputs();a.assets_to_retarget=[u.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(clip.get_path_name())];a.source_mesh=mesh;a.target_mesh=target;a.ik_retarget_asset=rt;a.target_path='/Game/Cricket26/Characters/Animations/Review/Recovered';a.overwrite_existing_files=True;a.include_referenced_assets=False
 output=u.IKRetargetBatchOperation.run_batch_retarget(a);assert output
 result=output[0].get_asset();result.set_editor_property('enable_root_motion',False);result.set_editor_property('force_root_lock',True);lib.save_loaded_asset(result,only_if_is_dirty=False)
 records.append({'source_mesh':mesh.get_path_name(),'source_clip':clip.get_path_name(),'result':result.get_path_name(),'approved':False})
(root/'Artifacts/CharacterAudit/recovered-actions.json').write_text(json.dumps(records,indent=2));u.log('C26_RECOVERED_ACTIONS '+str(records))
