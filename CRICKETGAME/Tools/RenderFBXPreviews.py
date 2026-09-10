import bpy
import sys
import os
import math
from mathutils import Vector

fbx_files = [
    "/Users/aagamjain/Downloads/04aa892e68b810952b319e74312e3515.fbx",
    "/Users/aagamjain/Downloads/4260cecc7e442f2e2bae94504197d685.fbx",
    "/Users/aagamjain/Downloads/f4e20b7b4783bfaccf51887ac893cffc.fbx",
    "/Users/aagamjain/Downloads/2beed08eff22799e06fa32354cbe2863.fbx",
    "/Users/aagamjain/Downloads/0434293e5196868c1e7037f018c8b52e.fbx",
    "/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx",
    "/Users/aagamjain/Downloads/7d3019fcdfeb6a7f57b8c983b9e893ef.fbx",
    "/Users/aagamjain/Downloads/37929429758365e2e04e78d63348fd71.fbx",
    "/Users/aagamjain/Downloads/19b08966891a0630abb13956782a12c1.fbx",
    "/Users/aagamjain/Downloads/7996719ee0a1bf4d2eed1c06106897ae.fbx",
]

os.makedirs("ArtSource/Previews", exist_ok=True)
os.makedirs("/Users/aagamjain/.gemini/antigravity-acp/brain/f4716146-0c2c-40d6-ba1e-24f4316ff130", exist_ok=True)

for idx, fbx_path in enumerate(fbx_files):
    hash_name = os.path.splitext(os.path.basename(fbx_path))[0]
    out_img = f"ArtSource/Previews/{idx+1:02d}_{hash_name[:8]}.png"
    art_img = f"/Users/aagamjain/.gemini/antigravity-acp/brain/f4716146-0c2c-40d6-ba1e-24f4316ff130/{idx+1:02d}_{hash_name[:8]}.png"
    
    # Reset scene
    bpy.ops.wm.read_factory_settings(use_empty=True)
    
    # Import FBX
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    
    # Extract / save packed images
    raw_tex_dir = f"ArtSource/Raw/{idx+1:02d}_{hash_name[:8]}"
    os.makedirs(raw_tex_dir, exist_ok=True)
    
    for img in bpy.data.images:
        if img.has_data:
            tex_out = os.path.join(raw_tex_dir, img.name)
            if not tex_out.lower().endswith('.png'):
                tex_out += '.png'
            try:
                img.save_render(tex_out)
                print(f"Saved texture {img.name} -> {tex_out}")
            except Exception as e:
                print(f"Failed to save image {img.name}: {e}")
                
    # Calculate bounding box of all meshes
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        print(f"No meshes found in {hash_name}")
        continue
        
    bbox_corners = [o.matrix_world @ Vector(corner) for o in meshes for corner in o.bound_box]
    min_c = Vector((min(c.x for c in bbox_corners), min(c.y for c in bbox_corners), min(c.z for c in bbox_corners)))
    max_c = Vector((max(c.x for c in bbox_corners), max(c.y for c in bbox_corners), max(c.z for c in bbox_corners)))
    center = (min_c + max_c) / 2.0
    size = max_c - min_c
    max_dim = max(size.x, size.y, size.z)
    
    # Set up Camera
    cam_data = bpy.data.cameras.new("PreviewCam")
    cam_data.lens = 50
    cam_obj = bpy.data.objects.new("PreviewCam", cam_data)
    bpy.context.scene.collection.objects.link(cam_obj)
    bpy.context.scene.camera = cam_obj
    
    # Camera distance based on bounding box
    dist = max_dim * 2.2
    cam_obj.location = center + Vector((dist * 0.7, -dist * 1.5, dist * 0.6))
    
    # Point camera at center
    direction = center - cam_obj.location
    rot_quat = direction.to_track_quat('-Z', 'Y')
    cam_obj.rotation_euler = rot_quat.to_euler()
    
    # Lighting
    light_data = bpy.data.lights.new(name="KeyLight", type='SUN')
    light_data.energy = 3.5
    light_obj = bpy.data.objects.new(name="KeyLight", object_data=light_data)
    bpy.context.scene.collection.objects.link(light_obj)
    light_obj.rotation_euler = (math.radians(45), math.radians(15), math.radians(30))
    
    fill_data = bpy.data.lights.new(name="FillLight", type='SUN')
    fill_data.energy = 1.8
    fill_obj = bpy.data.objects.new(name="FillLight", object_data=fill_data)
    bpy.context.scene.collection.objects.link(fill_obj)
    fill_obj.rotation_euler = (math.radians(-30), math.radians(45), math.radians(-120))
    
    # World background
    world = bpy.data.worlds.new("PreviewWorld")
    world.use_nodes = True
    bg_node = world.node_tree.nodes.get("Background")
    if bg_node:
        bg_node.inputs[0].default_value = (0.08, 0.10, 0.14, 1.0) # Deep slate studio backdrop
    bpy.context.scene.world = world
    
    # Render settings
    bpy.context.scene.render.engine = 'BLENDER_EEVEE'
    bpy.context.scene.render.resolution_x = 512
    bpy.context.scene.render.resolution_y = 512
    bpy.context.scene.render.filepath = out_img
    bpy.context.scene.render.image_settings.file_format = 'PNG'
    
    print(f"Rendering {out_img} (dims: {size.x:.3f} x {size.y:.3f} x {size.z:.3f})...")
    bpy.ops.render.render(write_still=True)
    
    # Copy to artifact
    import shutil
    shutil.copyfile(out_img, art_img)
    print(f"Done render {idx+1}: {out_img}")

print("\nAll previews rendered!")
