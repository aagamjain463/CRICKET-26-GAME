"""Rebind every hero cricketer body onto the production rig with transferred Mixamo weights.

WHY THIS EXISTS
---------------
The ten `SK_Cricketer_Hero*.fbx` bodies are Sketchfab cricketer scans that were parented to the
67-bone Mixamo `Armature` with Blender's *automatic weights*. Automatic weights failed on them,
and not subtly -- measured on `SK_Cricketer_HeroBatter.fbx`, the dominant influence per vertex was:

    mixamorig:Spine        3084 verts spanning z=[0.155, 1.331]   <- ankle height to chest
    mixamorig:LeftEye      1799 verts spanning z=[1.473, 1.774]   <- the entire head
    mixamorig:RightToe_End 1196 verts spanning z=[0.001, 0.784]   <- the entire lower leg
    mixamorig:Hips         1491 verts spanning z=[0.898, 1.325]   <- the chest
    mixamorig:LeftFoot        7 verts

and `LeftArm`, `LeftForeArm`, `LeftHand`, `RightArm`, `RightForeArm`, `RightHand` owned *no*
vertices at all. Only 24 of 67 bones had a vertex group. That is why the players rendered as a
torso standing in a hole with rigid arms: the legs were being driven by a toe bone and a spine
bone, so posing the knees dragged the whole lower body through the turf, and the arms could not
move because nothing in them was weighted to an arm.

The fix needs no new art. Sitting in the same `.blend` as each Sketchfab body are the original
Mixamo pieces -- `Body`, `Tops`, `Bottoms`, `Shoes` -- carrying correct, hand-authored Mixamo
weights for all 67 bones. This script joins those into one donor surface and transfers its weights
onto the hero body by nearest-surface interpolation, which is the standard way to re-skin a new
mesh against a rig that already has a good bind.

IT ALSO UNIFIES THE SKELETON
----------------------------
Each hero FBX shipped its own copy of the armature, so `Tools/ImportHeroSkeletal.py` gave each one
its own `USkeleton` asset. That breaks animation outright: `AC26Athlete::ApplyAuthoredClip` samples
a clip by *bone index* into the bound mesh's bone array, so a clip authored against the KitBase
skeleton applied to a differently-ordered skeleton drives the wrong bones. Every mesh here is
therefore re-parented to the single `Armature` out of `C26_KitBase_v002.blend` -- same object, same
bone order, same rest pose -- so all ten import onto one skeleton and one clip library drives every
role with no retargeting.

Each body is uniformly scaled onto that shared rig first (Kabsch fit over the common bone heads),
so a 1.74 m keeper and a 1.83 m fielder both sit on it without limb stretch.

GOTCHAS WORTH KEEPING
---------------------
- `C26_KitBase_v002.blend` keeps its objects in a collection that is **not linked into the scene**.
  Under `--background` nothing sees them until they are linked, so every object this script touches
  is linked explicitly first.
- Vertex groups have to exist on the destination before a Data Transfer with
  `layers_vgroup_select_dst='NAME'` can write into them; `datalayout_transfer` creates them, but
  only for groups that exist on the source, which is why the donor is given an (empty) group for
  every bone before the join.
- Blender's FBX exporter writes the *evaluated* mesh, so the Data Transfer modifier is applied
  rather than left live -- a live modifier referencing a deleted donor exports as an unweighted
  mesh with no error.

Run:
    /Applications/Blender.app/Contents/MacOS/Blender -b -noaudio \
        --python ArtSource/Blender/Characters/c26_reskin_heroes.py
"""

import json
import os
import sys

import bpy
import mathutils

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
KITBASE = os.path.join(HERE, 'C26_KitBase_v002.blend')
SRC = os.path.join(ROOT, 'ArtSource', 'Exports', 'PlayersSkeletal')

# The Mixamo pieces that may carry trustworthy weights, in preference order. Only `Body` survives
# the degeneracy check below in `C26_KitBase_v002.blend` -- `Tops`, `Bottoms` and `Shoes` are all
# collapsed to a 3 mm blob at the rig's origin in world space, and joining them into the donor is
# what made the first run of this script fail: for most of a hero body the *nearest surface* was
# then that blob, so arm and hand vertices came back with hip and neck weights. `Body` is the bare
# Mixamo skin, spans the full T-pose to +/-0.89 m at the fingertips and carries 64 of the 67 bones,
# so it is a complete donor on its own. `Hair` and `Eyes` stay out regardless: their weights are
# single-bone and would flood a destination scalp with one rigid assignment.
DONORS = ('Body', 'Tops', 'Bottoms', 'Shoes')

# A donor piece has to be a real surface in the rig's own space. Anything whose world bounding box
# is smaller than this is a collapsed object, not geometry, and silently poisons the transfer.
MIN_DONOR_SPAN = 0.25

ROLES = (
    'SK_Cricketer_HeroBatter',
    'SK_Cricketer_HeroBowler',
    'SK_Cricketer_HeroKeeper',
    'SK_Cricketer_HeroUmpire',
    'SK_Cricketer_HeroFielder01',
    'SK_Cricketer_HeroFielder02',
    'SK_Cricketer_HeroFielder03',
    'SK_Cricketer_HeroFielder04',
    'SK_Cricketer_HeroFielder05',
    'SK_Cricketer_HeroFielder06',
)

# Anatomical acceptance gate, in metres above the ground, as a fraction of the body's own height.
# A bone whose dominant vertices sit outside its band did not get sane weights, and shipping that
# is what produced the torso-in-a-hole. Bands are generous on purpose: they catch the failure mode
# this script exists for (a limb owned by a bone at the other end of the body), not tuning.
BANDS = {
    'LeftFoot': (0.00, 0.22), 'RightFoot': (0.00, 0.22),
    'LeftLeg': (0.04, 0.35), 'RightLeg': (0.04, 0.35),
    'LeftUpLeg': (0.28, 0.62), 'RightUpLeg': (0.28, 0.62),
    'Hips': (0.40, 0.68),
    'Spine1': (0.55, 0.90), 'Spine2': (0.60, 0.95),
    'Neck': (0.75, 0.98), 'Head': (0.78, 1.05),
    'LeftArm': (0.62, 0.92), 'RightArm': (0.62, 0.92),
    'LeftForeArm': (0.55, 0.92), 'RightForeArm': (0.55, 0.92),
    'LeftHand': (0.45, 0.92), 'RightHand': (0.45, 0.92),
}


def log(*a):
    print('C26_RESKIN', *a)
    sys.stdout.flush()


def link(obj):
    """Put an object in the active scene. KitBase keeps everything in an unlinked collection."""
    if obj.name not in bpy.context.scene.collection.objects:
        try:
            bpy.context.scene.collection.objects.link(obj)
        except RuntimeError:
            pass
    return obj


def select_only(objs, active=None):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or (objs[0] if objs else None)


def bare(name):
    """Bone name without the Mixamo namespace, which survives FBX round-trips as `_` or `:`."""
    return name.replace('mixamorig:', '').replace('mixamorig_', '')


def fit_transform(src_arm, dst_arm):
    """Similarity transform (uniform scale + rotation + translation) taking src rig onto dst rig.

    Both armatures are the same Mixamo skeleton at different scales and, after an FBX round trip,
    possibly a different up-axis convention. Kabsch over the shared bone heads recovers the whole
    transform from the rigs themselves rather than trusting either file's export settings.
    """
    src = {bare(b.name): src_arm.matrix_world @ b.head_local for b in src_arm.data.bones}
    dst = {bare(b.name): dst_arm.matrix_world @ b.head_local for b in dst_arm.data.bones}
    keys = sorted(set(src) & set(dst))
    if len(keys) < 8:
        raise RuntimeError('only %d shared bones between rigs' % len(keys))

    p = [src[k] for k in keys]
    q = [dst[k] for k in keys]
    cp = sum(p, mathutils.Vector()) / len(p)
    cq = sum(q, mathutils.Vector()) / len(q)
    p = [v - cp for v in p]
    q = [v - cq for v in q]

    # Uniform scale from the ratio of spreads; the rigs are the same shape, so this is exact
    # enough that the rotation solve below sees a near-orthogonal problem.
    sp = sum(v.length_squared for v in p)
    sq = sum(v.length_squared for v in q)
    scale = (sq / sp) ** 0.5 if sp > 1e-12 else 1.0

    h = mathutils.Matrix(((0, 0, 0), (0, 0, 0), (0, 0, 0)))
    for a, b in zip(p, q):
        for i in range(3):
            for j in range(3):
                h[i][j] += a[i] * b[j]

    # mathutils has no SVD, so the rotation is recovered by iterated orthogonal projection
    # (Higham): repeatedly averaging a matrix with its own inverse-transpose converges on the
    # nearest orthogonal matrix, which for two copies of one rig is a handful of steps.
    r = h.transposed()
    for _ in range(48):
        try:
            r = (r + r.inverted().transposed()) * 0.5
        except ValueError:
            break
    r.normalize()

    m = mathutils.Matrix.Identity(4)
    for i in range(3):
        for j in range(3):
            m[i][j] = r[i][j] * scale
    t = cq - m.to_3x3() @ cp
    m[0][3], m[1][3], m[2][3] = t.x, t.y, t.z

    err = max((m @ src[k] - dst[k]).length for k in keys)
    return m, scale, err


def build_donor(arm):
    """One joined mesh carrying the Mixamo weights, in the space of `arm`."""
    copies = []
    for name in DONORS:
        o = bpy.data.objects.get(name)
        if not o or o.type != 'MESH' or not len(o.data.vertices):
            continue
        co = [o.matrix_world @ v.co for v in o.data.vertices]
        span = max(max(c[i] for c in co) - min(c[i] for c in co) for i in range(3))
        if span < MIN_DONOR_SPAN:
            log('donor', name, 'REJECTED: collapsed to %.4f m in rig space' % span)
            continue
        log('donor', name, 'span %.3f m, %d verts' % (span, len(co)))
        c = o.copy()
        c.data = o.data.copy()
        c.name = 'C26_Donor_' + name
        c.modifiers.clear()
        link(c)
        copies.append(c)
    if not copies:
        raise RuntimeError('no donor meshes found in KitBase')

    # Every bone gets a group, even where the donor has no weight for it, so the Data Transfer
    # creates the matching group on the destination instead of silently skipping the bone.
    names = {bare(b.name): b.name for b in arm.data.bones}
    for c in copies:
        have = {bare(g.name) for g in c.vertex_groups}
        for short, full in names.items():
            if short not in have:
                c.vertex_groups.new(name=full)

    select_only(copies, copies[0])
    if len(copies) > 1:
        bpy.ops.object.join()
    donor = bpy.context.view_layer.objects.active
    donor.name = 'C26_WeightDonor'
    return donor


def aim_bone(arm, name, direction):
    """Point a pose bone along a world-space direction, keeping its head where it is.

    Written against `pose_bone.matrix` (armature space) rather than euler channels so it is
    independent of the rig's bone roll and axis convention -- Mixamo's arm bones run down +Y with
    an arbitrary roll, and anything that assumes otherwise lands the arm in a different place on
    each of the ten bodies.
    """
    pb = arm.pose.bones.get(name)
    if not pb:
        return
    have = (pb.tail - pb.head)
    if have.length < 1e-6:
        return
    rot = have.normalized().rotation_difference(direction.normalized())
    m = pb.matrix.copy()
    head = m.translation.copy()
    m = rot.to_matrix().to_4x4() @ m
    m.translation = head
    pb.matrix = m
    # Pose bone matrices are evaluated lazily, and a child read before this lands sees the old
    # parent, so the chain has to be flushed between joints.
    bpy.context.view_layer.update()


def pose_arms(arm, out_x, fwd_y):
    """Hang both arms at the body's sides. `out_x` is how far the arm stands off the ribs."""
    for pb in arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    bpy.context.view_layer.update()
    for side in ('Left', 'Right'):
        name = {bare(b.name): b.name for b in arm.data.bones}
        sign = 1.0 if arm.data.bones[name[side + 'Arm']].head_local.x > 0 else -1.0
        upper = mathutils.Vector((sign * out_x, 0.0, -1.0))
        lower = mathutils.Vector((sign * out_x * 0.6, fwd_y, -1.0))
        aim_bone(arm, name[side + 'Arm'], upper)
        aim_bone(arm, name[side + 'ForeArm'], lower)
        aim_bone(arm, name[side + 'Hand'], lower)


def surface_error(donor, samples):
    """Mean distance from a sample of hero vertices to the posed donor surface."""
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    ev = donor.evaluated_get(dg)
    mesh = ev.to_mesh()
    tree = BVHTree.FromPolygons(
        [ev.matrix_world @ v.co for v in mesh.vertices],
        [tuple(p.vertices) for p in mesh.polygons], all_triangles=False)
    total = 0.0
    for p in samples:
        hit = tree.find_nearest(p)
        total += hit[3] if hit and hit[3] is not None else 1.0
    ev.to_mesh_clear()
    return total / max(1, len(samples))


def match_pose(arm, donor, hero):
    """Find the arms-down pose of the shared rig that best covers this hero body.

    The Sketchfab cricketers stand with their arms at their sides; the rig, and the Mixamo donor
    bound to it, are a T-pose. Nearest-surface weight transfer across that gap is what produced
    hero bodies whose arms owned no vertices at all -- an arm vertex's nearest donor surface was
    the ribcage, so it came back with a spine weight and the arm could not move.

    Rather than infer the arm axis from the scan's geometry (thin, noisy, and different on each
    body), this searches a small grid of hanging poses and keeps whichever one puts the donor
    surface closest to the hero surface overall. The search is over the whole body, so a pose that
    fixes the arms by wrecking the torso cannot win.
    """
    verts = [hero.matrix_world @ v.co for v in hero.data.vertices]
    samples = verts[::max(1, len(verts) // 1200)]
    arm.data.pose_position = 'POSE'
    best = None
    for out_x in (0.02, 0.08, 0.15, 0.24):
        for fwd_y in (-0.10, 0.0, 0.12):
            pose_arms(arm, out_x, fwd_y)
            err = surface_error(donor, samples)
            if best is None or err < best[0]:
                best = (err, out_x, fwd_y)
    pose_arms(arm, best[1], best[2])
    return best


def unpose(hero, arm):
    """Move the hero geometry from the matched pose back onto the rig's rest pose.

    Linear blend skinning sends a rest vertex to `sum(w_b * M_b) @ v`. The hero body arrived
    already in the posed shape, so the rest shape is that sum inverted and applied -- which is what
    lets every body ship bound to one shared T-pose skeleton instead of ten private ones. Without
    this the mesh would be yanked from arms-down into the T-pose the moment it was bound.
    """
    world = arm.matrix_world
    inv_world = world.inverted()
    skin = {}
    for b in arm.data.bones:
        pb = arm.pose.bones.get(b.name)
        if pb:
            skin[b.name] = world @ pb.matrix @ b.matrix_local.inverted() @ inv_world

    group_bone = {g.index: g.name for g in hero.vertex_groups}
    moved = 0
    for v in hero.data.vertices:
        acc = mathutils.Matrix(((0,) * 4,) * 4)
        total = 0.0
        for g in v.groups:
            m = skin.get(group_bone.get(g.group))
            if m is None or g.weight <= 0.0:
                continue
            for i in range(4):
                for j in range(4):
                    acc[i][j] += m[i][j] * g.weight
            total += g.weight
        if total <= 1e-6:
            continue
        for i in range(4):
            for j in range(4):
                acc[i][j] /= total
        try:
            rest = acc.inverted() @ v.co
        except ValueError:
            continue
        if (rest - v.co).length > 1e-5:
            moved += 1
        v.co = rest
    return moved


def dominant_bands(mesh):
    """Per-group vertex count and world-Z span of the vertices that group dominates."""
    out = {}
    mw = mesh.matrix_world
    for v in mesh.data.vertices:
        if not v.groups:
            e = out.setdefault('__UNWEIGHTED__', [0, 9e9, -9e9])
            e[0] += 1
            continue
        g = max(v.groups, key=lambda x: x.weight)
        if g.weight <= 0.0:
            continue
        name = bare(mesh.vertex_groups[g.group].name)
        z = (mw @ v.co).z
        e = out.setdefault(name, [0, 9e9, -9e9])
        e[0] += 1
        e[1] = min(e[1], z)
        e[2] = max(e[2], z)
    return out


def check(mesh, label):
    """Gate the result. Returns (ok, report) -- a failure names the bone and what it grabbed."""
    bands = dominant_bands(mesh)
    height = max((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
    problems = []
    if bands.get('__UNWEIGHTED__', [0])[0]:
        problems.append('%d unweighted verts' % bands['__UNWEIGHTED__'][0])
    for bone, (lo, hi) in BANDS.items():
        e = bands.get(bone)
        if not e or e[0] < 4:
            problems.append('%s owns %d verts' % (bone, e[0] if e else 0))
            continue
        # Centre of the owned span is what has to sit in the band; the span itself legitimately
        # runs past it, because a deltoid reaches down the arm and a hip reaches into the thigh.
        mid = (e[1] + e[2]) * 0.5 / height
        if not (lo <= mid <= hi):
            problems.append('%s centred at %.2fH, wanted %.2f-%.2f' % (bone, mid, lo, hi))
    return (not problems), problems


def reskin(role):
    src = os.path.join(SRC, role + '.fbx')
    if not os.path.exists(src):
        log('SKIP', role, 'no source FBX')
        return None

    bpy.ops.wm.open_mainfile(filepath=KITBASE)
    kit_arm = bpy.data.objects.get('Armature')
    if not kit_arm or kit_arm.type != 'ARMATURE':
        raise RuntimeError('KitBase has no Armature')
    for name in ('Armature',) + DONORS:
        o = bpy.data.objects.get(name)
        if o:
            link(o)
    # A stale pose on the shared rig would bake into every export. Rest position guarantees the
    # armature the heroes bind against is the rest pose the clips were authored on.
    kit_arm.data.pose_position = 'REST'
    # World matrices of objects that were not in the scene are stale until the depsgraph catches
    # up, and every alignment decision below is made from world space.
    bpy.context.view_layer.update()

    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=src)
    fresh = [o for o in bpy.data.objects if o not in before]
    hero = max((o for o in fresh if o.type == 'MESH'), key=lambda o: len(o.data.vertices), default=None)
    hero_arm = next((o for o in fresh if o.type == 'ARMATURE'), None)
    if not hero or not hero_arm:
        raise RuntimeError('%s: expected a mesh and an armature' % role)
    for o in fresh:
        link(o)

    fit, scale, err = fit_transform(hero_arm, kit_arm)
    log(role, 'rig fit scale=%.4f residual=%.4fm verts=%d' % (scale, err, len(hero.data.vertices)))

    # Bake the fit into the hero geometry and drop its private armature, so what is left is one
    # body sitting on the shared rig in the shared rest pose.
    hero.modifiers.clear()
    hero.parent = None
    hero.matrix_world = fit @ hero.matrix_world
    select_only([hero], hero)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in fresh:
        if o is not hero:
            bpy.data.objects.remove(o, do_unlink=True)

    donor = build_donor(kit_arm)
    # The donor has to wear the same pose as the body it is donating to before any nearest-surface
    # question is asked of it. It gets its armature back for exactly that reason.
    donor_mod = donor.modifiers.new(name='Armature', type='ARMATURE')
    donor_mod.object = kit_arm
    fit_err, out_x, fwd_y = match_pose(kit_arm, donor, hero)
    log(role, 'arm pose out=%.2f fwd=%.2f surface error %.1f mm' % (out_x, fwd_y, fit_err * 1000.0))

    hero.vertex_groups.clear()
    select_only([hero], hero)
    mod = hero.modifiers.new(name='C26WeightTransfer', type='DATA_TRANSFER')
    mod.object = donor
    mod.use_vert_data = True
    mod.data_types_verts = {'VGROUP_WEIGHTS'}
    # Nearest interpolated face point, not nearest vertex: the donor is a 7k-vertex Mixamo body and
    # the hero is a 17k-vertex scan, so per-vertex nearest would quantise the weights into visible
    # patches across the shoulder and knee.
    mod.vert_mapping = 'POLYINTERP_NEAREST'
    mod.layers_vgroup_select_src = 'ALL'
    mod.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.datalayout_transfer(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)

    # The hero scan is 2.5x denser than the donor and has interior shells the donor does not, so a
    # minority of vertices resolve to a surface across a gap -- a sliver of armpit taking chest
    # weights, the inside of a calf taking hip. Smoothing over the mesh's own connectivity pulls
    # those back toward their neighbours, which is what removes the speckled tearing that survives
    # an otherwise correct transfer.
    # `vertex_group_smooth` polls for an active group even in ALL mode, and a freshly transferred
    # mesh has no active index.
    if hero.vertex_groups:
        hero.vertex_groups.active_index = 0
    try:
        bpy.ops.object.vertex_group_smooth(group_select_mode='ALL', factor=0.5, repeat=4)
    except RuntimeError as exc:
        log(role, 'smoothing skipped:', exc)
    # Four influences is what the engine's default GPU skinning path expects, and normalising
    # after the limit is what keeps a vertex from shrinking toward the origin once its fifth and
    # sixth influences are dropped.
    bpy.ops.object.vertex_group_limit_total(limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)

    moved = unpose(hero, kit_arm)
    span = (max(v.co.x for v in hero.data.vertices) - min(v.co.x for v in hero.data.vertices))
    log(role, 'unposed %d verts, arm span now %.2f m' % (moved, span))

    bpy.data.objects.remove(donor, do_unlink=True)
    for pb in kit_arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    kit_arm.data.pose_position = 'REST'
    bpy.context.view_layer.update()

    hero.parent = kit_arm
    hero.matrix_parent_inverse = kit_arm.matrix_world.inverted()
    arm_mod = hero.modifiers.new(name='Armature', type='ARMATURE')
    arm_mod.object = kit_arm

    ok, problems = check(hero, role)
    # The donor T-pose measures 1.785 m fingertip to fingertip. A hero that comes out of the
    # un-pose much narrower than that still has its arms at its sides, which means the weights it
    # was given do not actually drive the arms -- the exact failure this whole stage exists to
    # catch, and one that is invisible in a bind-pose screenshot.
    if span < 1.35:
        problems.append('arm span %.2f m: un-pose did not reach the T-pose' % span)
        ok = False
    for p in problems:
        log(role, 'WEIGHT WARNING:', p)

    hero.name = role
    select_only([kit_arm, hero], kit_arm)
    out = os.path.join(SRC, role + '.fbx')
    bpy.ops.export_scene.fbx(
        filepath=out,
        use_selection=True,
        add_leaf_bones=False,
        bake_anim=False,
        object_types={'ARMATURE', 'MESH'},
        mesh_smooth_type='FACE',
        use_mesh_modifiers=False,
        primary_bone_axis='Y',
        secondary_bone_axis='X',
        apply_scale_options='FBX_SCALE_NONE',
        global_scale=1.0,
    )
    log(role, 'exported', 'OK' if ok else 'WITH WARNINGS', out)
    return {'role': role, 'ok': ok, 'problems': problems,
            'bands': {k: [v[0], round(v[1], 3), round(v[2], 3)]
                      for k, v in sorted(dominant_bands(hero).items())}}


def main():
    # `-- <substring>` restricts the run to matching roles, so a change can be tried on one body
    # before it is spent on all ten.
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    report = []
    for role in ROLES:
        if only and not any(o.lower() in role.lower() for o in only):
            continue
        try:
            r = reskin(role)
            if r:
                report.append(r)
        except Exception as exc:                                  # noqa: BLE001 - report, continue
            log(role, 'FAILED', repr(exc))
            report.append({'role': role, 'ok': False, 'problems': [repr(exc)]})
    dest = os.path.join(ROOT, 'Artifacts', 'hero_reskin.json')
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, 'w') as fh:
        json.dump(report, fh, indent=2)
    good = sum(1 for r in report if r.get('ok'))
    log('DONE %d/%d clean -> %s' % (good, len(report), dest))


main()
