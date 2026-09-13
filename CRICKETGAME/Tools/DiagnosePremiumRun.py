import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out={}
def t(x):return str(x)
for label,path,clip in [('source','/Game/Cricket26/Characters/SK_Cricketer','/Game/Cricket26/Animations/A_Run'),('target','/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate','/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run')]:
 mesh=u.load_asset(path);anim=u.load_asset(clip);bind=u.C26CharacterProfile.bind_pose(mesh)
 names=[str(n) for n in bind if any(str(n).lower().endswith(x) for x in ['hips','head','leftfoot','leftarm','leftforearm','lefthand']) or str(n) in ['root','pelvis','head','upperarm_l','lowerarm_l','hand_l','foot_l','ball_l']]
 out[label]={'height':mesh.get_bounds().box_extent.z*2,'bind':{n:t(bind[n]) for n in names},'clip':{n:[t(u.AnimationLibrary.get_bone_pose_for_time(anim,n,time,False)) for time in [0,.3]] for n in names},'root_lock':str(anim.get_editor_property('root_motion_root_lock'))}
rt=u.load_asset('/Game/Cricket26/Characters/IK/RTG_C26_RunToFullBody');c=u.IKRetargeterController.get_controller(rt)
out['ops']=[{'name':str(c.get_op_name(i)),'enabled':c.get_retarget_op_enabled(i),'settings':str(c.get_op_controller(i).get_settings())} for i in range(c.get_num_retarget_ops())]
(root/'Artifacts/CharacterAudit/run-diagnosis.json').write_text(json.dumps(out,indent=2))
