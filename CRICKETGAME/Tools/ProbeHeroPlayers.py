"""Probe the hero player scans: skeleton, materials, and scale.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/ProbeHeroPlayers.py

Why this matters
----------------
The match currently renders SK_Cricketer_Match, whose five material slots all resolve to
/Engine/EngineMaterials/WorldGridMaterial -- the renderer's default grey. Meanwhile
Content/Cricket26/Characters/Players/ holds ten 7-8 MB hero scans and
Content/Cricket26/Materials/Players/ holds eleven MI_Player_* material instances built for them.
That asset set is the "premium realistic player" work, already authored and never used.

The one question that decides how much work the swap is:

    do the hero scans share SK_Cricketer_KitBase_Skeleton with SK_Cricketer_Match?

If yes, swapping the mesh is a one-line change and every authored clip (A_C26_BattingDrive,
A_C26_BowlingPace, A_Run, A_Idle) plus the whole procedural posing system keeps working untouched.
If no, the clips need IK Retargeting onto each hero skeleton first.

Also reports mesh height, because AC26Athlete::RebuildReference() applies a 0.48 ground scale
when a bound mesh is taller than 250 units -- a hero scan at a different scale would need that
handled or the players will be the wrong size.

Output goes to the engine log, not stdout -- grep C26_HEROPROBE.
"""
import unreal as u

BASE_MESH = '/Game/Cricket26/Characters/SK_Cricketer_Match'
HEROES = [
    'HeroBatter', 'HeroBowler', 'HeroKeeper', 'HeroUmpire',
    'HeroFielder01', 'HeroFielder02', 'HeroFielder03',
    'HeroFielder04', 'HeroFielder05', 'HeroFielder06',
]
HERO_DIR = '/Game/Cricket26/Characters/Players/SK_Cricketer_'
MAT_DIR = '/Game/Cricket26/Materials/Players/'

LIB = u.EditorAssetLibrary


def report(label, value):
    u.log('C26_HEROPROBE %s = %s' % (label, value))


def mesh_info(path, label):
    m = LIB.load_asset(path)
    if not m:
        report(label + '.load', 'FAILED')
        return None
    skel = None
    try:
        skel = m.get_editor_property('skeleton')
    except Exception as e:
        report(label + '.skeleton', 'unreadable (%s)' % e)
    report(label + '.skeleton', skel.get_path_name() if skel else 'None')

    # Bounds tell us the scale, and whether the 0.48 rule will trigger.
    try:
        b = m.get_bounds()
        report(label + '.bounds', 'origin=%s extent=%s' % (b.origin, b.box_extent))
    except Exception as e:
        report(label + '.bounds', 'unreadable (%s)' % e)

    try:
        mats = m.get_editor_property('materials')
        names = []
        for sm in mats:
            try:
                mi = sm.get_editor_property('material_interface')
                names.append('%s=%s' % (sm.get_editor_property('material_slot_name'),
                                        mi.get_name() if mi else 'NONE'))
            except Exception:
                names.append('?')
        report(label + '.slots(%d)' % len(mats), ', '.join(names))
    except Exception as e:
        report(label + '.materials', 'unreadable (%s)' % e)
    return skel


def main():
    u.log('C26_HEROPROBE ' + '=' * 70)

    u.log('C26_HEROPROBE ---- BASE (what the match renders today) ----')
    base_skel = mesh_info(BASE_MESH, 'BASE')
    base_skel_path = base_skel.get_path_name() if base_skel else None

    u.log('C26_HEROPROBE ---- HERO SCANS ----')
    matching = 0
    for h in HEROES:
        skel = mesh_info(HERO_DIR + h, h)
        sp = skel.get_path_name() if skel else None
        same = (sp is not None and sp == base_skel_path)
        report(h + '.SHARES_BASE_SKELETON', same)
        if same:
            matching += 1

    report('heroes_sharing_base_skeleton', '%d / %d' % (matching, len(HEROES)))

    u.log('C26_HEROPROBE ---- MI_Player_* material parents ----')
    for h in HEROES + ['Batter']:
        pass
    try:
        for a in sorted(LIB.list_assets(MAT_DIR, recursive=False, include_folder=False)):
            mi = LIB.load_asset(a)
            if not mi:
                continue
            parent = None
            try:
                parent = mi.get_editor_property('parent')
            except Exception:
                pass
            report(a.rsplit('/', 1)[-1] + '.parent', parent.get_path_name() if parent else 'None')
    except Exception as e:
        report('mi_list', 'unreadable (%s)' % e)

    u.log('C26_HEROPROBE ---- master material usage flags ----')
    for mp in ['/Game/Cricket26/Materials/M_Athlete_PBR',
               '/Game/Cricket26/Materials/M_C26_PlayerSkin']:
        m = LIB.load_asset(mp)
        short = mp.rsplit('/', 1)[-1]
        if not m:
            report(short + '.load', 'FAILED')
            continue
        for flag in ('used_with_skeletal_mesh', 'used_with_static_lighting',
                     'used_with_instanced_static_meshes'):
            try:
                report('%s.%s' % (short, flag), m.get_editor_property(flag))
            except Exception as e:
                report('%s.%s' % (short, flag), 'unreadable (%s)' % e)

    u.log('C26_HEROPROBE_DONE')


main()
