import bpy
from mathutils import Vector
import os

downloads = '/Users/aagamjain/Downloads'
assets = [
    # (index, role_name, hash_fbx, target_h_m, yaw_offset_deg)
    ("01", "SM_C26_Player_Fielder1", "04aa892e68b810952b319e74312e3515.fbx", 1.82, 0),
    ("02", "SM_C26_Player_Umpire", "4260cecc7e442f2e2bae94504197d685.fbx", 1.80, 0),
    ("03", "SM_C26_Player_Fielder2", "f4e20b7b4783bfaccf51887ac893cffc.fbx", 1.82, 0),
    ("04", "SM_C26_Player_Fielder3", "2beed08eff22799e06fa32354cbe2863.fbx", 1.82, 0),
    ("05", "SM_C26_Player_Fielder4", "0434293e5196868c1e7037f018c8b52e.fbx", 1.82, 0),
    ("06", "SM_C26_Player_Batter", "d8114a2f9e26e151d26f8eac5ebbcab9.fbx", 1.77, 0),
    ("07", "SM_C26_Player_NonStriker", "7d3019fcdfeb6a7f57b8c983b9e893ef.fbx", 1.62, 0),
    ("08", "SM_C26_Player_Bowler", "37929429758365e2e04e78d63348fd71.fbx", 1.85, 0),
    ("09", "SM_C26_Player_Fielder5", "19b08966891a0630abb13956782a12c1.fbx", 1.82, 0),
    ("10", "SM_C26_Player_Keeper", "7996719ee0a1bf4d2eed1c06106897ae.fbx", 1.72, 0),
]

out_dir = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Players"
os.makedirs(out_dir, exist_ok=True)

# Standard reference scan upright height is 1.146m
# Scale factor to 1.82m is 1.82 / 1.146 = 1.5881326
UNIVERSAL_SCALE = 1.82 / 1.146

for idx, role_name, fname, target_h, yaw_deg in assets:
    fbx_p = os.path.join(downloads, fname)
    print(f"\n--- Processing [{idx}] {role_name} ({fname}) ---")
    
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx_p)
    m = bpy.data.objects.get('node_0')
    if not m:
        print(f"ERROR: node_0 not found in {fname}")
        continue
        
    bpy.context.view_layer.objects.active = m
    m.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    
    # Calculate raw bounds
    corners = [Vector(c) for c in m.bound_box]
    min_z = min(c.z for c in corners)
    max_z = max(c.z for c in corners)
    center_x = (min(c.x for c in corners) + max(c.x for c in corners)) / 2.0
    center_y = (min(c.y for c in corners) + max(c.y for c in corners)) / 2.0
    
    scale_m = UNIVERSAL_SCALE
    
    # Rotate: in raw scan, -Y is forward, +Z is up, +X is right.
    # To bring into Unreal (+X forward, +Y right, +Z up):
    # rotate +90 deg around Z:
    # new_x = -oy * scale_m
    # new_y =  ox * scale_m
    # new_z =  oz * scale_m
    for v in m.data.vertices:
        ox = v.co.x - center_x
        oy = v.co.y - center_y
        oz = v.co.z - min_z
        v.co.x = -oy * scale_m
        v.co.y =  ox * scale_m
        v.co.z =  oz * scale_m
        
    m.data.update()
    
    # Decimate to ~35,000 tris for smooth real-time performance in UE5
    poly_count = len(m.data.polygons)
    if poly_count > 40000:
        dec = m.modifiers.new('GameLOD', 'DECIMATE')
        dec.ratio = 35000.0 / float(poly_count)
        bpy.ops.object.modifier_apply(modifier=dec.name)
        print(f"Decimated from {poly_count} to {len(m.data.polygons)} polygons")
        
    for p in m.data.polygons:
        p.use_smooth = True
        
    out_fbx = os.path.join(out_dir, f"{role_name}.fbx")
    bpy.ops.export_scene.fbx(
        filepath=out_fbx,
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=False,
        axis_forward='X',
        axis_up='Z'
    )
    print(f"Saved: {out_fbx}")
