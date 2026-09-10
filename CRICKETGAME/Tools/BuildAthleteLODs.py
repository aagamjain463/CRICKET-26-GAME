"""Build the mobile LOD chain on the hero athlete skeletal mesh.

Run:  UnrealEditor CRICKETGAME.uproject -ExecutePythonScript=Tools/BuildAthleteLODs.py \
          -unattended -nosplash -nosound -abslog=Artifacts/athlete_lods.log

Every match spawns fourteen of this mesh and nine of them are ring fielders forty metres from the
lens, so a single-LOD athlete pays full skinning cost for a few dozen pixels. The previous attempt
(inside Tools/FinishMatchAssets.py) called `regenerate_lod` on the subsystem *class* rather than on
an instance of it, swallowed the resulting AttributeError with a warning, and never reached its
`save_loaded_asset`, so it reported nothing and changed nothing. This script resolves the API
explicitly, fails loudly if none of the candidates work, and verifies the count after saving.
"""
import unreal as u

LIB = u.EditorAssetLibrary
PATH = '/Game/Cricket26/Characters/SK_Cricketer_Match'
WANT = 3

# A headless Python host does not load the editor-only mesh modules on its own.
for name in ('SkeletalMeshEditor', 'StaticMeshEditor'):
    try:
        u.load_module(name)
    except Exception as exc:                                    # noqa: BLE001 - reported, not fatal
        u.log_warning('C26_ATHLETE_LOD module %s unavailable: %s' % (name, exc))

mesh = LIB.load_asset(PATH)
assert mesh, 'Missing ' + PATH

subsystem = None
cls = getattr(u, 'SkeletalMeshEditorSubsystem', None)
if cls:
    try:
        subsystem = u.get_editor_subsystem(cls)
    except Exception as exc:                                    # noqa: BLE001
        u.log_warning('C26_ATHLETE_LOD subsystem unavailable: %s' % exc)


def count(m):
    """LOD count, whichever of the several spellings this engine build exposes."""
    for holder, name in ((m, 'get_lod_num'), (m, 'get_num_lods'), (m, 'get_lod_count'),
                         (subsystem, 'get_lod_count'),
                         (u.EditorSkeletalMeshLibrary, 'get_lod_count')):
        fn = getattr(holder, name, None) if holder is not None else None
        if not fn:
            continue
        try:
            return int(fn(m) if holder is not m else fn())
        except Exception:                                       # noqa: BLE001 - try the next spelling
            continue
    return -1


before = count(mesh)
u.log('C26_ATHLETE_LOD before=%d' % before)

built, how = False, 'none'
attempts = []
if subsystem is not None and hasattr(subsystem, 'regenerate_lod'):
    attempts.append(('subsystem', lambda: subsystem.regenerate_lod(mesh, WANT, False, False)))
if hasattr(u, 'EditorSkeletalMeshLibrary'):
    attempts.append(('library', lambda: u.EditorSkeletalMeshLibrary.regenerate_lod(mesh, WANT, False, False)))
for label, call in attempts:
    try:
        result = call()
    except Exception as exc:                                    # noqa: BLE001 - next candidate
        u.log_warning('C26_ATHLETE_LOD %s failed: %s' % (label, exc))
        continue
    if result is False:
        u.log_warning('C26_ATHLETE_LOD %s declined' % label)
        continue
    built, how = True, label
    break

if built:
    LIB.save_loaded_asset(mesh)

after = count(LIB.load_asset(PATH))
u.log('C26_ATHLETE_LOD via=%s before=%d after=%d' % (how, before, after))
if after >= WANT:
    u.log('C26_ATHLETE_LOD_COMPLETE lods=%d' % after)
else:
    u.log_error('C26_ATHLETE_LOD_FAIL only %d LOD(s) on %s' % (after, PATH))
