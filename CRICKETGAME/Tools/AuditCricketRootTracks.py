import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=[]
for n,m in [('A_Run','SK_Cricketer'),('A_C26_BattingDrive','SK_Cricketer_Match'),('A_C26_BowlingPace','SK_Cricketer_Match')]:
 mesh=u.load_asset('/Game/Cricket26/Characters/'+m);clip=u.load_asset('/Game/Cricket26/Animations/'+n);pose=u.C26CharacterProfile.bind_pose(mesh);bone=next(str(x) for x in pose if str(x).lower().endswith('hips'))
 out.append({'clip':n,'root':bone,'bind':str(pose[bone]),'keys':[str(u.AnimationLibrary.get_bone_pose_for_time(clip,bone,t,False)) for t in [0,.2,.6]],'controller_api':[x for x in dir(clip) if 'controller' in x or 'data_model' in x]})
(root/'Artifacts/CharacterAudit/cricket-root-tracks.json').write_text(json.dumps(out,indent=2));u.log('C26_CRICKET_ROOTS '+str(out))
