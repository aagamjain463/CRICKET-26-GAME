import bpy
from mathutils import Vector
import os

fbx_files = [
    ("01_04aa892e", "/Users/aagamjain/Downloads/04aa892e68b810952b319e74312e3515.fbx"),
    ("02_4260cecc", "/Users/aagamjain/Downloads/4260cecc7e442f2e2bae94504197d685.fbx"),
    ("03_f4e20b7b", "/Users/aagamjain/Downloads/f4e20b7b4783bfaccf51887ac893cffc.fbx"),
    ("04_2beed08e", "/Users/aagamjain/Downloads/2beed08eff22799e06fa32354cbe2863.fbx"),
    ("05_0434293e", "/Users/aagamjain/Downloads/0434293e5196868c1e7037f018c8b52e.fbx"),
    ("06_d8114a2f", "/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx"),
    ("07_7d3019fc", "/Users/aagamjain/Downloads/7d3019fcdfeb6a7f57b8c983b9e893ef.fbx"),
    ("08_37929429", "/Users/aagamjain/Downloads/37929429758365e2e04e78d63348fd71.fbx"),
    ("09_19b08966", "/Users/aagamjain/Downloads/19b08966891a0630abb13956782a12c1.fbx"),
    ("10_7996719e", "/Users/aagamjain/Downloads/7996719ee0a1bf4d2eed1c06106897ae.fbx"),
]

for label, path in fbx_files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    
    m = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    
    verts = m.data.vertices
    min_y = min(v.co.y for v in verts)
    max_y = max(v.co.y for v in verts)
    h = max_y - min_y
    
    head_verts = [v.co for v in verts if (v.co.y - min_y) > 0.88 * h]
    head_min_z = min(v.z for v in head_verts) if head_verts else 0
    head_max_z = max(v.z for v in head_verts) if head_verts else 0
    head_min_x = min(v.x for v in head_verts) if head_verts else 0
    head_max_x = max(v.x for v in head_verts) if head_verts else 0
    
    visor_verts = [v for v in head_verts if v.z > 0.12]
    held_item_verts = [v.co for v in verts if v.co.z > 0.12 and 0.25 * h < (v.co.y - min_y) < 0.65 * h]
    
    shin_verts = [v.co for v in verts if 0.15 * h < (v.co.y - min_y) < 0.40 * h]
    shin_min_z = min(v.z for v in shin_verts) if shin_verts else 0
    shin_max_z = max(v.z for v in shin_verts) if shin_verts else 0
    shin_depth = shin_max_z - shin_min_z
    
    ankle_verts = [v.co for v in verts if (v.co.y - min_y) < 0.12 * h]
    left_ankles = [v for v in ankle_verts if v.x > 0.05]
    right_ankles = [v for v in ankle_verts if v.x < -0.05]
    
    l_foot_z = sum(v.z for v in left_ankles)/len(left_ankles) if left_ankles else 0
    r_foot_z = sum(v.z for v in right_ankles)/len(right_ankles) if right_ankles else 0
    stride_z = abs(l_foot_z - r_foot_z)
    
    print(f"\n==========================================")
    print(f"ASSET: {label}")
    print(f"Height: {h:.3f}m")
    print(f"Head: X=[{head_min_x:.3f}, {head_max_x:.3f}], Z=[{head_min_z:.3f}, {head_max_z:.3f}] (Visor/Grille verts: {len(visor_verts)})")
    print(f"Held Item / Front Gear verts: {len(held_item_verts)}")
    print(f"Shin Depth (Pads indicator): {shin_depth:.3f}m")
    print(f"Foot Stride (Z offset between feet): {stride_z:.3f}m")
    
    role = "Athletic Player"
    if len(held_item_verts) > 5000:
        role = "Batter with Bat"
    elif h < 1.05 and head_max_z > 0.15:
        role = "Wicketkeeper in Crouch"
    elif stride_z > 0.06:
        role = "Bowler / Running Athlete"
    elif len(visor_verts) > 1000:
        role = "Player with Helmet / Cap"
    elif shin_depth > 0.22:
        role = "Padded Batter / Fielder"
    else:
        role = "Umpire / Match Official / Neutral Athlete"
    print(f"IDENTIFIED ROLE: {role}")

