"""Hunt for any Python-reachable path to the pipeline object that carries bBakeMaterials.

Epic's own tool does:
    UMetaHumanCollectionEditorPipeline* SelectedPipeline = PropertyObject->GetSelectedEditorPipeline();
    FindFProperty<FBoolProperty>(SelectedPipeline->GetClass(), TEXT("bBakeMaterials"))
so the property lives on the object returned by GetSelectedEditorPipeline(), not on the
Blueprint CDO that Python was able to load. This dumps the Python surface of every type in that
chain, plus the character asset, looking for a way in.
"""
import unreal as u

def dump(type_name, label):
    t = getattr(u, type_name, None)
    u.log('C26_MHSEEK ==== %s (%s) ====' % (label, t))
    if t is None:
        return
    try:
        members = [m for m in dir(t) if not m.startswith('__')]
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHSEEK   dir failed: %s' % exc)
        return
    keep = [m for m in members if any(k in m.lower() for k in
            ('pipeline', 'bake', 'material', 'property', 'selected', 'restore', 'build'))]
    u.log('C26_MHSEEK   relevant members: %s' % (keep or members[:20]))


for name in ['MetaHumanCollectionEditorPipeline', 'MetaHumanCharacterEditorPipelineToolProperties',
             'MetaHumanCharacterEditorPipeline', 'MetaHumanDefaultPipeline',
             'MetaHumanDefaultPipelineBase', 'MetaHumanBuildInputBase']:
    dump(name, name)

# The character asset itself.
char = u.EditorAssetLibrary.load_asset(
    '/Game/Cricket26/Characters/MetaHumans/Players/Player_001/MH_C26_Player_001')
u.log('C26_MHSEEK character = %s' % char)
if char:
    members = [m for m in dir(char) if not m.startswith('_')]
    keep = [m for m in members if any(k in m.lower() for k in
            ('pipeline', 'bake', 'material', 'build', 'rig', 'spec'))]
    u.log('C26_MHSEEK character relevant members: %s' % keep)

u.log('C26_MHSEEK_DONE')
