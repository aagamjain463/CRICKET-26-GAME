"""Introspect the MetaHuman build API to find a way around the TextureGraph crash.

The build dies in UTG_AsyncExportTask::TG_AsyncExportTask, reached from
UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials. The plugin config ships several
pipelines (BP_DefaultPipeline, BP_DefaultLegacyPipeline_{Cinematic,High,Medium,Low},
BP_DefaultUEFNPipeline_{High,Medium,Low}); the UEFN ones are the lightweight target and may
not bake materials through TextureGraph at all. This prints what the build parameters and the
subsystem actually accept, so the choice is made from the real API and not from a guess.
"""
import unreal as u

def show(obj, label):
    u.log('C26_MHPROBE ---- %s (%s) ----' % (label, type(obj).__name__))
    try:
        props = obj.get_editor_property_names() if hasattr(obj, 'get_editor_property_names') else []
    except Exception as exc:                                   # noqa: BLE001
        u.log('C26_MHPROBE   property listing failed: %s' % exc)
        props = []
    for name in props:
        try:
            u.log('C26_MHPROBE   %s = %r' % (name, obj.get_editor_property(name)))
        except Exception as exc:                               # noqa: BLE001
            u.log('C26_MHPROBE   %s = <unreadable: %s>' % (name, exc))

# 1. What can be passed to build_meta_human?
try:
    params = u.MetaHumanCharacterEditorBuildParameters()
    show(params, 'MetaHumanCharacterEditorBuildParameters')
except Exception as exc:                                       # noqa: BLE001
    u.log('C26_MHPROBE could not construct build parameters: %s' % exc)

# 2. What does the subsystem expose?
sub = u.get_editor_subsystem(u.MetaHumanCharacterEditorSubsystem)
for attr in sorted(d for d in dir(sub) if not d.startswith('_')):
    if any(k in attr.lower() for k in ('build', 'pipeline', 'bake', 'material', 'quality')):
        u.log('C26_MHPROBE sub.%s' % attr)

# 3. Are the pipeline classes actually loadable?
for path in [
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultPipeline.BP_DefaultPipeline_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultLegacyPipeline_Low.BP_DefaultLegacyPipeline_Low_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Low.BP_DefaultUEFNPipeline_Low_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_Medium.BP_DefaultUEFNPipeline_Medium_C',
    '/MetaHumanCharacter/BuildPipeline/BP_DefaultUEFNPipeline_High.BP_DefaultUEFNPipeline_High_C',
]:
    loaded = u.EditorAssetLibrary.load_asset(path)
    u.log('C26_MHPROBE pipeline %s -> %s' % (path.rsplit('/', 1)[-1], loaded))

u.log('C26_MHPROBE_DONE')
