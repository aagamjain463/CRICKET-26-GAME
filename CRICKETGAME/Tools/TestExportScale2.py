import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath="/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx")
m = bpy.data.objects['node_0']
bpy.context.view_layer.objects.active = m
m.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

corners = [Vector(c) for c in m.bound_box]
min_z = min(c.z for c in corners)
max_z = max(c.z for c in corners)
h = max_z - min_z
center_x = (min(c.x for c in corners) + max(c.x for c in corners)) / 2.0
center_y = (min(c.y for c in corners) + max(c.y for c in corners)) / 2.0

scale_to_m = 1.82 / h

for v in m.data.vertices:
    ox, oy, oz = v.co.x - center_x, v.co.y - center_y, v.co.z - min_z
    v.co.x = -oy * scale_to_m
    v.co.y = ox * scale_to_m
    v.co.z = oz * scale_to_m

m.data.update()

dec = m.modifiers.new('GameLOD', 'DECIMATE')
dec.ratio = 35000 / float(len(m.data.polygons))
bpy.ops.object.modifier_apply(modifier=dec.name)

for p in m.data.polygons:
    p.use_smooth = True

out_fbx = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players/SM_C26_Player_Batter.fbx"
bpy.ops.export_scene.fbx(
    filepath=out_fbx,
    use_selection=True,
    global_scale=1.0,
    apply_unit_scale=False,
    axis_forward='X',
    axis_up='Z'
)
print("Exported scale test FBX 2")
