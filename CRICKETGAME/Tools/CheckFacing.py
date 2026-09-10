import bpy
from mathutils import Vector

p = "/Users/aagamjain/Downloads/4260cecc7e442f2e2bae94504197d685.fbx" # Umpire
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=p)
m = bpy.data.objects['node_0']

# Find vertices around head height (z > 1.0 in world coords)
head_pts = [m.matrix_world @ Vector(v.co) for v in m.data.vertices if (m.matrix_world @ Vector(v.co)).z > 1.0]
avg_head_y = sum(pt.y for pt in head_pts) / len(head_pts)

# Find the extreme Y point in the head (nose)
min_y = min(pt.y for pt in head_pts)
max_y = max(pt.y for pt in head_pts)
print("Umpire head avg Y:", avg_head_y, "min Y:", min_y, "max Y:", max_y)

# Let's check feet (z < 0.1)
feet_pts = [m.matrix_world @ Vector(v.co) for v in m.data.vertices if (m.matrix_world @ Vector(v.co)).z < 0.1]
min_feet_y = min(pt.y for pt in feet_pts)
max_feet_y = max(pt.y for pt in feet_pts)
avg_feet_y = sum(pt.y for pt in feet_pts) / len(feet_pts)
print("Umpire feet avg Y:", avg_feet_y, "min Y (toes/heels):", min_feet_y, "max Y:", max_feet_y)

# Which direction extends further from the center of mass?
# Usually toes extend in front of the ankles (+Y or -Y?).
