# Render a turn-table preview of a C26 character .blend so the mesh can actually be looked at.
# Usage: blender -b <file.blend> -P Tools/BlenderPreview.py -- <out.png>
import sys, os, math
import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
out = argv[0] if argv else "/tmp/c26_preview.png"

print("C26_BLEND objects:")
for o in bpy.data.objects:
    extra = ""
    if o.type == "MESH":
        extra = " verts=%d polys=%d" % (len(o.data.vertices), len(o.data.polygons))
        extra += " mats=%s" % ",".join([m.name for m in o.data.materials if m])
        extra += " mods=%s" % ",".join([m.type for m in o.modifiers])
    elif o.type == "ARMATURE":
        extra = " bones=%d" % len(o.data.bones)
    print("C26_BLEND   %-40s %-9s%s" % (o.name, o.type, extra))

# Bounds of everything visible.
mins = Vector((1e9, 1e9, 1e9))
maxs = Vector((-1e9, -1e9, -1e9))
for o in bpy.data.objects:
    if o.type != "MESH" or not o.visible_get():
        continue
    for corner in o.bound_box:
        w = o.matrix_world @ Vector(corner)
        for i in range(3):
            mins[i] = min(mins[i], w[i])
            maxs[i] = max(maxs[i], w[i])
center = (mins + maxs) / 2.0
size = max((maxs - mins).x, (maxs - mins).y, (maxs - mins).z)
print("C26_BLEND bounds min=%s max=%s size=%.3f" % (tuple(round(v, 2) for v in mins), tuple(round(v, 2) for v in maxs), size))

# Camera, three-quarter front.
cam_data = bpy.data.cameras.new("C26Cam")
cam = bpy.data.objects.new("C26Cam", cam_data)
bpy.context.scene.collection.objects.link(cam)
cam.location = center + Vector((size * 1.15, -size * 1.55, size * 0.30))
direction = center - cam.location
cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
cam_data.lens = 62
bpy.context.scene.camera = cam

# Lights.
for name, loc, energy in (("C26Key", (size * 1.6, -size * 1.9, size * 1.5), 4.0),
                          ("C26Fill", (-size * 1.8, -size * 1.1, size * 0.9), 1.6),
                          ("C26Rim", (-size * 0.7, size * 1.8, size * 1.3), 2.2)):
    ld = bpy.data.lights.new(name, "SUN")
    ld.energy = energy
    lo = bpy.data.objects.new(name, ld)
    bpy.context.scene.collection.objects.link(lo)
    lo.location = center + Vector(loc)
    lo.rotation_euler = (center - lo.location).to_track_quat("-Z", "Y").to_euler()

# Neutral studio background so the silhouette reads.
world = bpy.data.worlds.new("C26World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.18, 0.20, 0.23, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.85
bpy.context.scene.world = world

scene = bpy.context.scene
for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
    try:
        scene.render.engine = engine
        print("C26_BLEND engine=%s" % engine)
        break
    except Exception:
        continue
scene.render.resolution_x = 700
scene.render.resolution_y = 1000
scene.render.film_transparent = False
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("C26_BLEND rendered %s" % out)
