import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();mesh=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate');out=[]
for s in mesh.get_editor_property('materials'):
 out.append({'slot':str(s.material_slot_name),'material':s.material_interface.get_path_name() if s.material_interface else None})
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Cricket26/Characters/Debug/L_C26_CharacterReview')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for a in actors.get_all_level_actors():
 if isinstance(a,u.SkeletalMeshActor):out.append({'overrides':str(a.get_component_by_class(u.SkeletalMeshComponent).get_editor_property('override_materials'))})
 if isinstance(a,u.CameraActor):
  cam=a.get_component_by_class(u.CameraComponent);cam.set_editor_property('constrain_aspect_ratio',False);cam.set_field_of_view(42)
  eye=a.get_actor_location()*1.3;eye.z=130;a.set_actor_location(eye,False,False);a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,u.Vector(0,0,95)),False)
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(100,-100,250),u.Rotator(pitch=-30,yaw=145,roll=0));comp=light.get_component_by_class(u.DirectionalLightComponent);comp.set_intensity(2);comp.set_cast_shadows(False)
levels.save_current_level()
(root/'Artifacts/CharacterAudit/material-slots.json').write_text(json.dumps(out,indent=2));u.log('C26_MATERIAL_AUDIT '+str(out))
