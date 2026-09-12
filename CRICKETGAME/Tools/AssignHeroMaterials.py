"""Assign MI_Player_* materials to the hero player scans -- via the skeletal-mesh editor API.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/AssignHeroMaterials.py

The first attempt mutated the slot structs returned by `get_editor_property('materials')` and
wrote them back with `set_editor_property('materials', ...)`. That silently does nothing on a
USkeletalMesh: the log showed `WorldGridMaterial -> WorldGridMaterial` for all ten scans.

USkeletalMeshEditorSubsystem::SetMaterial() is the supported path -- it goes through the same code
the Skeletal Mesh Editor's material slot UI uses. Signature in Python:

    unreal.SkeletalMeshEditorSubsystem().set_material(skeletal_mesh, material, lod_index, section_index)

This script probes the real method name/signature before using it, then verifies every assignment
by re-reading the asset from disk, so "OK" here means the asset genuinely changed.
"""
import unreal as u

HERO_DIR = '/Game/Cricket26/Characters/Players/SK_Cricketer_Hero'
MAT_DIR = '/Game/Cricket26/Materials/Players/MI_Player_'

PAIRS = [
    ('Batter', 'Batter'),
    ('Bowler', 'Bowler'),
    ('Keeper', 'Keeper'),
    ('Umpire', 'Umpire'),
    ('Fielder01', 'Fielder_01'),
    ('Fielder02', 'Fielder_02'),
    ('Fielder03', 'Fielder_03'),
    ('Fielder04', 'Fielder_04'),
    ('Fielder05', 'Fielder_05'),
    ('Fielder06', 'Fielder_06'),
]

LIB = u.EditorAssetLibrary


def report(label, value):
    u.log('C26_HEROMAT %s = %s' % (label, value))


def slots_of(mesh):
    """Current material interface name per slot, or [] if unreadable."""
    try:
        out = []
        for sm in mesh.get_editor_property('materials'):
            mi = sm.get_editor_property('material_interface')
            out.append(mi.get_name() if mi else 'NONE')
        return out
    except Exception as e:
        report('slots_of', 'unreadable (%s)' % e)
        return []


def find_setter():
    """Resolve the skeletal-mesh material setter across the API spellings UE has used."""
    cls = getattr(u, 'SkeletalMeshEditorSubsystem', None)
    if cls is None:
        return None, None, 'SkeletalMeshEditorSubsystem not exposed'
    try:
        sub = cls()
    except Exception as e:
        return None, None, 'cannot instantiate (%s)' % e
    for name in ('set_material', 'SetMaterial'):
        if hasattr(sub, name):
            return sub, name, None
    return sub, None, 'no set_material; has: %s' % [m for m in dir(sub) if 'aterial' in m]


def main():
    u.log('C26_HEROMAT ' + '=' * 70)

    sub, setter, err = find_setter()
    if setter is None:
        report('SETTER', 'UNAVAILABLE: %s' % err)
        return
    report('setter', setter)

    ok = 0
    for mesh_suffix, mi_suffix in PAIRS:
        mesh = LIB.load_asset(HERO_DIR + mesh_suffix)
        mi = LIB.load_asset(MAT_DIR + mi_suffix)
        if not mesh or not mi:
            report(mesh_suffix, 'MISSING mesh=%s mat=%s' % (bool(mesh), bool(mi)))
            continue

        before = slots_of(mesh)

        # Try the documented 4-arg form first, then the 3-arg form some versions use.
        applied = False
        last_err = None
        for args in ((mesh, mi, 0, 0), (mesh, mi, 0)):
            try:
                getattr(sub, setter)(*args)
                applied = True
                break
            except Exception as e:
                last_err = e
        if not applied:
            report(mesh_suffix, 'set_material FAILED (%s)' % last_err)
            continue

        LIB.save_loaded_asset(mesh)

        # Verify from a freshly reloaded asset, not the in-memory object.
        try:
            LIB.unload_asset(mesh)
        except Exception:
            pass
        reloaded = LIB.load_asset(HERO_DIR + mesh_suffix)
        after = slots_of(reloaded) if reloaded else []

        good = bool(after) and after[0] == mi.get_name()
        report(mesh_suffix, 'slots=%d  %s -> %s  %s'
               % (len(after), ','.join(before), ','.join(after), 'OK' if good else 'NOT APPLIED'))
        if good:
            ok += 1

    report('meshes_fixed', '%d / %d' % (ok, len(PAIRS)))

    # --- SK_Cricketer_Match: the mesh the match actually renders today -------------------------
    # Its slot names do not all have a same-named material (Jerseymat / Trousermat / Eyesmat have
    # no asset), so map by intent onto the Remy PBR instances that do exist.
    MATCH_PATH = '/Game/CricketerNone'
    MATCH_PATH = '/Game/Cricket26/Characters/SK_Cricketer_Match'
    MATCH_MAP = {
        'Bodymat': '/Game/Cricket26/Characters/Bodymat',
        'Jerseymat': '/Game/Cricket26/Characters/Topmat',
        'Trousermat': '/Game/Cricket26/Characters/Bottommat',
        'Eyelashmat': '/Game/Cricket26/Characters/Eyelashmat',
        'Eyesmat': '/Game/Cricket26/Characters/Bodymat',
    }
    m = LIB.load_asset(MATCH_PATH)
    if m:
        u.log('C26_HEROMAT ---- SK_Cricketer_Match ----')
        before = slots_of(m)
        try:
            mats = m.get_editor_property('materials')
        except Exception as e:
            report('Match.materials', 'unreadable (%s)' % e)
            mats = []
        for i, sm in enumerate(mats):
            slot_name = str(sm.get_editor_property('material_slot_name'))
            target = MATCH_MAP.get(slot_name)
            if not target:
                report('Match.' + slot_name, 'no mapping for slot %d' % i)
                continue
            mi = LIB.load_asset(target)
            if not mi:
                report('Match.' + slot_name, 'target missing %s' % target)
                continue
            try:
                getattr(sub, setter)(m, mi, 0, i)
            except Exception as e:
                # 3-arg form: index is the section, not the LOD
                try:
                    getattr(sub, setter)(m, mi, i)
                except Exception as e2:
                    report('Match.' + slot_name, 'FAILED (%s / %s)' % (e, e2))
                    continue
            report('Match.' + slot_name, '-> %s' % mi.get_name())
        LIB.save_loaded_asset(m)
        try:
            LIB.unload_asset(m)
        except Exception:
            pass
        m2 = LIB.load_asset(MATCH_PATH)
        report('Match.after', ','.join(slots_of(m2)) if m2 else 'reload failed')

    u.log('C26_HEROMAT_DONE')


main()
