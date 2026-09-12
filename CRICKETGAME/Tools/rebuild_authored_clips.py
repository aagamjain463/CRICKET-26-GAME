#!/usr/bin/env python3
"""CRICKET 26 — offline rebuild of the authored cricket clips, with the facing fix.

WHY THIS EXISTS
---------------
The authored clips (ArtSource/Blender/Animation/c26_anim_author.py) were keyed
under a wrong rig-convention claim (+Y forward); the shipped rig faces armature
-Y. The stroke therefore happened through the character's back. The repair
(repair_facing() in the authoring script — conjugate spine twists by rotZ(180),
rotate IK targets by (x,y,z)->(-x,-y,z), keep pole x/z and negate pole y) is
now applied at solve time in Blender itself.

This tool re-runs THE SAME solve outside Blender, because the sandbox has no
Blender: it imports the authoring module through a mathutils shim, solves every
key with repair_facing() applied, bakes every frame at 24 fps (smoothstep
between keys, standing in for the Blender Bezier/AUTO_CLAMPED bake), and writes
the result as FBX curves into ArtSource/Exports/Animations/Solved/.

Those files are in the ORIGINAL (v2) rig frame — identical in structure to what
a Blender bake + export would produce. Run Tools/correct_authored_anim.py on
them afterwards to express them on the shipped (v1) skeleton.

Run:  python3 Tools/rebuild_authored_clips.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from c26_mathutils_shim import Vector, Quaternion, Matrix
from fbxread import load, save
from correct_authored_anim import RigData, quat_to_euler, clean

AUTHORING = os.path.join(ROOT, 'ArtSource', 'Blender', 'Animation', 'c26_anim_author.py')
SRC_DIR = os.path.join(ROOT, 'ArtSource', 'Exports', 'Animations')
OUT_DIR = os.path.join(SRC_DIR, 'Solved')


# ------------------------------------------------------------------ authoring import
def load_authoring_module():
    """Exec the Blender authoring script outside Blender. Only its key
    definitions (batting_keys/bowling_keys/spine/R/repair_facing/Rig) are
    used; its main() is stripped."""
    src = open(AUTHORING).read()
    src = src.replace('import bpy\n', '')
    src = src.replace('from mathutils import Matrix, Quaternion, Vector\n',
                      'from c26_mathutils_shim import Matrix, Quaternion, Vector\n')
    src = src.replace('\nmain()\n', '\n')
    ns = {'__name__': 'c26_anim_author', '__file__': AUTHORING}
    exec(compile(src, AUTHORING, 'exec'), ns)
    return ns


# ------------------------------------------------------------------ rig extraction
def _quat_matrix(q):
    m = Matrix.Identity(4)
    m._store_quat(q)
    return m


def _bone_len(rig, nm):
    import math
    bid = rig.id[nm]
    return math.sqrt(sum(c * c for c in rig.rest[bid].t))


class SolverRig:
    """The authoring Rig, restated over FBX-extracted rest data.

    The TRUE v2 T-pose rest is reconstructed from the shipped asset rig
    (C26_KitBase_v001.fbx): v2 is that rig uniformly scaled to metres
    (1/(ratio*100)) and re-based into the Blender armature frame (R90X), with
    identical bone rotations. The anim FBX's own Model defaults CANNOT be used:
    they hold the frame-1 POSE (the exporter wrote the current pose), not the
    rest. Validation: the reconstructed T-pose Hips lands at armature
    (x, y, 0.9348), exactly the anim file's stance Hips plus the authored
    hips_z drop of 0.115.

    apply() and two_bone() are taken VERBATIM from the authoring module, so the
    offline solve is the same solve Blender runs.
    """

    V1_PATH = os.path.join(ROOT, 'ArtSource', 'Exports', 'C26_KitBase_v001.fbx')

    def __init__(self, anim_rig, authoring):
        self.apply = authoring['Rig'].apply.__get__(self, type(self))
        self.two_bone = authoring['Rig'].two_bone
        # ---- true v2 T-pose rest, from the v1 asset rig ---------------------
        v1 = RigData(self.V1_PATH)
        W1 = v1.world_rest()
        ratio = _bone_len(v1, 'mixamorig:LeftLeg') / (_bone_len(anim_rig, 'mixamorig:LeftLeg') * 100.0)
        k = 1.0 / (ratio * 100.0)      # asset units -> v2 metres
        R90 = Quaternion(Vector((1.0, 0.0, 0.0)), 90.0)
        self.rest = {}
        for bid in v1.order:
            nm = v1.name[bid]
            if nm in ('Armature', 'Armature.001'):
                continue
            xf = W1[bid]
            m = _quat_matrix((Quaternion(R90.q) @ Quaternion(xf.q)).q)
            t = xf.t
            m.translation = (t[0]*k, -t[2]*k, t[1]*k)   # R(+90X): (x,y,z)->(x,-z,y)
            self.rest[nm] = m
        # ---- hierarchy in name space ----------------------------------------
        self.parent = {}
        for bid in anim_rig.order:
            nm = anim_rig.name[bid]
            if nm == 'Armature':
                continue
            p = anim_rig.parent.get(bid)
            pn = anim_rig.name.get(p) if p is not None else None
            self.parent[nm] = None if (pn is None or pn == 'Armature') else pn
        missing = [nm for nm in self.parent if nm not in self.rest]
        if missing:
            raise SystemExit('v1 rig missing bones: %s' % missing)
        self.children = {}
        for nm, p in self.parent.items():
            if p:
                self.children.setdefault(p, []).append(nm)
        self.order = []
        seen = set()

        def walk(nm):
            if nm in seen:
                return
            seen.add(nm)
            self.order.append(nm)
            for c in self.children.get(nm, []):
                walk(c)
        for nm in self.rest:
            if self.parent[nm] is None:
                walk(nm)
        for nm in self.rest:
            walk(nm)
        # bone lengths: distance to the nearest child origin (chain bones have
        # exactly the next chain bone at their tail)
        self._len = {}
        for nm in self.rest:
            kids = self.children.get(nm)
            if kids:
                head = self.rest[nm].to_translation()
                self._len[nm] = min((self.rest[k].to_translation() - head).length
                                    for k in kids)
            else:
                self._len[nm] = 0.10
        self.captured = {}

    def rest_len(self, name):
        return self._len.get(name, 0.10)

    def _write(self, name, base, m_pose):
        self.captured[name] = (base.copy(), m_pose.copy())


# ------------------------------------------------------------------ solve + bake
def _slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d > 0.9995:
        q = tuple(x + (y - x) * t for x, y in zip(a, b))
        n = math.sqrt(sum(c * c for c in q)) or 1.0
        return tuple(c / n for c in q)
    theta = math.acos(max(-1.0, min(1.0, d)))
    s = math.sin(theta)
    wa = math.sin((1 - t) * theta) / s
    wb = math.sin(t * theta) / s
    return tuple(x * wa + y * wb for x, y in zip(a, b))


def solve_clip(solver, keys, repair, f0, f1):
    """Return {frame: {bone: basis Matrix}} with repair_facing applied."""
    solved = {}
    for f, spec, hips, ik in keys:
        solver.captured = {}
        solver.apply(f, *repair(spec, hips, ik))
        basis = {}
        for name, (base, m_pose) in solver.captured.items():
            basis[name] = base.inverted() @ m_pose
        solved[f] = basis
    # interpolate every in-between frame
    frames = sorted(solved.keys())
    out = {}
    for f in range(f0, f1 + 1):
        if f in solved:
            out[f] = solved[f]
            continue
        i = 0
        while i + 1 < len(frames) and frames[i + 1] < f:
            i += 1
        a, b = frames[i], frames[i + 1]
        t = (f - a) / float(b - a)
        t = t * t * (3.0 - 2.0 * t)     # Bezier-ish ease, like the bake
        blended = {}
        for name in solved[a]:
            ma, mb = solved[a][name], solved[b][name]
            qa = ma.to_quaternion().q
            qb = mb.to_quaternion().q
            if sum(x * y for x, y in zip(qa, qb)) < 0:
                qb = tuple(-c for c in qb)
            qb = _slerp(qa, qb, t)
            m = _quat_matrix(qb)
            ta, tb = ma.to_translation(), mb.to_translation()
            m.translation = tuple(x + (y - x) * t for x, y in zip(ta.v, tb.v))
            blended[name] = m
        out[f] = blended
    return out


def poses_to_locals(solver, frames_basis):
    """Per frame: FBX Lcl locals (v2 frame) from armature-space basis matrices.

    The armature-space pose of a bone is base_chain @ basis, with base_chain
    rebuilt exactly like the authoring solver: parent pose @ parent rest^-1 @
    bone rest. The FBX Lcl of a bone is its armature-space transform relative
    to its parent's (for Hips: the armature node frame, which IS armature
    space, so Hips' Lcl equals its armature-space matrix).
    """
    out = {}
    for f, basis in frames_basis.items():
        arm = {}
        for name in solver.order:
            p = solver.parent[name]
            base = solver.rest[name].copy() if p is None else \
                arm[p] @ solver.rest[p].inverted() @ solver.rest[name]
            arm[name] = base @ basis[name]
        locals_ = {}
        for name in solver.order:
            p = solver.parent[name]
            locals_[name] = arm[name] if p is None else arm[p].inverted() @ arm[name]
        out[f] = locals_
    return out


# ------------------------------------------------------------------ FBX writing
def write_clip(src_path, dst_path, frames_locals):
    rig = RigData(src_path)
    changed = 0
    for cid, (bone, letter, axis, node) in rig.curve_of.items():
        nm = rig.name[bone]
        if nm == 'Armature':
            continue
        kv = node.find('KeyValueFloat') or node.find('KeyValueDouble')
        if kv is None:
            continue
        vals = kv.props[0][1]
        idx = 'XYZ'.index(axis)
        for k in range(len(vals)):
            frame = k + 1
            m = frames_locals.get(frame)
            if m is None or nm not in m:
                continue
            if letter == 'T':
                vals[k] = m[nm].to_translation()[idx]
            elif letter == 'R':
                vals[k] = quat_to_euler(m[nm].to_quaternion().q)[idx]
            else:
                vals[k] = 1.0
        changed += 1
    save(dst_path, rig.nodes, rig.version, rig.tail)
    return changed


# ------------------------------------------------------------------ sanity report
def report(solver, frames_locals, marks):
    """Armature-space landmarks at key frames, for eyeballing the repair."""
    for label, f in marks:
        loc = frames_locals.get(f)
        if loc is None:
            continue
        arm = {}
        for name in solver.order:
            p = solver.parent[name]
            # FBX local of a bone = arm[parent]^-1 @ arm[bone]; Hips' local IS
            # its armature-space matrix (the armature node's frame).
            arm[name] = loc[name] if p is None else arm[p] @ loc[name]

        def pos(n):
            return arm[n].to_translation()
        hands = tuple((pos('mixamorig:LeftHand')[i] + pos('mixamorig:RightHand')[i]) / 2
                      for i in range(3))
        hips = pos('mixamorig:Hips')
        print('    f%-3d %-10s hands=(%+.2f,%+.2f,%+.2f) hips=(%+.2f,%+.2f,%+.2f)'
              ' lfoot_y=%+.2f rfoot_y=%+.2f'
              % (f, label, hands[0], hands[1], hands[2], hips[0], hips[1], hips[2],
                 pos('mixamorig:LeftFoot')[1], pos('mixamorig:RightFoot')[1]))


def main():
    authoring = load_authoring_module()
    repair = authoring['repair_facing']
    os.makedirs(OUT_DIR, exist_ok=True)
    # New clips have no source FBX of their own yet: each shares the frame layout
    # (36 keys / contact at 23 for batting, 46 / release at 31 for bowling) and the
    # exact ANIMATED bone set of its family's base clip, so the base clip's FBX is
    # used as the structural template and every curve value is overwritten by the
    # solve.
    jobs = [
        ('A_C26_BattingDrive', authoring['batting_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('backlift', 13), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingPull', authoring['batting_pull_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('backlift', 13), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingCut', authoring['batting_cut_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('backlift', 13), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingSweep', authoring['batting_sweep_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('descend', 18), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingDefence', authoring['batting_defence_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('press', 18), ('contact', 23), ('absorb', 29)]),
        ('A_C26_BattingHook', authoring['batting_hook_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('backlift', 13), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingLoftedDrive', authoring['batting_lofted_drive_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('backlift', 13), ('contact', 23), ('follow', 29)]),
        ('A_C26_BattingGlance', authoring['batting_glance_keys'](), 'A_C26_BattingDrive',
         1, 36, [('stance', 1), ('press', 18), ('contact', 23), ('deflect', 29)]),
        ('A_C26_BowlingPace', authoring['bowling_keys'](), 'A_C26_BowlingPace',
         1, 46, [('gather', 8), ('backfoot', 20), ('release', 31), ('follow', 38)]),
        ('A_C26_BowlingOffSpin', authoring['bowling_offspin_keys'](), 'A_C26_BowlingPace',
         1, 46, [('gather', 8), ('backfoot', 20), ('release', 31), ('follow', 38)]),
        ('A_C26_BowlingLegSpin', authoring['bowling_legspin_keys'](), 'A_C26_BowlingPace',
         1, 46, [('gather', 8), ('backfoot', 20), ('release', 31), ('follow', 38)]),
    ]
    for name, keys, template, f0, f1, marks in jobs:
        src = os.path.join(SRC_DIR, template + '.fbx')
        dst = os.path.join(OUT_DIR, name + '.fbx')
        rig = RigData(src)
        solver = SolverRig(rig, authoring)
        frames_basis = solve_clip(solver, keys, repair, f0, f1)
        frames_locals = poses_to_locals(solver, frames_basis)
        changed = write_clip(src, dst, frames_locals)
        print('C26_REBUILD %s: %d keys solved, %d frames baked, %d curves'
              % (name, len(keys), f1 - f0 + 1, changed))
        print('  armature-space landmarks (forward is -Y after the repair):')
        report(solver, frames_locals, marks)
    print('C26_REBUILD_DONE')


if __name__ == '__main__':
    main()
