import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath="/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx")
m = bpy.data.objects['node_0']
bpy.context.view_layer.objects.active = m
m.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Find lowest point (bat tip vs feet)
verts = [Vector(v.co) for v in m.data.vertices]
low_pts = [v for v in verts if v.z < 0.05]
print("Lowest points count:", len(low_pts))
min_x = min(v.x for v in low_pts)
max_x = max(v.x for v in low_pts)
min_y = min(v.y for v in low_pts)
max_y = max(v.y for v in low_pts)
print(f"Lowest points (feet/bat) bounds: X=[{min_x:.3f}, {max_x:.3f}], Y=[{min_y:.3f}, {max_y:.3f}]")

# Where is the head?
head_pts = [v for v in verts if v.z > 0.95]
avg_head = sum(head_pts, Vector((0,0,0))) / len(head_pts)
print(f"Head average position: X={avg_head.x:.3f}, Y={avg_head.y:.3f}, Z={avg_head.z:.3f}")
