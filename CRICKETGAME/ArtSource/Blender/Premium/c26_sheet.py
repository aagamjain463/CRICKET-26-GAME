"""Render a grid of poses to one PNG.

Reviewing cricket motion means actually looking at it, but one render per pose is a slow and
expensive way to look. This composites many poses into a single contact sheet so a whole action can
be judged at a glance, the way an animator flips through keys on a lightbox.
"""
import bpy, numpy as np
from mathutils import Vector

CELL_W, CELL_H = 300, 430


def setup(rig, view='threequarter'):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.color_type = 'MATERIAL'
    shading.show_shadows = True
    shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = CELL_W, CELL_H
    scene.render.film_transparent = False
    if scene.world is None:
        scene.world = bpy.data.worlds.new('C26_SheetWorld')
    scene.world.color = (0.045, 0.05, 0.06)

    cam = bpy.data.objects.get('C26_SheetCam')
    if not cam:
        data = bpy.data.cameras.new('C26_SheetCam')
        cam = bpy.data.objects.new('C26_SheetCam', data)
        bpy.context.collection.objects.link(cam)
    cam.data.lens = 62
    focus = Vector((0, 0, 0.95))
    offsets = {
        # The rig faces -Y, so the "front" camera sits out at -Y.
        'front': Vector((0.05, -4.6, 1.15)),
        'threequarter': Vector((2.9, -3.6, 1.35)),
        'side': Vector((4.6, -0.2, 1.15)),
        'offside': Vector((-3.4, -3.1, 1.3)),
        'high': Vector((2.4, -3.4, 2.7)),
    }
    cam.location = offsets.get(view, offsets['threequarter'])
    direction = focus - cam.location
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam

    if not bpy.data.objects.get('C26_SheetFloor'):
        bpy.ops.mesh.primitive_plane_add(size=14, location=(0, 0, 0))
        bpy.context.active_object.name = 'C26_SheetFloor'
    return cam


def _render_cell():
    scene = bpy.context.scene
    scene.render.filepath = '/tmp/c26_cell.png'
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load('/tmp/c26_cell.png', check_existing=False)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
    bpy.data.images.remove(img)
    return px[::-1]  # Blender scanlines run bottom-up


def sheet(rig, apply_pose, entries, out_path, cols=6, view='threequarter'):
    """`entries` is [(label, spec[, view]), ...]. Labels are burned in as a simple marker bar so a cell can
    be identified without a legend."""
    setup(rig, view)
    rows = (len(entries) + cols - 1) // cols
    canvas = np.zeros((rows * CELL_H, cols * CELL_W, 4), dtype=np.float32)
    canvas[..., 3] = 1.0
    for i, entry in enumerate(entries):
        label, spec = entry[0], entry[1]
        if len(entry) > 2:
            setup(rig, entry[2])
        apply_pose(rig, spec)
        bpy.context.view_layer.update()
        cell = _render_cell()
        r, c = divmod(i, cols)
        canvas[r * CELL_H:(r + 1) * CELL_H, c * CELL_W:(c + 1) * CELL_W] = cell
        # index bar: one tick per unit, so cells stay identifiable in the grid
        y0 = r * CELL_H + 6
        for t in range(i + 1):
            x0 = c * CELL_W + 8 + t * 7
            canvas[y0:y0 + 9, x0:x0 + 5] = (1.0, 0.42, 0.08, 1.0)
    out = bpy.data.images.new('C26_Sheet', width=cols * CELL_W, height=rows * CELL_H)
    out.pixels = canvas[::-1].ravel().tolist()
    out.filepath_raw = str(out_path)
    out.file_format = 'PNG'
    out.save()
    bpy.data.images.remove(out)
    return str(out_path)
