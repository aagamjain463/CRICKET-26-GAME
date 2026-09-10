import bpy, os, math
from mathutils import Vector, Euler

downloads = '/Users/aagamjain/Downloads'
out_dir = '/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players'
os.makedirs(out_dir, exist_ok=True)

assets = [
    ('SM_C26_Player_Fielder_01', '04aa892e68b810952b319e74312e3515.fbx', 182.0),
    ('SM_C26_Player_Umpire',     '4260cecc7e442f2e2bae94504197d685.fbx', 183.0),
    ('SM_C26_Player_Fielder_02', 'f4e20b7b4783bfaccf51887ac893cffc.fbx', 182.0),
    ('SM_C26_Player_Fielder_03', '2beed08eff22799e06fa32354cbe2863.fbx', 182.0),
    ('SM_C26_Player_Fielder_04', '0434293e5196868c1e7037f018c8b52e.fbx', 182.0),
    ('SM_C26_Player_Batter',     'd8114a2f9e26e151d26f8eac5ebbcab9.fbx', 182.0),
    ('SM_C26_Player_Keeper',     '7d3019fcdfeb6a7f57b8c983b9e893ef.fbx', 165.0), # crouched
    ('SM_C26_Player_Bowler',     '37929429758365e2e04e78d63348fd71.fbx', 185.0),
    ('SM_C26_Player_Fielder_05', '19b08966891a0630abb13956782a12c1.fbx', 182.0),
    ('SM_C26_Player_Fielder_06', '7996719ee0a1bf4d2eed1c06106897ae.fbx', 182.0)
]

for out_name, in_fbx, target_h_cm in assets:
    in_path = os.path.join(downloads, in_fbx)
    out_path = os.path.join(out_dir, out_name + '.fbx')
    
    print(f"\n--- Processing {out_name} from {in_fbx} ---")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=in_path)
    
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    assert meshes, f"No mesh in {in_fbx}"
    m = meshes[0]
    
    # Apply existing transforms first
    bpy.context.view_layer.objects.active = m
    m.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    
    # Compute bounds
    corners = [Vector(c) for c in m.bound_box]
    min_z = min(c.z for c in corners)
    max_z = max(c.z for c in corners)
    current_h_m = max_z - min_z
    
    # Move origin to ground center (min_z = 0, center x, y)
    center_x = (min(c.x for c in corners) + max(c.x for c in corners)) / 2.0
    center_y = (min(c.y for c in corners) + max(c.y for c in corners)) / 2.0
    
    for v in m.data.vertices:
        v.co.x -= center_x
        v.co.y -= center_y
        v.co.z -= min_z
    m.data.update()
    
    # Target scale: target_h_cm in cm
    # FBX in Blender default units is meters. 1 unit in Unreal = 1 cm.
    # So we want the vertex coordinates to be directly in centimetres!
    # Scale factor from current meters to centimetres:
    scale_factor = target_h_cm / current_h_m
    
    for v in m.data.vertices:
        # Rotate by +90 deg around Z: (-y, x, z) so -Y becomes +X forward!
        old_x, old_y, old_z = v.co.x, v.co.y, v.co.z
        new_x = -old_y * scale_factor
        new_y = old_x * scale_factor
        new_z = old_z * scale_factor
        v.co.x = new_x
        v.co.y = new_y
        v.co.z = new_z
    m.data.update()
    
    # Decimate modifier to ~35,000 faces (ratio ~ 0.046)
    target_faces = 35000
    current_faces = len(m.data.polygons)
    if current_faces > target_faces:
        ratio = target_faces / float(current_faces)
        dec = m.modifiers.new('GameLOD', 'DECIMATE')
        dec.ratio = ratio
        bpy.ops.object.modifier_apply(modifier=dec.name)
        print(f"Decimated {current_faces} -> {len(m.data.polygons)} faces")
        
    # Smooth normals
    for p in m.data.polygons:
        p.use_smooth = True
        
    # Set material slot name to M_Athlete
    m.name = out_name
    m.data.name = out_name
    if m.material_slots:
        m.material_slots[0].material.name = 'M_Athlete'
        
    # Export FBX
    bpy.ops.export_scene.fbx(
        filepath=out_path,
        use_selection=True,
        global_scale=1.0,
        axis_forward='X',
        axis_up='Z',
        apply_unit_scale=True,
        bake_space_transform=True
    )
    print(f"Exported {out_path} ({os.path.getsize(out_path)/1024:.1f} KB, verts: {len(m.data.vertices)})")

print("\n=== ALL 10 PLAYERS PROCESSED SUCCESSFULLY ===")
