"""One-clip quality gate: import/inspect the new body, align rigs, retarget one run, create review level.
Never marks a profile ApprovedForMatch and never mass-retargets unreviewed sports clips.
"""
import unreal as u,json,runpy
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Tools/ImportPremiumBody.py'),run_name='__main__')
mesh=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate');assert mesh
errors=u.C26CharacterProfile.inspect_body(mesh);assert not errors,'Body validation: '+str(errors)
run=u.load_asset('/Game/Cricket26/Animations/A_Run');assert run
source=None
for path in u.EditorAssetLibrary.list_assets('/Game/Cricket26/Characters',recursive=False):
 obj=u.load_asset(path)
 if isinstance(obj,u.SkeletalMesh) and obj.get_editor_property('skeleton')==run.get_editor_property('skeleton'):
  source=obj;break
assert source,'No mesh matching A_Run skeleton; do not force raw bone copies'
folder='/Game/Cricket26/Characters/IK';lib=u.EditorAssetLibrary
src=lib.load_asset(folder+'/IK_C26_RunSource') if lib.does_asset_exist(folder+'/IK_C26_RunSource') else lib.duplicate_asset(folder+'/IK_C26_LegacySource',folder+'/IK_C26_RunSource')
tgt=lib.load_asset(folder+'/IK_C26_FullBody') if lib.does_asset_exist(folder+'/IK_C26_FullBody') else lib.duplicate_asset(folder+'/IK_C26_MetaHuman',folder+'/IK_C26_FullBody')
u.IKRigController.get_controller(src).set_skeletal_mesh(source);u.IKRigController.get_controller(tgt).set_skeletal_mesh(mesh)
for obj in [src,tgt]:lib.save_loaded_asset(obj)
path=folder+'/RTG_C26_RunToFullBody'
rt=lib.load_asset(path) if lib.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('RTG_C26_RunToFullBody',folder,u.IKRetargeter,u.IKRetargetFactory())
c=u.IKRetargeterController.get_controller(rt);c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,src);c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,tgt)
c.set_preview_mesh(u.RetargetSourceOrTarget.SOURCE,source);c.set_preview_mesh(u.RetargetSourceOrTarget.TARGET,mesh)
c.remove_all_ops();c.add_default_ops();c.auto_map_chains(u.AutoMapChainType.EXACT,True)
c.auto_align_all_bones(u.RetargetSourceOrTarget.TARGET,u.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
# Pelvis Motion owns pelvis; the ground root is generated, never copied from source Hips.
for chain in ['Root','Pelvis']:c.set_source_chain('',chain)
pose=c.get_current_retarget_pose_name(u.RetargetSourceOrTarget.TARGET)
c.reset_retarget_pose(pose,['root','pelvis'],u.RetargetSourceOrTarget.TARGET)
for i in range(c.get_num_retarget_ops()):
 name=str(c.get_op_name(i));op=c.get_op_controller(i)
 if name=='Root Motion':
  s=op.get_settings();s.root_motion_source=u.RootMotionSource.GENERATE_FROM_TARGET_PELVIS;s.root_height_source=u.RootMotionHeightSource.SNAP_TO_GROUND;s.rotate_with_pelvis=False;op.set_settings(s)
lib.save_loaded_asset(rt)
args=u.IKRetargetBatchOperationInputs();args.assets_to_retarget=[u.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(run.get_path_name())]
args.source_mesh=source;args.target_mesh=mesh;args.ik_retarget_asset=rt;args.prefix='C26_';args.target_path='/Game/Cricket26/Characters/Animations/Locomotion';args.overwrite_existing_files=True;args.include_referenced_assets=False
out=u.IKRetargetBatchOperation.run_batch_retarget(args);assert out,'Run retarget failed'
clip=out[0].get_asset();clip.set_editor_property('enable_root_motion',False);clip.set_editor_property('force_root_lock',True);lib.save_loaded_asset(clip)
report={'body':mesh.get_path_name(),'source':source.get_path_name(),'run':clip.get_path_name(),'height_cm':mesh.get_bounds().box_extent.z*2,'structural_errors':list(errors),'approved_for_match':False,'retarget_pose':'engine chain alignment; visual review pending'}
(root/'Artifacts/CharacterAudit/locomotion-slice.json').write_text(json.dumps(report,indent=2))
# Neutral, one-character review level. Real match acceptance follows this gate.
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);level='/Game/Cricket26/Characters/Debug/L_C26_CharacterReview'
assert levels.new_level(level),'Create review level failed'
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
a=actors.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,3));a.set_actor_label('C26_FullBody_Locomotion_Review')
body=a.get_component_by_class(u.SkeletalMeshComponent);body.set_skeletal_mesh(mesh)
body.set_editor_property('animation_mode',u.AnimationMode.ANIMATION_SINGLE_NODE)
data=body.get_editor_property('animation_data');data.set_editor_property('anim_to_play',clip);data.set_editor_property('saved_looping',True);data.set_editor_property('saved_playing',True);body.set_editor_property('animation_data',data)
body.set_editor_property('cast_shadow',True)
ground=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-5));g=ground.get_component_by_class(u.StaticMeshComponent);g.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'));ground.set_actor_scale3d(u.Vector(15,15,.1))
g.set_material(0,u.load_asset('/Game/Cricket26/Materials/M_Surface'))
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,400),u.Rotator(pitch=-45,yaw=-35,roll=0));light.get_component_by_class(u.DirectionalLightComponent).set_intensity(4)
sky=actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,300));sky.get_component_by_class(u.SkyLightComponent).set_intensity(1)
# Default imported athlete orientation is verified below via reference-pose foot direction.
pose=u.C26CharacterProfile.bind_pose(mesh);lf=pose['foot_l'].translation;toe=pose['ball_l'].translation
forward=u.Vector(toe.x-lf.x,toe.y-lf.y,0);forward.normalize();eye=forward*550+u.Vector(0,0,130)
camera=actors.spawn_actor_from_class(u.CameraActor,eye,u.MathLibrary.find_look_at_rotation(eye,u.Vector(0,0,100)))
camera.get_component_by_class(u.CameraComponent).set_editor_property('constrain_aspect_ratio',False);camera.set_editor_property('auto_activate_for_player',u.AutoReceiveInput.PLAYER0);camera.get_component_by_class(u.CameraComponent).set_field_of_view(42)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',u.C26CharacterReviewMode)
assert levels.save_current_level();u.log('C26_LOCOMOTION_SLICE '+str(report))
