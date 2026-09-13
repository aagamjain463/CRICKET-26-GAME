"""Assemble existing original MetaHuman and persist generated project packages.
Does not approve match migration. Run in UnrealEditor-Cmd with an absolute script path.
"""
import runpy,json
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Tools/MetaHumanBuildPlayer001.py'),run_name='__main__')
prefix='/Game/Cricket26/Characters/MetaHumans/'
packages=[p for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name().startswith(prefix)]
assert packages,'Assembly generated no dirty project packages'
assert u.EditorLoadingAndSavingUtils.save_packages(packages,only_dirty=True),'Could not save assembled MetaHuman packages'
assets=u.EditorAssetLibrary.list_assets(prefix,recursive=True)
meshes=[p for p in assets if isinstance(u.load_asset(p),u.SkeletalMesh)]
(root/'Artifacts/CharacterAudit/metahuman-assembly.json').write_text(json.dumps({'saved_packages':[p.get_name() for p in packages],'meshes':meshes,'approved_for_match':False},indent=2))
u.log('C26_FOUNDATION_SAVED meshes='+str(meshes))
assert len(meshes)>=2,'Assembly did not persist body AND face'
