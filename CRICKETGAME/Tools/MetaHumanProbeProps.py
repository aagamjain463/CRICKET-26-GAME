"""Which pipeline properties can actually be reached and set from Python?

bBakeMaterials sits at line 540 of MetaHumanDefaultEditorPipelineBase.h, and bOptimizeBoneCounts
at line 533 of the same class. Testing both tells us whether the whole class is unreachable from
Python or only that one property, which decides whether disabling the material bake is possible
at all from script.
"""
import unreal as u

NAMES = [
    'bOptimizeBoneCounts',   # same class, line 533
    'bBakeMaterials',        # the one we need, line 540
    'bScalableNormals',      # same class, line 544
    'PipelineType',
    'pipeline_type',
]

for bp_name in ['BP_DefaultLegacyPipeline', 'BP_DefaultPipeline', 'BP_DefaultLegacyPipeline_Low', 'BP_DefaultUEFNPipeline_Low']:
    pkg = '/MetaHumanCharacter/BuildPipeline/%s' % bp_name
    obj = u.load_object(None, pkg + '.' + bp_name)
    if obj is None:
        continue
    cls = obj.generated_class()
    cdo = u.get_default_object(cls)
    u.log('C26_MHPROP ==== %s (class %s) ====' % (bp_name, cls.get_name()))
    for n in NAMES:
        try:
            v = cdo.get_editor_property(n)
            u.log('C26_MHPROP   %-22s READABLE = %r' % (n, v))
        except Exception as exc:                               # noqa: BLE001
            u.log('C26_MHPROP   %-22s no (%s)' % (n, str(exc)[:70]))

u.log('C26_MHPROP_DONE')
