"""Generate real reduced LODs and validate every anatomical region before saving."""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();mesh=u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
s=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem);mesh.modify()
assert s.regenerate_lod(mesh,4,False,False),'Native skeletal LOD reduction failed'
errors=u.C26CharacterProfile.inspect_body(mesh)
report={'lods':[{'lod':i,'vertices':s.get_num_verts(mesh,i),'sections':s.get_num_sections(mesh,i)} for i in range(s.get_lod_count(mesh))],'errors':list(errors),'mobile_profiled':False}
(root/'Artifacts/CharacterAudit/body-lods.json').write_text(json.dumps(report,indent=2));assert not errors,str(errors)
u.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False);u.log('C26_LOD_AUDIT '+str(report))
