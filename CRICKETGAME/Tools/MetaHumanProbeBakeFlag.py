"""Find which pipeline class carries bBakeMaterials, and confirm which one is actually in use."""
import unreal as u

PIPELINES = [
    'BP_DefaultPipeline',
    'BP_DefaultLegacyPipeline',
    'BP_DefaultLegacyPipeline_Low',
    'BP_DefaultUEFNPipeline_Low',
]

for name in PIPELINES:
    pkg = '/MetaHumanCharacter/BuildPipeline/%s' % name
    obj = u.load_object(None, pkg + '.' + name)
    if obj is None:
        u.log('C26_MHCLS %-28s -> not loadable' % name)
        continue
    cls = None
    try:
        cls = obj.generated_class()
    except Exception:                                          # noqa: BLE001
        cls = None
    if cls is None:
        cls = obj.get_class()
    u.log('C26_MHCLS %-28s class=%s' % (name, cls.get_name()))

    cdo = u.get_default_object(cls)
    # Walk the class chain looking for any property whose name mentions baking.
    seen = []
    walker = cls
    while walker is not None:
        for prop in ('bBakeMaterials', 'BakeMaterials', 'bBakeTextures', 'bBuildMaterials'):
            try:
                seen.append('%s.%s=%s' % (walker.get_name(), prop, cdo.get_editor_property(prop)))
            except Exception:                                  # noqa: BLE001
                pass
        try:
            walker = walker.get_super_class()
        except Exception:                                      # noqa: BLE001
            walker = None
        if walker is not None and walker.get_name() in ('Object', 'Class'):
            break
    u.log('C26_MHCLS   bake props: %s' % (seen or 'NONE FOUND'))

# Which pipeline is the project actually configured to use?
try:
    settings = u.get_default_object(u.MetaHumanCharacterPaletteProjectSettings)
    u.log('C26_MHCLS configured DefaultCharacterPipelineClass = %s'
          % settings.get_editor_property('DefaultCharacterPipelineClass'))
except Exception as exc:                                       # noqa: BLE001
    u.log('C26_MHCLS could not read project settings: %s' % exc)

u.log('C26_MHCLS_DONE')
