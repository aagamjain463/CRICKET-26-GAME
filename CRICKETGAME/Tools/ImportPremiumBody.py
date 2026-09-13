"""Import the original full-body candidate at real scale, preserve UV-correct materials.
Creates a new canonical skeleton; does not overwrite or activate existing match actors.
"""
import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();folder='/Game/Cricket26/Characters/Bodies';name='SK_C26_FullBody_Candidate'
o=u.FbxImportUI();o.automated_import_should_detect_type=False;o.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
o.import_as_skeletal=True;o.import_animations=False;o.import_materials=False;o.import_textures=False
sk=o.skeletal_mesh_import_data
for key,value in [('import_morph_targets',False),('update_skeleton_reference_pose',True),('use_t0_as_ref_pose',False),('preserve_smoothing_groups',True)]:sk.set_editor_property(key,value)
sk.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
sk.set_editor_property('import_rotation',u.Rotator(pitch=0,yaw=0,roll=0))
t=u.AssetImportTask();t.filename=str(root/'ArtSource/Premium/FullBody'/f'{name}.fbx');t.destination_path=folder;t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True;t.options=o
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);mesh=u.load_asset(folder+'/'+name);assert mesh
import runpy
runpy.run_path(str(root/'Tools/ApplyPremiumBodyMaterials.py'),run_name='__main__')
