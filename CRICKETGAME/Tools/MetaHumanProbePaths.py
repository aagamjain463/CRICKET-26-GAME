"""Find the working way to reach the MetaHuman build-pipeline Blueprint from Python."""
import unreal as u

PKG = '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Low'
OBJ = PKG + '.BP_DefaultUEFNPipeline_Low'
CLS = PKG + '.BP_DefaultUEFNPipeline_Low_C'

u.log('C26_MHP exists  = %s' % u.EditorAssetLibrary.does_asset_exist(PKG))
try:
    u.log('C26_MHP list    = %s' % u.EditorAssetLibrary.list_assets('/MetaHumanCharacter/BuildPipeline', recursive=False, include_folder=False)[:6])
except Exception as exc:                                       # noqa: BLE001
    u.log('C26_MHP list failed: %s' % exc)

for label, fn in [
    ('load_asset(PKG)', lambda: u.EditorAssetLibrary.load_asset(PKG)),
    ('load_asset(OBJ)', lambda: u.EditorAssetLibrary.load_asset(OBJ)),
    ('load_asset(CLS)', lambda: u.EditorAssetLibrary.load_asset(CLS)),
    ('load_object(OBJ)', lambda: u.load_object(None, OBJ)),
    ('load_object(CLS)', lambda: u.load_object(None, CLS)),
]:
    try:
        u.log('C26_MHP %-18s -> %s' % (label, fn()))
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHP %-18s !! %s' % (label, exc))

# If we can reach the class at all, we can read the flag off its CDO.
try:
    cls = u.load_object(None, CLS)
    if cls:
        cdo = u.get_default_object(cls)
        u.log('C26_MHP cdo=%s bBakeMaterials=%s' % (cdo, cdo.get_editor_property('bBakeMaterials')))
except Exception as exc:                                       # noqa: BLE001
    u.log('C26_MHP cdo read failed: %s' % exc)

u.log('C26_MHP_DONE')
