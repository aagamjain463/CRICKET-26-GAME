import bpy
import os
import math
from mathutils import Vector

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
    
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        print(f"[{label}] No mesh")
        continue
        
    m = meshes[0]
    bbox = [Vector(b) for b in m.bound_box]
    min_x = min(b.x for b in bbox)
    max_x = max(b.x for b in bbox)
    min_y = min(b.y for b in bbox)
    max_y = max(b.y for b in bbox)
    min_z = min(b.z for b in bbox)
    max_z = max(b.z for b in bbox)
    
    dx = max_x - min_x
    dy = max_y - min_y
    dz = max_z - min_z
    
    # Vertices distribution
    verts = m.data.vertices
    n_verts = len(verts)
    
    # Let's inspect height profile along the longest axis
    # Which axis is longest?
    axes = [('X', dx), ('Y', dy), ('Z', dz)]
    axes.sort(key=lambda a: a[1], reverse=True)
    longest_axis, length = axes[0]
    
    # Sample colors from texture
    avg_color = [0, 0, 0]
    tex_path = f"ArtSource/Raw/{label}/texture_pbr_20250901.png"
    
    print(f"\n[{label}] Dims: X={dx:.3f}, Y={dy:.3f}, Z={dz:.3f} | Longest={longest_axis} ({length:.3f}) | Verts={n_verts}")
    
    # Let's check vertical slices along Z (assuming Z is height) or Y (if Y-up)
    # Check if Y is height (typical in FBX)
    print(f"   Bounds X: [{min_x:.3f}, {max_x:.3f}]")
    print(f"   Bounds Y: [{min_y:.3f}, {max_y:.3f}]")
    print(f"   Bounds Z: [{min_z:.3f}, {max_z:.3f}]")

