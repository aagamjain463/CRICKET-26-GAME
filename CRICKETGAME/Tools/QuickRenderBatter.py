import bpy

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath="/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx")
m = bpy.data.objects['node_0']
bpy.context.view_layer.objects.active = m
m.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# Texture
img_path = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Raw/06_d8114a2f/texture_pbr_20250901.png"
mat = bpy.data.materials.new(name="Mat")
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get("Principled BSDF")
tex_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex_node.image = bpy.data.images.load(img_path)
mat.node_tree.links.new(bsdf.inputs['Base Color'], tex_node.outputs['Color'])
m.data.materials.clear()
m.data.materials.append(mat)

# Camera looking at batter from -Y (front)
cam_data = bpy.data.cameras.new(name='Cam')
cam = bpy.data.objects.new('Cam', cam_data)
bpy.context.scene.collection.objects.link(cam)
bpy.context.scene.camera = cam
cam.location = (0, -2.5, 0.8)
cam.rotation_euler = (1.5708, 0, 0)

# Light
light_data = bpy.data.lights.new(name='Light', type='SUN')
light_data.energy = 3.0
light = bpy.data.objects.new('Light', light_data)
bpy.context.scene.collection.objects.link(light)
light.location = (2, -2, 3)

bpy.context.scene.render.resolution_x = 256
bpy.context.scene.render.resolution_y = 256
bpy.context.scene.render.filepath = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Previews/batter_front_view.png"
bpy.ops.render.render(write_still=True)
print("Rendered batter front view")
