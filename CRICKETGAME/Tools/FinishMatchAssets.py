"""Import original Foley and build mobile LODs on the final athlete and equipment."""
import unreal as u
from pathlib import Path
root=Path(u.Paths.project_dir());lib=u.EditorAssetLibrary
for name in ['runup_step','ball_release','final_ball_pulse']:
    t=u.AssetImportTask();t.filename=str(root/'ArtSource/Generated/Audio'/(name+'.wav'));t.destination_path='/Game/Cricket26/Audio';t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    a=lib.load_asset('/Game/Cricket26/Audio/'+name);assert a;u.log('C26_AUDIO '+name)
# Editor modules are not loaded by a headless Python commandlet by default.
for mod in ['SkeletalMeshEditor','StaticMeshEditor']:
    try:u.load_module(mod)
    except Exception as e:u.log_warning(str(e))
mesh=lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer_Match')
try:
    cls=getattr(u,'SkeletalMeshEditorSubsystem',None)
    ok=cls.regenerate_lod(mesh,3,False,False) if cls else u.EditorSkeletalMeshLibrary.regenerate_lod(mesh,3,False,False)
    u.log('C26_ATHLETE_LOD result=%s count=%s'%(ok,cls.get_lod_count(mesh) if cls else u.EditorSkeletalMeshLibrary.get_lod_count(mesh)))
    lib.save_loaded_asset(mesh)
except Exception as e:u.log_warning('C26_ATHLETE_LOD '+str(e))
try:
    cls=getattr(u,'StaticMeshEditorSubsystem',None)
    sms=u.get_editor_subsystem(cls) if cls else None
    for path in lib.list_assets('/Game/Cricket26/Equipment'):
        a=lib.load_asset(path)
        if not isinstance(a,u.StaticMesh):continue
        settings=u.StaticMeshReductionOptions(auto_compute_lod_screen_size=True,reduction_settings=[u.StaticMeshReductionSettings(percent_triangles=1.0),u.StaticMeshReductionSettings(percent_triangles=.42),u.StaticMeshReductionSettings(percent_triangles=.16)])
        count=sms.set_lods(a,settings) if sms else u.EditorStaticMeshLibrary.set_lods(a,settings)
        u.log('C26_KIT_LOD '+a.get_name()+' count='+str(count));lib.save_loaded_asset(a)
except Exception as e:u.log_warning('C26_KIT_LOD '+str(e))
