import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();file=root/'ArtSource/Premium/Foundation/manifest.json';j=json.loads(file.read_text())
for m in j['meshes']:
 m['export_material_map']={str(k):v for k,v in u.C26CharacterProfile.export_material_map(u.load_asset(m['path']),0).items()}
 u.log('C26_EXPORT_MATERIAL_MAP '+m['label']+' '+str(m['export_material_map']))
file.write_text(json.dumps(j,indent=2))
