#!/usr/bin/env python3
"""Forensic comparison: skeleton rest pose vs authored animation FBX.

Q1: Does the rig the clips were authored on (C26_KitBase_v002 -> A_C26_*.fbx)
    share its rest pose with the rig the UE skeleton was built from
    (C26_KitBase_v001.fbx)?  If not, animation-only import onto that skeleton
    is broken by exactly the difference, and that is the contortion.

Q2: Where does the bowling arm actually end up at the release frame when the
    clip is evaluated in the FBX's own frame (i.e. what UE sees)?
"""
import math
import sys

sys.path.insert(0, 'Tools')
from fbxread import load, get_objects, bone_models, connections, curve_nodes, curves


def clean(name):
    return name.replace('\x00\x01Model', '').replace('\x00\x01', '')


def rest_table(path):
    nodes, ver, tail = load(path)
    obj, conn = get_objects(nodes)
    bones = bone_models(obj)
    parent_of, children_of = connections(conn)
    table = {}
    for bid, m in bones.items():
        nm = clean(m.prop(1))
        t = m.p70('Lcl Translation')
        r = m.p70('Lcl Rotation')
        s = m.p70('Lcl Scaling')
        parent = None
        if bid in parent_of and parent_of[bid] in bones:
            parent = clean(bones[parent_of[bid]].prop(1))
        table[nm] = {'id': bid, 't': tuple(t) if t else None,
                     'r': tuple(r) if r else None, 's': tuple(s) if s else None,
                     'parent': parent, 'model': m}
    creator = None
    for n in nodes:
        if n.name == 'Creator':
            creator = n.prop(0)
    return table, nodes, ver, creator


def angle_diff_deg(a, b):
    """Smallest angular difference between two Euler triples applied as XYZ... we
    compare quaternions built from Euler order used by FBX (XYZ order is a fair
    approximation for small deltas; for the report we mostly care about large ones)."""
    def quat(e):
        x, y, z = (math.radians(c) for c in e)
        cx, sx = math.cos(x/2), math.sin(x/2)
        cy, sy = math.cos(y/2), math.sin(y/2)
        cz, sz = math.cos(z/2), math.sin(z/2)
        # FBX/UE convention: R = Rz * Ry * Rx applied to column vectors
        return (cz*cy*cx + sz*sy*sx,
                cz*cy*sx - sz*sy*cx,
                cz*sy*cx + sz*cy*sx,
                sz*cy*cx - cz*sy*sx)
    qa, qb = quat(a), quat(b)
    d = abs(sum(x*y for x, y in zip(qa, qb)))
    d = min(1.0, d)
    return math.degrees(2*math.acos(d))


def main():
    base, _, vbase, cbase = rest_table('ArtSource/Exports/C26_KitBase_v001.fbx')
    anim, anodes, vanim, canim = rest_table('ArtSource/Exports/Animations/A_C26_BowlingPace.fbx')
    print('v001 FBX version %d creator: %s' % (vbase, str(cbase)[:70]))
    print('anim FBX version %d creator: %s' % (vanim, str(canim)[:70]))
    print('bones: v001=%d anim=%d' % (len(base), len(anim)))

    common = [n for n in base if n in anim and n != 'Armature' and n != 'Armature.001']
    print('common bones:', len(common))

    worst = []
    for n in sorted(common):
        b, a = base[n], anim[n]
        rb = b['r'] if b['r'] else (0, 0, 0)
        ra = a['r'] if a['r'] else (0, 0, 0)
        tb = b['t'] if b['t'] else (0, 0, 0)
        ta = a['t'] if a['t'] else (0, 0, 0)
        dr = angle_diff_deg(rb, ra)
        dt = max(abs(x-y) for x, y in zip(tb, ta))
        worst.append((dr, dt, n, rb, ra))
    worst.sort(reverse=True)
    print('\n=== REST POSE DIFFERENCES (rotation degrees | translation) ===')
    for dr, dt, n, rb, ra in worst[:15]:
        print('%-28s rot %7.2f deg  trans %8.4f' % (n, dr, dt))
    same = sum(1 for dr, *_ in worst if dr < 0.5)
    print('%d/%d bones within 0.5 deg' % (same, len(worst)))

    # Also compare parents
    print('\n=== PARENT MISMATCHES ===')
    mism = 0
    for n in common:
        pb = base[n]['parent'].replace('mixamorig:', '') if base[n]['parent'] else None
        pa = anim[n]['parent'].replace('mixamorig:', '') if anim[n]['parent'] else None
        if pb != pa:
            mism += 1
            print('%-28s v001 parent=%s  anim parent=%s' % (n.replace('mixamorig:', ''), pb, pa))
    print('parent mismatches:', mism)


if __name__ == '__main__':
    main()
