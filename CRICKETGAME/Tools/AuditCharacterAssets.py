"""Read-only Unreal registry/LOD audit. No asset saves."""
import unreal as u,json
from pathlib import Path
out={'meshes':[],'animations':[],'metahuman':[]}
s=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for path in u.EditorAssetLibrary.list_assets('/Game/Cricket26/Characters',recursive=True):
 a=u.load_asset(path)
 if isinstance(a,u.SkeletalMesh):
  c=u.new_object(u.SkeletalMeshComponent);c.set_skeletal_mesh(a)
  lods=range(s.get_lod_count(a))
  out['meshes'].append({'path':path,'skeleton':str(a.get_editor_property('skeleton')),'bones':c.get_num_bones(),'bounds':str(a.get_bounds()),'lods':[{'index':i,'vertices':s.get_num_verts(a,i),'sections':s.get_num_sections(a,i),'geometry_review':'requires native AuditBody and visual LOD review'} for i,l in enumerate(lods)],'materials':[str(m.material_slot_name) for m in a.get_editor_property('materials')]})
 elif a and 'MetaHumanCharacter' in a.get_class().get_name():out['metahuman'].append({'path':path,'class':a.get_class().get_name()})
for path in u.EditorAssetLibrary.list_assets('/Game/Cricket26/Animations',recursive=True):
 a=u.load_asset(path)
 if isinstance(a,u.AnimSequence):out['animations'].append({'path':path,'skeleton':str(a.get_editor_property('skeleton')),'duration':a.get_play_length(),'root_motion':a.get_editor_property('enable_root_motion'),'notifies':[str(n.notify_name) for n in a.get_editor_property('notifies')]})
p=Path(u.Paths.project_dir())/'Artifacts/CharacterAudit/unreal-assets.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(out,indent=2));u.log('C26_ASSET_AUDIT '+str(p))
