"""Find a working way to set a material on a USkeletalMesh asset, and verify current state.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ProbeMaterialSetter.py

Why: mutating the FSkeletalMaterial structs from get_editor_property('materials') and writing the
array back with set_editor_property('materials', ...) does not persist on USkeletalMesh, and
SkeletalMeshEditorSubsystem in 5.8 exposes only overlay-material functions (no set_material).

This probe enumerates every material-related member the Python bindings actually expose on
USkeletalMesh / USkinnedAsset / USkeletalMeshEditorSubsystem, then re-reads SK_Cricketer_Match and
the hero scans so we know whether the earlier assignment attempt stuck or not.

Output goes to the engine log, not stdout -- grep C26_MATSET.
"""
import unreal as u

LIB = u.EditorAssetLibrary


def report(label, value):
    u.log('C26_MATSET %s = %s' % (label, value))


def members(obj, needles):
    out = []
    for m in dir(obj):
        if any(n in m.lower() for n in needles):
            out.append(m)
    return sorted(out)


def slots_of(mesh):
    try:
        out = []
        for sm in mesh.get_editor_property('materials'):
            mi = sm.get_editor_property('material_interface')
            out.append(mi.get_name() if mi else 'NONE')
        return out
    except Exception as e:
        return ['unreadable: %s' % e]


def main():
    u.log('C26_MATSET ' + '=' * 70)

    # --- what does the mesh binding expose? ---------------------------------------------------
    m = LIB.load_asset('/Game/Cricket26/Characters/SK_Cricketer_Match')
    if m:
        report('SkeletalMesh.material_members', members(m, ['material', 'slot']))
        report('SkeletalMesh.set_members', members(m, ['set_']))
        # Is the materials property even writable?
        try:
            mats = m.get_editor_property('materials')
            report('materials.type', type(mats))
            report('materials.len', len(mats))
            if mats:
                report('slot0.type', type(mats[0]))
                report('slot0.members', members(mats[0], ['material', 'slot']))
        except Exception as e:
            report('materials', 'unreadable (%s)' % e)

    # --- class-level helpers ------------------------------------------------------------------
    for cname in ('SkeletalMeshEditorSubsystem', 'StaticMeshEditorSubsystem',
                  'MaterialEditingLibrary', 'EditorAssetLibrary'):
        cls = getattr(u, cname, None)
        if cls is None:
            report(cname, 'not exposed')
            continue
        try:
            inst = cls() if cname.endswith('Subsystem') else cls
        except Exception:
            inst = cls
        report(cname + '.members', members(inst, ['material', 'slot']))

    # --- current on-disk state: did anything stick? -------------------------------------------
    u.log('C26_MATSET ---- current slot state ----')
    for p in ['/Game/Cricket26/Characters/SK_Cricketer_Match',
              '/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBatter',
              '/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBowler']:
        mm = LIB.load_asset(p)
        short = p.rsplit('/', 1)[-1]
        report(short, ','.join(slots_of(mm)) if mm else 'LOAD FAILED')

    u.log('C26_MATSET_DONE')


main()
