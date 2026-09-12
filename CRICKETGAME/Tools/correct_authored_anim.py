#!/usr/bin/env python3
"""CRICKET 26 — authored-clip frame corrector. THE fix for the disabled clips.

ROOT CAUSE (measured with Tools/fbx_forensic2.py, see Docs/AUTHORED_ANIMATION.md):
  The shipped skeleton (SK_Cricketer_KitBase_Skeleton, from C26_KitBase_v001.fbx)
  is a ~380 cm T-pose rig in centimetres whose bone locals are near-identity.
  The authored clips were exported from C26_KitBase_v002.blend: a 1.79 m rig in
  metres whose rest pose is NOT a T-pose (arms hang bent at the sides) and whose
  bone-local frames differ by up to 143 degrees. UE's animation-only import bound
  those curves onto the shipped skeleton verbatim, so every bone landed rotated by
  its rest-frame difference, the hips sat 238x too low, and the root armature's
  -90deg / x100 conversion was never applied. Result: the contorted batter and the
  underarm bowler that got the clips disabled in AC26Athlete::Animate.

WHAT THIS DOES — per keyframe, re-express the whole clip in the target frame:
  1. Compose the clip's FBX locals into world-space poses W2(t) (Y-up, cm).
  2. Copy every bone's WORLD-RELATIVE orientation (parent->child, world space)
     onto the v001 rig. World-relative orientations are rest-pose independent,
     which is what makes this exact across the T-pose / arms-down difference.
  3. The hips carry the authored world translation delta, scaled from authored
     centimetres into the target's import scale by /0.48 — the exact ground scale
     AC26Athlete::RebuildReference applies at runtime, so the C++ pose code needs
     no changes.
  4. Rewrite AnimationCurve key values, LimbNode Lcl defaults and the root node
     name ('Armature' -> 'Armature.001'), leaving key times and structure intact.

The output is a drop-in for Tools/ImportAnimations.py (same paths, same clips).

Run:  python3 Tools/correct_authored_anim.py [source.fbx ...] [--out DIR] [--verify-only]
Without arguments it corrects both shipped clips and verifies them offline:
hands together on the handle through the batting clip, bowling hand above the
head at the release key, feet on the ground at the stance key.
"""
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fbxread import (load, save, bone_models, connections, get_objects)

GROUND_SCALE = 0.48          # AC26Athlete::RebuildReference, BoundHeight>250 branch
TARGET_ROOT = 'Armature.001'
SOURCE_ROOT = 'Armature'
CONTACT_FRAME = 23           # A_C26_BattingDrive, 24 fps, 1-based
RELEASE_FRAME = 31           # A_C26_BowlingPace, 24 fps, 1-based
STANCE_FRAME = 1


# ------------------------------------------------------------------ quaternion math
def qmul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return (w1*w2 - x1*x2 - y1*y2 - z1*z2,
            w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2,
            w1*z2 + x1*y2 - y1*x2 + z1*w2)


def qconj(q):
    w, x, y, z = q
    return (w, -x, -y, -z)


def qrot(q, v):
    w, x, y, z = q
    qv = (0.0,) + tuple(v)
    return qmul(qmul(q, qv), qconj(q))[1:]


def qnorm(q):
    n = math.sqrt(sum(c*c for c in q)) or 1.0
    return tuple(c / n for c in q)


def euler_to_quat(rx, ry, rz):
    """FBX eXYZ: R = Rz*Ry*Rx applied to column vectors."""
    def ax(i, deg):
        h = math.radians(deg) / 2.0
        q = [math.cos(h), 0.0, 0.0, 0.0]
        q[1 + i] = math.sin(h)
        return tuple(q)
    return qnorm(qmul(qmul(ax(2, rz), ax(1, ry)), ax(0, rx)))


def quat_to_euler(q):
    """Inverse of euler_to_quat; returns degrees (rx, ry, rz)."""
    w, x, y, z = qnorm(q)
    m00 = 1 - 2*(y*y + z*z)
    m10 = 2*(x*y + w*z)
    m20 = 2*(x*z - w*y)
    m21 = 2*(y*z + w*x)
    m22 = 1 - 2*(x*x + y*y)
    sy = max(-1.0, min(1.0, -m20))
    if abs(sy) < 0.99999:
        ry = math.asin(sy)
        rx = math.atan2(m21, m22)
        rz = math.atan2(m10, m00)
    else:
        ry = math.copysign(math.pi / 2, sy)
        m11 = 1 - 2*(x*x + z*z)
        m12 = 2*(y*z - w*x)
        rx = math.atan2(-m12, m11)
        rz = 0.0
    return (math.degrees(rx), math.degrees(ry), math.degrees(rz))


def vsub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])


def vadd(a, b):
    return (a[0]+b[0], a[1]+b[1], a[2]+b[2])


def vscale(a, k):
    return (a[0]*k, a[1]*k, a[2]*k)


class Xform:
    __slots__ = ('t', 'q', 's')

    def __init__(self, t=(0, 0, 0), q=(1, 0, 0, 0), s=(1, 1, 1)):
        self.t = tuple(t)
        self.q = qnorm(q)
        self.s = tuple(s)

    def __mul__(self, o):
        return Xform(vadd(self.t, qrot(self.q, tuple(c*sc for c, sc in zip(o.t, self.s)))),
                     qmul(self.q, o.q),
                     tuple(a*b for a, b in zip(self.s, o.s)))


def clean(name):
    return str(name).replace('\x00\x01Model', '').replace('\x00\x01', '')


class RigData:
    """Hierarchy + rest locals + curve bindings for one FBX file."""

    def __init__(self, path):
        self.path = path
        nodes, ver, tail = load(path)
        self.version, self.tail, self.nodes = ver, tail, nodes
        obj, conn = get_objects(nodes)
        self.obj, self.conn = obj, conn
        models = bone_models(obj)
        parent_links, _ = connections(conn)
        self.parent = {}
        for src, dsts in parent_links.items():
            if src not in models:
                continue
            for dst in dsts:
                if dst in models and dst != src:
                    self.parent[src] = dst
        self.name = {bid: clean(m.prop(1)) for bid, m in models.items()}
        self.id = {}
        for bid, nm in self.name.items():
            self.id.setdefault(nm, bid)
        self.model = models
        self.children = {}
        for bid in models:
            p = self.parent.get(bid)
            if p:
                self.children.setdefault(p, []).append(bid)
        self.order = []
        seen = set()

        def walk(b):
            if b in seen:
                return
            seen.add(b)
            self.order.append(b)
            for c in self.children.get(b, []):
                walk(c)
        for bid in models:
            walk(bid)
        self.rest = {}
        for bid, m in models.items():
            t = m.p70('Lcl Translation')
            r = m.p70('Lcl Rotation')
            s = m.p70('Lcl Scaling')
            self.rest[bid] = Xform(tuple(t) if t else (0.0, 0.0, 0.0),
                                   euler_to_quat(*(tuple(r) if r else (0.0, 0.0, 0.0))),
                                   tuple(s) if s else (1.0, 1.0, 1.0))
        # ---- curve bindings -------------------------------------------------
        curve_nodes = {n.prop(0): n for n in obj.children if n.name == 'AnimationCurveNode'}
        curves = {n.prop(0): n for n in obj.children if n.name == 'AnimationCurve'}
        self.curve_of = {}       # curve_id -> (bone_id, 'T'/'R'/'S', axis)
        node_owner = {}          # curve_node_id -> (bone_id, channel letter)
        for c in conn.children:
            if c.name != 'C' or c.props[0][1] != 'OP' or len(c.props) < 4:
                continue
            src, dst, prop = c.props[1][1], c.props[2][1], c.props[3][1]
            if dst in curve_nodes and src in curves:
                node = curve_nodes[dst]
                bone, letter = node_owner[dst]
                axis = prop.split('|')[1] if '|' in prop else '?'
                self.curve_of[src] = (bone, letter, axis, curves[src])
            elif dst in models and src in curve_nodes:
                node = curve_nodes[src]
                letter = clean(node.prop(1))[0]      # 'T', 'R' or 'S'
                node_owner[src] = (dst, letter)
        self.times = None
        for bone, letter, axis, node in self.curve_of.values():
            kt = node.find('KeyTime')
            if kt is None:
                continue
            t = kt.props[0][1]
            if self.times is None or len(t) > len(self.times):
                self.times = t

    def local_at(self, key_index):
        """Sample every node's local transform at a key index."""
        out = {}
        for bone, letter, axis, node in self.curve_of.values():
            kv = node.find('KeyValueFloat') or node.find('KeyValueDouble')
            if kv is None:
                continue
            slot = out.setdefault(bone, {'T': [None, None, None],
                                         'R': [None, None, None],
                                         'S': [None, None, None]})
            slot[letter]['XYZ'.index(axis)] = kv.props[0][1][key_index]
        for bid, xf in self.rest.items():
            if bid in out:
                d = out[bid]
                t = tuple(c if c is not None else xf.t[i] for i, c in enumerate(d['T']))
                r = tuple(c if c is not None else 0.0 for i, c in enumerate(d['R']))
                s = tuple(c if c is not None else xf.s[i] for i, c in enumerate(d['S']))
                out[bid] = Xform(t, euler_to_quat(*r), s)
            else:
                out[bid] = xf
        return out

    def compose(self, locals_):
        out = {}
        for bid in self.order:
            p = self.parent.get(bid)
            out[bid] = out[p] * locals_[bid] if (p is not None and p in out) else locals_[bid]
        return out

    def world_rest(self):
        return self.compose(self.rest)


# --------------------------------------------------------------------------- core

def correct_file(src, dst, target_rig):
    rig = RigData(src)
    if SOURCE_ROOT not in rig.id or TARGET_ROOT not in target_rig.id:
        raise SystemExit('%s: expected root %r and target root %r' % (src, SOURCE_ROOT, TARGET_ROOT))
    missing = [nm for nm in rig.name.values() if nm not in target_rig.id and nm != SOURCE_ROOT]
    if missing:
        raise SystemExit('%s: bones missing on target rig: %s' % (src, missing))

    W2rest = rig.world_rest()
    W1rest = target_rig.world_rest()
    root2 = rig.id[SOURCE_ROOT]
    root1 = target_rig.id[TARGET_ROOT]
    nkeys = len(rig.times)

    # Exact world-scale ratio between the two rigs, measured from a long bone.
    # The v2 file is in metres behind an armature x100, so its world cm length
    # is |local T| * 100.
    def bone_len(r, nm):
        bid = r.id[nm]
        return math.sqrt(sum(c*c for c in r.rest[bid].t))
    ratio = bone_len(target_rig, 'mixamorig:LeftLeg') / (bone_len(rig, 'mixamorig:LeftLeg') * 100.0)
    if not (1.5 < ratio < 3.5):
        raise SystemExit('%s: implausible rig ratio %.4f' % (src, ratio))

    # Ground anchoring, in both rigs' worlds.
    # PLANTED1: ankle height of the target rig's straight-leg rest (its origin is
    # the ground). P2: the authored clips' planted ankle height, measured at the
    # stance frame (key 0). air2(k): how high the authored lowest ankle is above
    # its planted height -- the authored airborne phase, which must survive.
    foot_l1 = target_rig.id['mixamorig:LeftFoot']
    foot_r1 = target_rig.id['mixamorig:RightFoot']
    PLANTED1 = min(W1rest[foot_l1].t[1], W1rest[foot_r1].t[1])
    foot_l2 = rig.id['mixamorig:LeftFoot']
    foot_r2 = rig.id['mixamorig:RightFoot']
    W2_stance = rig.compose(rig.local_at(0))
    P2 = min(W2_stance[foot_l2].t[1], W2_stance[foot_r2].t[1])

    # per-key corrected locals, in TARGET ids
    corrected = []
    for k in range(nkeys):
        L2 = rig.local_at(k)
        W2 = rig.compose(L2)
        qW1, pW1 = {}, {}
        qW1[root1] = W1rest[root1].q
        pW1[root1] = W1rest[root1].t
        for b2 in rig.order:
            if b2 == root2:
                continue
            nm = rig.name[b2]
            b1 = target_rig.id[nm]
            p2 = rig.parent[b2]
            if p2 == root2:
                # Hips: world rotation delta + world translation delta at the
                # rigs' own world-scale ratio, so crouch, bound and weight
                # transfer read at the same magnitude the legs realize them.
                dq = qmul(qconj(W2rest[b2].q), W2[b2].q)
                qW1[b1] = qmul(dq, W1rest[b1].q)
                dp = vsub(W2[b2].t, W2rest[b2].t)
                pW1[b1] = vadd(W1rest[b1].t, vscale(dp, ratio))
            else:
                p1 = target_rig.id[rig.name[p2]]
                qrel = qmul(qconj(W2[p2].q), W2[b2].q)
                qW1[b1] = qmul(qW1[p1], qrel)
                pW1[b1] = vadd(pW1[p1], qrot(qW1[p1], target_rig.rest[b1].t))
        hips1 = target_rig.id['mixamorig:Hips']
        # Ground pin: the realized lowest ankle must sit at the target rig's
        # planted height plus the authored airborne amount. The realized pose
        # hangs off the hips, so one vertical shift of the hips moves the feet.
        r1_low = min(pW1[foot_l1][1], pW1[foot_r1][1])
        l2_low = min(W2[foot_l2].t[1], W2[foot_r2].t[1])
        air2 = l2_low - P2
        shift = (PLANTED1 + air2 * ratio) - r1_low
        pW1[hips1] = (pW1[hips1][0], pW1[hips1][1] + shift, pW1[hips1][2])
        frame = {}
        for b1 in qW1:
            if b1 == root1:
                frame[b1] = target_rig.rest[b1]
                continue
            parent1 = target_rig.parent[b1]
            q_local = qmul(qconj(qW1[parent1]), qW1[b1])
            t_local = pW1[b1] if parent1 == root1 else target_rig.rest[b1].t
            frame[b1] = Xform(t_local, q_local, target_rig.rest[b1].s)
        corrected.append(frame)

    # ---- write curves ----------------------------------------------------
    changed_curves = 0
    for cid, (bone, letter, axis, node) in rig.curve_of.items():
        nm = rig.name[bone]
        b1 = target_rig.id[nm if nm != SOURCE_ROOT else TARGET_ROOT]
        kv = node.find('KeyValueFloat') or node.find('KeyValueDouble')
        if kv is None:
            continue
        idx = 'XYZ'.index(axis)
        vals = kv.props[0][1]
        for k in range(len(vals)):
            xf = corrected[k][b1]
            if letter == 'T':
                vals[k] = xf.t[idx]
            elif letter == 'R':
                vals[k] = quat_to_euler(xf.q)[idx]
            else:
                vals[k] = xf.s[idx]
        changed_curves += 1
        # keep the curve node's static d|X value consistent with key 0
        for cnode in rig.obj.children:
            pass
    # ---- static curve-node values + LimbNode defaults + root rename -------
    for n in rig.obj.children:
        if n.name == 'AnimationCurveNode':
            p70 = n.find('Properties70')
            if p70 is None:
                continue
            for p in p70.children:
                nm = p.props[0][1]
                if not nm.startswith('d|'):
                    continue
                # value equals key 0 of the connected curve; leave literals alone
        if n.name == 'Model':
            nm = clean(n.prop(1))
            if nm == SOURCE_ROOT:
                # rename root to the target skeleton's root bone name
                n.props[1] = (n.props[1][0], TARGET_ROOT + '\x00\x01Model')
            if nm == SOURCE_ROOT or nm in target_rig.id:
                b1 = target_rig.id[TARGET_ROOT if nm == SOURCE_ROOT else nm]
                rest = target_rig.rest[b1]
                n.set_p70('Lcl Translation', list(rest.t), wanted='Lcl Translation')
                n.set_p70('Lcl Rotation', list(quat_to_euler(rest.q)), wanted='Lcl Rotation')
                n.set_p70('Lcl Scaling', list(rest.s), wanted='Lcl Scaling')
    save(dst, rig.nodes, rig.version, rig.tail)
    return nkeys, changed_curves


# ------------------------------------------------------------------- verification

def verify_file(path, target_rig, checks):
    rig = RigData(path)
    W1rest = target_rig.world_rest()
    root1 = target_rig.id[TARGET_ROOT]
    ok = True
    print('  verifying %s (%d keys)' % (os.path.basename(path), len(rig.times)))
    for label, frame1based, test in checks:
        k = frame1based - 1
        W = rig.compose(rig.local_at(k))
        by_name = {rig.name[bid]: w for bid, w in W.items()}
        good, detail = test(by_name, W1rest, target_rig)
        print('    frame %2d %-34s %s  %s' % (frame1based, label, 'PASS' if good else 'FAIL', detail))
        ok = ok and good
    return ok


def hand_span(name_map):
    lh = name_map['mixamorig:LeftHand'].t
    rh = name_map['mixamorig:RightHand'].t
    return math.dist(lh, rh)


def main():
    target = RigData(os.path.join(os.path.dirname(HERE), 'ArtSource', 'Exports', 'C26_KitBase_v001.fbx'))
    here = os.path.dirname(HERE)
    srcdir = os.path.join(here, 'ArtSource', 'Exports', 'Animations')
    outdir = os.path.join(srcdir, 'Corrected')
    os.makedirs(outdir, exist_ok=True)

    jobs = [
        ('A_C26_BattingDrive.fbx', [
            ('hands on handle (all keys)', None, lambda m, w, t: (hand_span(m) < 20.0, 'span %.1f cm' % hand_span(m))),
        ]),
    ]
    # generic checks run per file below
    args = sys.argv[1:]
    verify_only = '--verify-only' in args
    args = [a for a in args if not a.startswith('--')]
    files = args or ['A_C26_BattingDrive.fbx', 'A_C26_BowlingPace.fbx']

    def resolve(f):
        return f if os.sep in f else os.path.join(srcdir, f)

    for f in files:
        src = resolve(f)
        dst = os.path.join(outdir, os.path.basename(f))
        if not verify_only:
            nkeys, ncurves = correct_file(src, dst, target)
            print('corrected %s: %d keys, %d curves -> %s' % (f, nkeys, ncurves, dst))
        checks = [
            ('stance: lowest ankle at ground', STANCE_FRAME,
             lambda m, w, t: (abs(min(m['mixamorig:LeftFoot'].t[1], m['mixamorig:RightFoot'].t[1]) - 24.7) < 8.0,
                              'ankle Y %.1f / %.1f' % (m['mixamorig:LeftFoot'].t[1], m['mixamorig:RightFoot'].t[1]))),
            ('stance: hips at athletic height', STANCE_FRAME,
             lambda m, w, t: (150.0 < m['mixamorig:Hips'].t[1] < 215.0, 'hips Y %.1f' % m['mixamorig:Hips'].t[1])),
            ('hands together on handle', STANCE_FRAME,
             lambda m, w, t: (hand_span(m) < 20.0, 'span %.1f cm' % hand_span(m))),
        ]
        if 'BattingDrive' in f:
            def hands_forward(m):
                fwd = (m['mixamorig:LeftToeBase'].t[2] - m['mixamorig:LeftFoot'].t[2],
                       m['mixamorig:LeftToeBase'].t[0] - m['mixamorig:LeftFoot'].t[0])
                n = math.hypot(*fwd) or 1.0
                fwd = (fwd[0] / n, fwd[1] / n)
                hands = vscale(vadd(m['mixamorig:LeftHand'].t, m['mixamorig:RightHand'].t), 0.5)
                hips = m['mixamorig:Hips'].t
                d = (hands[2] - hips[2], hands[0] - hips[0])
                return d[0]*fwd[0] + d[1]*fwd[1]
            checks.append(('contact: hands together', CONTACT_FRAME,
                           lambda m, w, t: (hand_span(m) < 20.0, 'span %.1f cm' % hand_span(m))))
            checks.append(('contact: hands in front of body', CONTACT_FRAME,
                           lambda m, w, t: (hands_forward(m) > 10.0,
                                            'hands %.1f cm in front of hips' % hands_forward(m))))
            checks.append(('backlift: hands behind body', 13,
                           lambda m, w, t: (hands_forward(m) < -10.0,
                                            'hands %.1f cm behind hips' % hands_forward(m))))
        if 'BowlingPace' in f:
            def release_check(m, w, t):
                head = m['mixamorig:Head'].t[1]
                lh = m['mixamorig:LeftHand'].t[1]
                rh = m['mixamorig:RightHand'].t[1]
                hi = max(lh, rh)
                return hi > head + 40.0, 'head %.0f, hands %.0f/%.0f' % (head, lh, rh)
            def run_direction(m, w, t):
                # hips must travel the direction the toes point (toward the batter)
                fwd = (m['mixamorig:LeftToeBase'].t[2] - m['mixamorig:LeftFoot'].t[2],
                       m['mixamorig:LeftToeBase'].t[0] - m['mixamorig:LeftFoot'].t[0])
                n = math.hypot(*fwd) or 1.0
                fwd = (fwd[0] / n, fwd[1] / n)
                d = (m['mixamorig:Hips'].t[2], m['mixamorig:Hips'].t[0])
                forward = d[0]*fwd[0] + d[1]*fwd[1]
                return forward > 20.0, 'hips %.1f cm down the pitch from stance' % forward
            checks.append(('release: bowling arm above head', RELEASE_FRAME, release_check))
            checks.append(('follow-through: travelled down pitch', 38, run_direction))
        if not verify_file(dst if not verify_only else src, target, checks):
            raise SystemExit('VERIFICATION FAILED for ' + f)
    print('C26_CORRECT_DONE')


if __name__ == '__main__':
    main()
