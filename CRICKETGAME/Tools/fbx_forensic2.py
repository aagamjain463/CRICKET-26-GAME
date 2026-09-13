#!/usr/bin/env python3
"""Forensic pass 2: reconstruct both rigs' rest world poses and verify
(a) both are the same T-pose shape (proportions) modulo scale,
(b) the exact hierarchy,
(c) the bowling-arm position at the release frame, evaluated in the anim FBX's own frame.

Also determines the FBX Euler order empirically: the order that makes v001's
near-identity locals compose into a symmetric arms-out T-pose is the right one.
"""
import math
import sys

sys.path.insert(0, 'Tools')
from fbxread import load, get_objects, bone_models, connections, curve_nodes, curves


def clean(name):
    return name.replace('\x00\x01Model', '').replace('\x00\x01', '')


# ------------------------------------------------------------------ quaternion utils
def euler_to_quat(rx, ry, rz, order='zyx'):
    """FBX eXYZ applied to column vectors: R = Rz*Ry*Rx  (order='zyx' multiply).
    The alternative is R = Rx*Ry*Rz (order='xyz'). Quaternions are (w,x,y,z)."""
    def axis_quat(idx, deg):
        h = math.radians(deg) / 2.0
        q = [math.cos(h), 0.0, 0.0, 0.0]
        q[1 + idx] = math.sin(h)
        return tuple(q)
    qx = axis_quat(0, rx)
    qy = axis_quat(1, ry)
    qz = axis_quat(2, rz)
    if order == 'zyx':
        q = qmul(qmul(qz, qy), qx)
    else:
        q = qmul(qmul(qx, qy), qz)
    return q


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
    qv = (0.0, ) + v
    return qmul(qmul(q, qv), qconj(q))[1:]


def qnorm(q):
    n = math.sqrt(sum(c*c for c in q)) or 1.0
    return tuple(c / n for c in q)


def qdot(a, b):
    d = sum(x*y for x, y in zip(a, b))
    return min(1.0, max(-1.0, d))


class Xform:
    __slots__ = ('t', 'q')

    def __init__(self, t=(0, 0, 0), q=(1, 0, 0, 0)):
        self.t = tuple(t)
        self.q = qnorm(q)

    def __mul__(self, other):
        return Xform(tuple(a+b for a, b in zip(self.t, qrot(self.q, other.t))),
                     qmul(self.q, other.q))

    def inverse(self):
        ci = qconj(self.q)
        return Xform((-x for x in qrot(ci, self.t)), ci)

    def apply(self, p):
        return tuple(a+b for a, b in zip(self.t, qrot(self.q, p)))


def local_from_model(m):
    t = m.p70('Lcl Translation')
    r = m.p70('Lcl Rotation')
    s = m.p70('Lcl Scaling')
    t = tuple(t) if t else (0, 0, 0)
    r = tuple(r) if r else (0, 0, 0)
    s = tuple(s) if s else (1, 1, 1)
    return t, r, s


class Rig:
    def __init__(self, path, order='zyx'):
        self.path = path
        self.order = order
        nodes, ver, tail = load(path)
        self.nodes = nodes
        obj, conn = get_objects(nodes)
        self.obj = obj
        self.conn = conn
        self.bones = bone_models(obj)
        parent_of, _ = connections(conn)
        # a bone's parent: the dst MODEL of its OO link (dst may also be a
        # NodeAttribute; only Models count)
        self.parent = {}
        for bid, m in self.bones.items():
            for dst in parent_of.get(bid, []):
                if dst in self.bones and dst != bid:
                    self.parent[bid] = dst
        self.name = {bid: clean(m.prop(1)) for bid, m in self.bones.items()}
        self.id = {v: k for k, v in self.name.items()}
        # rest locals
        self.rest_local = {}
        for bid, m in self.bones.items():
            t, r, s = local_from_model(m)
            self.rest_local[bid] = Xform(t, euler_to_quat(*r, order=order))
        # children order for composition
        self.children = {}
        for bid in self.bones:
            p = self.parent.get(bid)
            if p:
                self.children.setdefault(p, []).append(bid)

    def world_rest(self):
        out = {}
        def rec(bid, xform):
            out[bid] = xform * self.rest_local[bid]
            for c in self.children.get(bid, []):
                rec(c, out[bid])
        for bid in self.bones:
            if bid not in self.parent:
                rec(bid, Xform())
        return out


def armature_prefix(rig):
    for bid, nm in rig.name.items():
        if nm in ('Armature', 'Armature.001'):
            return bid
    return None


def main():
    for order in ('zyx', 'xyz'):
        v1 = Rig('ArtSource/Exports/C26_KitBase_v001.fbx', order=order)
        w1 = v1.world_rest()
        hip = v1.id.get('mixamorig:Hips')
        lhand = v1.id.get('mixamorig:LeftHand')
        rhand = v1.id.get('mixamorig:RightHand')
        head = v1.id.get('mixamorig:Head')
        print('order=%s v001 Hips=%s LHand=%s RHand=%s' % (
            order,
            tuple(round(c, 2) for c in w1[hip].t),
            tuple(round(c, 2) for c in w1[lhand].t),
            tuple(round(c, 2) for c in w1[rhand].t)))

    # choose the order that gives a T-pose: hands symmetric, spread along X, ~shoulder height
    v1 = Rig('ArtSource/Exports/C26_KitBase_v001.fbx', order='zyx')
    w1 = v1.world_rest()
    hip = v1.id.get('mixamorig:Hips')
    print('\nv001 sample rest-world positions (order zyx):')
    for nm in ['mixamorig:Hips', 'mixamorig:Head', 'mixamorig:LeftArm',
               'mixamorig:LeftHand', 'mixamorig:RightHand',
               'mixamorig:LeftFoot', 'mixamorig:RightFoot', 'mixamorig:LeftToeBase']:
        bid = v1.id.get(nm)
        if bid is None:
            print(' missing', nm)
            continue
        print('  %-24s %s' % (nm.replace('mixamorig:', ''), tuple(round(c, 2) for c in w1[bid].t)))

    v2 = Rig('ArtSource/Exports/Animations/A_C26_BowlingPace.fbx', order='zyx')
    w2 = v2.world_rest()
    print('\nanim FBX rest-world positions:')
    for nm in ['mixamorig:Hips', 'mixamorig:Head', 'mixamorig:LeftArm',
               'mixamorig:LeftHand', 'mixamorig:RightHand',
               'mixamorig:LeftFoot', 'mixamorig:RightFoot']:
        bid = v2.id.get(nm)
        if bid is None:
            print(' missing', nm)
            continue
        print('  %-24s %s' % (nm.replace('mixamorig:', ''), tuple(round(c, 4) for c in w2[bid].t)))

    # scale check: v001 (cm giant) vs v2 (m) -> compare ratios of bone lengths
    print('\nproportion check (bone world offsets, v001_cm / v2_m):')
    scale_refs = ['mixamorig:LeftArm', 'mixamorig:LeftForeArm', 'mixamorig:LeftUpLeg',
                  'mixamorig:LeftLeg', 'mixamorig:Spine', 'mixamorig:Spine1', 'mixamorig:Spine2']
    for nm in scale_refs:
        p1 = w1[v1.id[nm]].t
        # parent
        pid1 = v1.parent.get(v1.id[nm])
        p0 = w1[pid1].t if pid1 else (0, 0, 0)
        len1 = math.dist(p0, p1)
        p2 = w2[v2.id[nm]].t
        pid2 = v2.parent.get(v2.id[nm])
        q0 = w2[pid2].t if pid2 else (0, 0, 0)
        len2 = math.dist(q0, p2)
        print('  %-24s v001=%8.2fcm v2=%7.4fm ratio=%6.3f' % (nm.replace('mixamorig:', ''), len1, len2, len1/len2 if len2 else 0))


if __name__ == '__main__':
    main()
