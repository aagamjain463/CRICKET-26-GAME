"""Fix root/pelvis ownership for a Mixamo pelvis-root source, then recheck one run."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();lib=u.EditorAssetLibrary
rt=u.load_asset('/Game/Cricket26/Characters/IK/RTG_C26_RunToFullBody');c=u.IKRetargeterController.get_controller(rt)
source=u.load_asset('/Game/Cricket26/Characters/SK_Cricketer');target=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
# Pelvis Motion owns pelvis; the ground root is generated, never copied from source Hips.
for chain in ['Root','Pelvis']:c.set_source_chain('',chain)
pose=c.get_current_retarget_pose_name(u.RetargetSourceOrTarget.TARGET)
c.reset_retarget_pose(pose,['root','pelvis'],u.RetargetSourceOrTarget.TARGET)
for i in range(c.get_num_retarget_ops()):
 name=str(c.get_op_name(i));op=c.get_op_controller(i)
 if name=='Root Motion':
  s=op.get_settings();s.root_motion_source=u.RootMotionSource.GENERATE_FROM_TARGET_PELVIS;s.root_height_source=u.RootMotionHeightSource.SNAP_TO_GROUND;s.rotate_with_pelvis=False;op.set_settings(s)
lib.save_loaded_asset(rt)
a=u.IKRetargetBatchOperationInputs();a.assets_to_retarget=[u.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path('/Game/Cricket26/Animations/A_Run.A_Run')];a.source_mesh=source;a.target_mesh=target;a.ik_retarget_asset=rt;a.prefix='C26_';a.target_path='/Game/Cricket26/Characters/Animations/Locomotion';a.overwrite_existing_files=True;a.include_referenced_assets=False
out=u.IKRetargetBatchOperation.run_batch_retarget(a);assert out
clip=out[0].get_asset();clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True);lib.save_loaded_asset(clip)
# Persist review mode; the game controller must not replace the review camera with a default pawn.
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Cricket26/Characters/Debug/L_C26_CharacterReview')
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();w.get_world_settings().set_editor_property('default_game_mode',u.C26CharacterReviewMode);levels.save_current_level()
u.log('C26_RUN_REFINED root generated from pelvis on ground; reference root locked for in-place evaluation')
