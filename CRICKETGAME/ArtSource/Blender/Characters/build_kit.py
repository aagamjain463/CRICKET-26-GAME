"""Build a real cricket jersey and trousers onto the existing CRICKET 26 base rig.

The base character ships in a street top and street trousers. Rather than model new garments and
transfer skin weights onto them -- which is where cloth on a game character usually goes wrong --
each garment is DERIVED FROM THE BODY MESH ITSELF: duplicate the body, delete everything outside
the garment region, then Solidify outward. The result is guaranteed to fit, guaranteed never to
let skin poke through, and inherits the body's vertex groups exactly, so it deforms with the
existing skeleton with no weight painting at all.

Landmarks are read from the armature at build time rather than hard-coded, so the script still
works if the base rig is ever re-exported at a different scale.

World frame in this file: +Y up, +X to the character's left, Z depth. Units are ~0.485 cm.
"""
import bpy, bmesh, math

JERSEY_MAT = ('Jerseymat', (0.055, 0.300, 0.345))
TROUSER_MAT = ('Trousermat', (0.780, 0.790, 0.775))


def _activate(scene):
    """Make `scene` the one operators act on. bpy.context.window is None when a script runs
    outside a window context -- which is the normal case immediately after opening a file from an
    automation bridge -- so fall back to the window manager's own window list."""
    window = getattr(bpy.context, 'window', None)
    if window is None:
        windows = [w for wm in bpy.data.window_managers for w in wm.windows]
        window = windows[0] if windows else None
    if window is not None:
        window.scene = scene
    return scene


def _bone(arm, name):
    """Bone head in ARMATURE space, which is also the body mesh's own local space -- the body is
    parented with an identity parent-inverse. Landmarks and garment vertices therefore share one
    coordinate system, and neither depends on matrix_world, which is stale immediately after a
    file is opened and quietly reports the wrong space if you trust it."""
    return arm.data.bones['mixamorig:' + name].head_local


def _garment(src, arm, name, keep, thickness, lining, mat_name, mat_colour):
    """Duplicate src, keep only the vertices `keep(local_co)` accepts, then shell it outward."""
    obj = src.copy()
    obj.data = src.data.copy()
    obj.name = obj.data.name = name
    # Link beside the body, not into whatever scene happens to be active: the source file carries
    # several scenes and the rig only lives in one of them.
    for coll in src.users_collection:
        coll.objects.link(obj)

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    # Vertex coordinates, not world coordinates: the landmarks come from bone head_local, which
    # is armature space, and the body's mesh data is in that same space. Filtering against
    # matrix_world instead compares centimetre-scale landmarks to metre-scale world positions and
    # silently deletes the entire garment.
    doomed = [v for v in bm.verts if not keep(v.co)]
    bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.to_mesh(obj.data)
    bm.free()

    obj.data.materials.clear()
    mat = bpy.data.materials.get(mat_name) or bpy.data.materials.new(mat_name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    if bsdf:
        bsdf.inputs['Base Color'].default_value = (*mat_colour, 1.0)
        bsdf.inputs['Roughness'].default_value = 0.78
    obj.data.materials.append(mat)

    # Shell the garment outward in bmesh rather than with a Solidify modifier. Modifier apply
    # goes through bpy.ops, which needs the object to be in the active view layer -- and the rig
    # lives in a scene that is not the active one when this runs from an automation bridge. Doing
    # it on the mesh data directly has no context requirement at all, and it keeps the vertex
    # groups the garment inherited from the body, which is the whole reason for deriving it.
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bm.normal_update()
    offsets = [(v, v.normal.copy()) for v in bm.verts]
    for v, n in offsets:
        v.co += n * thickness
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=-lining)
    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()

    amod = obj.modifiers.new('Armature', 'ARMATURE')
    amod.object = arm
    # Parenting is inherited from the body copy and deliberately left alone: the body sits under
    # the armature with an identity parent-inverse, and overriding it with the armature's inverse
    # world matrix scales the garment by 100 and rotates it out of the rig.
    return obj


def _collar(arm, jersey, neck_y, radius, colour):
    """A turned cricket collar, rigidly weighted to the neck bone -- which is what a collar does."""
    me = bpy.data.meshes.new('Collar')
    verts, faces = [], []
    sides = 20
    rows = [(neck_y - 1.0, radius * 1.02, 0.0), (neck_y + 3.4, radius * 1.06, 0.0),
            (neck_y + 6.2, radius * 1.22, 0.0)]
    for ri, (y, r, _) in enumerate(rows):
        for j in range(sides):
            a = 2 * math.pi * j / sides
            verts.append((math.cos(a) * r, y, math.sin(a) * r * 0.78))
    for ri in range(len(rows) - 1):
        for j in range(sides):
            k = (j + 1) % sides
            faces.append((ri * sides + j, ri * sides + k, (ri + 1) * sides + k, (ri + 1) * sides + j))
    me.from_pydata(verts, [], faces)
    me.update()
    mat = bpy.data.materials.get('Collarmat') or bpy.data.materials.new('Collarmat')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    if bsdf:
        bsdf.inputs['Base Color'].default_value = (*colour, 1.0)
        bsdf.inputs['Roughness'].default_value = 0.72
    me.materials.append(mat)
    obj = bpy.data.objects.new('Collar', me)
    for coll in jersey.users_collection:
        coll.objects.link(obj)
    grp = obj.vertex_groups.new(name='mixamorig:Neck')
    grp.add(list(range(len(verts))), 1.0, 'REPLACE')
    obj.modifiers.new('Armature', 'ARMATURE').object = arm
    obj.parent = arm            # identity parent-inverse, same as the body
    return obj


def build():
    arm = bpy.data.objects['Armature']
    body = bpy.data.objects['Body']
    # Work in the scene that owns the rig. The source file carries three scenes and the active
    # one is not necessarily it, which makes every view-layer operation below fail or, worse,
    # silently build the garments somewhere the exporter will not find them.
    _activate(next(s for s in bpy.data.scenes if arm.name in s.objects))
    hips = _bone(arm, 'Hips').y
    neck = _bone(arm, 'Neck').y
    shoulder = _bone(arm, 'LeftArm')
    hand = _bone(arm, 'LeftHand')
    ankle = _bone(arm, 'LeftFoot').y
    waist = hips - 14.0
    sleeve = shoulder.x + (hand.x - shoulder.x) * 0.31       # short sleeve to mid-bicep
    scale = (neck - ankle) / 155.0                            # units per cm, from the real rig

    jersey = _garment(
        body, arm, 'Jersey',
        lambda p: abs(p.x) <= sleeve and waist <= p.y <= neck + 5.0,
        2.3 * scale, 0.9 * scale, *JERSEY_MAT)
    trousers = _garment(
        body, arm, 'Trousers',
        lambda p: p.y <= hips + 8.0 and p.y >= ankle + 6.0,
        2.8 * scale, 1.0 * scale, *TROUSER_MAT)
    collar = _collar(arm, jersey, neck + 2.0, 11.5 * scale, (0.030, 0.190, 0.225))

    print('C26_KIT jersey verts=%d trousers verts=%d collar verts=%d scale=%.3f sleeve=%.1f'
          % (len(jersey.data.vertices), len(trousers.data.vertices), len(collar.data.vertices),
             scale, sleeve))
    return jersey, trousers, collar


def export(path):
    """Export the re-kitted rig.

    Default FBX axes on purpose: the base character's mesh and armature data are Z-up with a +90
    X rotation on the armature object, which is exactly what Blender's FBX importer produces from
    a standard rig. Exporting with the same defaults round-trips that untouched, so the imported
    result keeps the orientation AC26Athlete already compensates for (mesh faces +Y, mesh +X to
    the character's left). Tools/ImportKit.py asserts the height and a bone position against the
    existing asset, so any drift fails the import instead of reaching the pitch.
    """
    import os
    # The street top, street trousers and trainers are all replaced now: the jersey and trousers
    # are authored here and the shoes are an authored static mesh attached to the foot bones.
    # Removing them outright rather than hiding them at runtime saves ~11k triangles per athlete.
    for name in ('Tops', 'Bottoms', 'Shoes'):
        obj = bpy.data.objects.get(name)
        if obj:
            bpy.data.objects.remove(obj, do_unlink=True)
    # The source file carries several scenes and the rig does not live in whichever one happens
    # to be active. Export from the scene that actually owns the armature, linking anything built
    # elsewhere into it first, so the FBX can never contain garments without the skeleton that
    # drives them -- which is exactly what a naive `context.scene.objects` export produced.
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    scene = next(s for s in bpy.data.scenes if arm.name in s.objects)
    _activate(scene)
    for o in bpy.data.objects:
        if o.type == 'MESH' and o.parent is arm and o.name not in scene.objects:
            scene.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT')
    keep = [o for o in scene.objects if o.type in {'MESH', 'ARMATURE'}]
    for o in keep:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, apply_unit_scale=True, global_scale=1.0,
        object_types={'MESH', 'ARMATURE'}, mesh_smooth_type='FACE',
        use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False,
        bake_space_transform=False, use_armature_deform_only=False)
    print('EXPORTED', path, os.path.getsize(path))
    print('OBJECTS', sorted(o.name for o in keep))
