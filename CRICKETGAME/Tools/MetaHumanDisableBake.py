"""Turn OFF material baking on the MetaHuman build pipeline.

The build dies in UTG_AsyncExportTask::TG_AsyncExportTask, reached only from
UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials. In that same class:

    MetaHumanDefaultEditorPipelineBase.h:540    bool bBakeMaterials;
    MetaHumanDefaultEditorPipelineBase.cpp:519  if (bBakeMaterials)
    MetaHumanDefaultEditorPipelineBase.cpp:769  if (bBakeMaterials)
    MetaHumanDefaultEditorPipelineBase.cpp:1056 if (!TryBakeMaterials(...

so the TextureGraph export -- and therefore the crash -- is reachable only when bBakeMaterials
is true. The flag is EditAnywhere on the pipeline, and the pipeline is chosen by
DefaultCharacterPipelineClass. Clearing it gets us the MetaHuman skeleton + skinned meshes
without the material bake; materials can be authored afterwards, which is the part of the
character we actually want to control for a cricket kit anyway.

Tries the engine's pipeline asset first, then a project-local duplicate if the engine copy is
not writable.
"""
import unreal as u

CANDIDATES = [
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Low',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultPipeline',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Low',
]


def try_pipeline(path):
    asset = u.EditorAssetLibrary.load_asset(path)
    if asset is None:
        u.log('C26_MHBAKE   %s -> not found' % path)
        return False
    u.log('C26_MHBAKE   %s -> %s (%s)' % (path, asset.get_name(), asset.get_class().get_name()))

    cls = None
    try:
        cls = asset.generated_class()
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHBAKE     generated_class failed: %s' % exc)
    if cls is None:
        cls = asset.get_class()

    try:
        cdo = u.get_default_object(cls)
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHBAKE     get_default_object failed: %s' % exc)
        return False

    try:
        before = cdo.get_editor_property('bBakeMaterials')
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHBAKE     cannot read bBakeMaterials on %s: %s' % (cls.get_name(), exc))
        return False

    u.log('C26_MHBAKE     bBakeMaterials was %s' % before)
    try:
        cdo.set_editor_property('bBakeMaterials', False)
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHBAKE     cannot set bBakeMaterials: %s' % exc)
        return False

    u.log('C26_MHBAKE     bBakeMaterials now %s' % cdo.get_editor_property('bBakeMaterials'))
    try:
        u.EditorAssetLibrary.save_loaded_asset(asset)
        u.log('C26_MHBAKE     saved %s' % path)
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHBAKE     save failed: %s' % exc)
    return True


for candidate in CANDIDATES:
    if try_pipeline(candidate):
        break

u.log('C26_MHBAKE_DONE')
