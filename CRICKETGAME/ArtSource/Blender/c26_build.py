"""Shared Blender mesh authoring helpers for CRICKET 26 hero assets.

Everything here is authored in CENTIMETRES because that is the unit the gameplay code, the
pitch dimensions and the athlete rig all speak. `mesh_from` divides by 100 on the way into
Blender so the saved scene is metres, which is what the FBX pipeline and Unreal expect.

Local frames are chosen to match the attach transforms already used by AC26Athlete::PlaceKit,
so imported geometry drops into the existing placement code without re-deriving it:
  bat    origin at the top of the handle, blade down -Z, hitting face +X
  helmet origin at the crown pivot, face +X, up +Z
  pad    origin mid-shin, up the shin +Z, front of the leg +X
  glove  origin at the palm, wrist->fingertip +Z, back of the hand +X
  shoe   origin at the ankle joint, toes +X, up +Z
"""
import bpy, bmesh, math, os
from mathutils import Vector

# Author in centimetres, store in metres. Measured against a real import: whatever
# UnitScaleFactor a Blender FBX declares, Unreal brings these in at x100, which is why the
# existing stadium assets are authored in metres too. Tools/ImportEquipment.py asserts the
# resulting centimetre size of every asset, so a regression here fails the import rather than
# quietly landing an athlete in equipment a hundred times the wrong size.
CM = 0.01


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.armatures):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def material(name, colour, roughness=0.7, metallic=0.0):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (*colour, 1.0)
            bsdf.inputs['Roughness'].default_value = roughness
            bsdf.inputs['Metallic'].default_value = metallic
    return mat


def mesh_from(name, verts, faces, face_mats, mats, uvs=None, smooth_angle=38.0):
    """verts in cm, faces as index tuples, face_mats parallel to faces, uvs parallel to face
    corners (list of lists of (u,v))."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([(v[0] * CM, v[1] * CM, v[2] * CM) for v in verts], [], faces)
    me.update()
    for m in mats:
        me.materials.append(m)
    for poly, idx in zip(me.polygons, face_mats):
        poly.material_index = idx
    if uvs:
        layer = me.uv_layers.new(name='UVMap')
        flat = []
        for corner in uvs:
            flat.extend(corner)
        for i, uv in enumerate(flat):
            if i < len(layer.data):
                layer.data[i].uv = uv
    obj = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    me.validate(verbose=False)
    bpy.ops.object.shade_smooth()
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(smooth_angle))
    except Exception:
        try:
            bpy.ops.object.shade_auto_smooth(angle=math.radians(smooth_angle))
        except Exception:
            pass
    obj.select_set(False)
    return obj


class Builder:
    """Accumulates verts/faces/material indices/UVs across several primitives so one object can
    carry a whole piece of equipment with clean material slots."""

    def __init__(self):
        self.v, self.f, self.m, self.uv = [], [], [], []

    def quad(self, a, b, c, d, mat, uva=(0, 0), uvb=(1, 0), uvc=(1, 1), uvd=(0, 1)):
        base = len(self.v)
        self.v.extend([a, b, c, d])
        self.f.append((base, base + 1, base + 2, base + 3))
        self.m.append(mat)
        self.uv.append([uva, uvb, uvc, uvd])

    def loft(self, rings, mat, closed=True, cap_start=None, cap_end=None, v_scale=1.0):
        """rings: list of lists of (x,y,z) in cm, every ring the same length. Winding produces
        outward normals for rings ordered counter-clockwise seen from +Z."""
        n = len(rings[0])
        base = len(self.v)
        for ring in rings:
            self.v.extend(ring)
        span = max(1, len(rings) - 1)
        for r in range(len(rings) - 1):
            for j in range(n if closed else n - 1):
                k = (j + 1) % n
                a = base + r * n + j
                b = base + r * n + k
                c = base + (r + 1) * n + k
                d = base + (r + 1) * n + j
                self.f.append((a, b, c, d))
                self.m.append(mat)
                u0, u1 = j / n, (j + 1) / n
                v0, v1 = r / span * v_scale, (r + 1) / span * v_scale
                self.uv.append([(u0, v0), (u1, v0), (u1, v1), (u0, v1)])
        for end, cap in ((0, cap_start), (len(rings) - 1, cap_end)):
            if cap is None:
                continue
            apex = len(self.v)
            self.v.append(cap)
            for j in range(n):
                k = (j + 1) % n
                a = base + end * n + j
                b = base + end * n + k
                self.f.append((a, b, apex) if end else (apex, b, a))
                self.m.append(mat)
                self.uv.append([(j / n, float(bool(end))), ((j + 1) / n, float(bool(end))), (0.5, 0.5)])

    def build(self, name, mats, smooth_angle=38.0):
        return mesh_from(name, self.v, self.f, self.m, mats, self.uv, smooth_angle)


def circle(cx, cy, cz, radius, sides, squash=1.0, phase=0.0):
    return [(cx + math.cos(2 * math.pi * j / sides + phase) * radius,
             cy + math.sin(2 * math.pi * j / sides + phase) * radius * squash, cz)
            for j in range(sides)]


def tube(path, radii, sides, up=Vector((0, 0, 1))):
    """Sweep a circular section along a polyline path (cm). Returns rings for Builder.loft."""
    rings = []
    for i, p in enumerate(path):
        p = Vector(p)
        nxt = Vector(path[min(i + 1, len(path) - 1)])
        prv = Vector(path[max(i - 1, 0)])
        axis = (nxt - prv)
        if axis.length < 1e-6:
            axis = Vector((0, 0, 1))
        axis.normalize()
        ref = up if abs(axis.dot(up)) < 0.94 else Vector((1, 0, 0))
        side = axis.cross(ref).normalized()
        other = side.cross(axis).normalized()
        ring = []
        for j in range(sides):
            a = 2 * math.pi * j / sides
            n = side * math.cos(a) + other * math.sin(a)
            ring.append(tuple(p + n * radii[i]))
        rings.append(ring)
    return rings


def report(objs):
    for o in objs:
        tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
        print('  %-26s verts %5d tris %5d dims %s slots %s' % (
            o.name, len(o.data.vertices), tris,
            [round(d / CM, 1) for d in o.dimensions],
            [m.name for m in o.data.materials]))


def export_fbx(objs, path):
    """Vertex data is already in centimetres and the exporter declares UnitScaleFactor=1.0, so
    Unreal imports these at 1:1 with no scene-unit conversion and no per-component fudge factor."""
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, apply_unit_scale=True, global_scale=1.0,
        # Identity axis passthrough: Blender (X,Y,Z) arrives in Unreal as (X,Y,Z), so the local
        # frames documented at the top of this file survive the round trip unchanged.
        axis_forward='Y', axis_up='Z', object_types={'MESH'}, mesh_smooth_type='FACE',
        use_mesh_modifiers=True, bake_space_transform=False, add_leaf_bones=False)
    print('EXPORTED', path, os.path.getsize(path))


def studio(objs, path, azimuth=38.0, elevation=16.0, res=(900, 900), pad=1.22, key=6.0):
    """Render objs on a neutral studio set and write a PNG. Used to inspect authored geometry
    the same way the game is inspected -- from a rendered frame, not from the vertex list."""
    scn = bpy.context.scene
    scn.render.engine = 'BLENDER_EEVEE'
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.film_transparent = False
    scn.render.image_settings.file_format = 'PNG'
    world = bpy.data.worlds.get('C26Studio') or bpy.data.worlds.new('C26Studio')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.055, 0.062, 0.075, 1)
    world.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    scn.world = world

    # Only the subjects are in frame. Everything else in the file is hidden and restored, so an
    # inspection render can never be read as showing the wrong asset.
    hidden = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o not in objs]
    for o in hidden:
        o.hide_render = True

    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for corner in o.bound_box:
            w = o.matrix_world @ Vector(corner)
            lo = Vector((min(lo[i], w[i]) for i in range(3)))
            hi = Vector((max(hi[i], w[i]) for i in range(3)))
    centre = (lo + hi) * 0.5
    radius = max((hi - lo).length * 0.5, 0.05)

    cam_data = bpy.data.cameras.new('C26Cam')
    cam_data.lens = 62
    cam = bpy.data.objects.new('C26Cam', cam_data)
    bpy.context.collection.objects.link(cam)
    a, e = math.radians(azimuth), math.radians(elevation)
    dist = radius * pad / math.tan(cam_data.angle * 0.5) * 1.05
    cam.location = centre + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * dist
    cam.rotation_mode = 'QUATERNION'
    cam.rotation_quaternion = (centre - cam.location).to_track_quat('-Z', 'Y')
    scn.camera = cam

    lights = []
    for name, ang, elev, power, size in (('Key', azimuth + 34, 44, key, 1.4),
                                         ('Fill', azimuth - 78, 8, key * 0.28, 2.4),
                                         ('Rim', azimuth + 168, 30, key * 0.55, 1.0)):
        ld = bpy.data.lights.new(name, 'AREA')
        ld.energy = power * (dist ** 2) * 0.35
        ld.size = size * radius
        lo_ = bpy.data.objects.new(name, ld)
        bpy.context.collection.objects.link(lo_)
        la, le = math.radians(ang), math.radians(elev)
        lo_.location = centre + Vector((math.cos(la) * math.cos(le), math.sin(la) * math.cos(le), math.sin(le))) * dist
        lo_.rotation_mode = 'QUATERNION'
        lo_.rotation_quaternion = (centre - lo_.location).to_track_quat('-Z', 'Y')
        lights.append(lo_)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    scn.render.filepath = path
    bpy.ops.render.render(write_still=True)
    for o in [cam] + lights:
        bpy.data.objects.remove(o, do_unlink=True)
    for o in hidden:
        o.hide_render = False
    print('RENDER', path, os.path.getsize(path))
    return path
