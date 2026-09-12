"""Determine whether the pipeline Blueprints really derive from MetaHumanDefaultEditorPipelineBase.

The crash stack goes through UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials, and
bBakeMaterials is a UPROPERTY on that class (header line 539). But reading it off the pipeline
Blueprint CDOs failed, which only makes sense if those Blueprints derive from a different base.
This checks the type relationship directly instead of walking a class chain by name.
"""
import unreal as u

base = getattr(u, 'MetaHumanDefaultEditorPipelineBase', None)
u.log('C26_MHBASE unreal.MetaHumanDefaultEditorPipelineBase = %s' % base)

# Every MetaHuman pipeline class the module exposes.
names = sorted(n for n in dir(u) if 'MetaHuman' in n and ('Pipeline' in n or 'Build' in n))
u.log('C26_MHBASE candidate types: %s' % names)

for name in ['BP_DefaultPipeline', 'BP_DefaultLegacyPipeline_Low', 'BP_DefaultUEFNPipeline_Low']:
    pkg = '/MetaHumanCharacter/BuildPipeline/%s' % name
    obj = u.load_object(None, pkg + '.' + name)
    if obj is None:
        u.log('C26_MHBASE %s not loadable' % name)
        continue
    cls = obj.generated_class()
    cdo = u.get_default_object(cls)
    u.log('C26_MHBASE %s cls=%s' % (name, cls.get_name()))
    if base is not None:
        u.log('C26_MHBASE   isinstance(cdo, base) = %s' % isinstance(cdo, base))
    # Ask the CDO for every property it will admit to having.
    try:
        props = cdo.get_editor_property_names()
    except Exception as exc:                                   # noqa: BLE001
        props = []
        u.log('C26_MHBASE   property enumeration failed: %s' % exc)
    hits = [p for p in props if 'ake' in p or 'aterial' in p]
    u.log('C26_MHBASE   bake/material props: %s' % (hits or 'NONE'))
    for p in hits:
        try:
            u.log('C26_MHBASE     %s = %r' % (p, cdo.get_editor_property(p)))
        except Exception as exc:                               # noqa: BLE001
            u.log('C26_MHBASE     %s unreadable: %s' % (p, exc))

u.log('C26_MHBASE_DONE')
