"""Review existing sources individually. No approval, generic-shot substitution or match migration."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();lib=u.EditorAssetLibrary;folder='/Game/Cricket26/Characters/IK'
mesh=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate');target=lib.load_asset(folder+'/IK_C26_FullBody');report=[]
for name in ['A_Idle','A_C26_BattingDrive','A_C26_BowlingPace']:
 clip=u.load_asset('/Game/Cricket26/Animations/'+name);assert clip
 source=next((u.load_asset(p) for p in ['/Game/Cricket26/Characters/SK_Cricketer','/Game/Cricket26/Characters/SK_Cricketer_Match'] if u.load_asset(p).get_editor_property('skeleton')==clip.get_editor_property('skeleton')),None);assert source,'Missing exact source skeleton'
 run_source=source.get_name()=='SK_Cricketer';src=lib.load_asset(folder+('/IK_C26_RunSource' if run_source else '/IK_C26_LegacySource'))
 path=folder+('/RTG_C26_RunToFullBody' if run_source else '/RTG_C26_MatchToFullBody')
 rt=lib.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset(folder+'/RTG_C26_RunToFullBody',path)
 c=u.IKRetargeterController.get_controller(rt);c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,src);c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,target)
 c.set_preview_mesh(u.RetargetSourceOrTarget.SOURCE,source);c.set_preview_mesh(u.RetargetSourceOrTarget.TARGET,mesh)
 c.remove_all_ops();c.add_default_ops();c.auto_map_chains(u.AutoMapChainType.EXACT,True)
 mode=u.RetargetSourceOrTarget.TARGET;pose=c.get_current_retarget_pose_name(mode);c.reset_retarget_pose(pose,[],mode)
 c.auto_align_all_bones(mode,u.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
 for chain in ['Root','Pelvis']:c.set_source_chain('',chain)
 c.reset_retarget_pose(pose,['root','pelvis'],mode)
 for i in range(c.get_num_retarget_ops()):
  if str(c.get_op_name(i))=='Root Motion':
   op=c.get_op_controller(i);s=op.get_settings();s.root_motion_source=u.RootMotionSource.GENERATE_FROM_TARGET_PELVIS;s.root_height_source=u.RootMotionHeightSource.SNAP_TO_GROUND;s.rotate_with_pelvis=False;op.set_settings(s)
 lib.save_loaded_asset(rt,only_if_is_dirty=False)
 a=u.IKRetargetBatchOperationInputs();a.assets_to_retarget=[u.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(clip.get_path_name())];a.source_mesh=source;a.target_mesh=mesh;a.ik_retarget_asset=rt;a.prefix='Review_';a.target_path='/Game/Cricket26/Characters/Animations/Review';a.overwrite_existing_files=True;a.include_referenced_assets=False
 outputs=u.IKRetargetBatchOperation.run_batch_retarget(a);assert outputs
 result=outputs[0].get_asset();result.set_editor_property('enable_root_motion',False);result.set_editor_property('force_root_lock',True);lib.save_loaded_asset(result,only_if_is_dirty=False)
 report.append({'source':clip.get_path_name(),'review':result.get_path_name(),'duration':result.get_play_length(),'approved':False})
(root/'Artifacts/CharacterAudit/review-clips.json').write_text(json.dumps(report,indent=2));u.log('C26_REVIEW_CLIPS '+str(report))
