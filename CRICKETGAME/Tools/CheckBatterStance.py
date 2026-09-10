import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath="/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx")
m = bpy.data.objects['node_0']
bpy.context.view_layer.objects.active = m
m.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Find where the bat is (lowest/furthest vertices)
verts = [Vector(v.co) for v in m.data.vertices]
min_x = min(v.x for v in verts)
max_x = max(v.x for v in verts)
min_y = min(v.y for v in verts)
max_y = max(v.y for v in verts)
min_z = min(v.z for v in verts)
max_z = max(v.z for v in verts)

print(f"Batter bounds: X=[{min_x:.3f}, {max_x:.3f}], Y=[{min_y:.3f}, {max_y:.3f}], Z=[{min_z:.3f}, {max_z:.3f}]")
