import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath="/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx")

m = bpy.data.objects['node_0']
print("Object rotation:", m.rotation_euler)
print("Object scale:", m.scale)
print("Object location:", m.location)

corners = [Vector(c) for c in m.bound_box]
min_x, max_x = min(c.x for c in corners), max(c.x for c in corners)
min_y, max_y = min(c.y for c in corners), max(c.y for c in corners)
min_z, max_z = min(c.z for c in corners), max(c.z for c in corners)
print(f"X: [{min_x:.3f}, {max_x:.3f}] (size {max_x-min_x:.3f})")
print(f"Y: [{min_y:.3f}, {max_y:.3f}] (size {max_y-min_y:.3f})")
print(f"Z: [{min_z:.3f}, {max_z:.3f}] (size {max_z-min_z:.3f})")

# Let's inspect which axis is UP:
# Is Z up, or Y up?
