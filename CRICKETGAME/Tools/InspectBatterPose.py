import bpy
from mathutils import Vector

p = "/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=p)
m = bpy.data.objects['node_0']

# Let's inspect where the hands, bat, legs are:
print("Batter mesh vertices:", len(m.data.vertices))
# Let's check the bounding box and key features
