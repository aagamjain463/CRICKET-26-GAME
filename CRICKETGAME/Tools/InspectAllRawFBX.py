import bpy
from mathutils import Vector
import os

downloads = '/Users/aagamjain/Downloads'
fbx_files = [
    ("01_04aa892e", "04aa892e68b810952b319e74312e3515.fbx"),
    ("02_4260cecc", "4260cecc7e442f2e2bae94504197d685.fbx"),
    ("03_f4e20b7b", "f4e20b7b4783bfaccf51887ac893cffc.fbx"),
    ("04_2beed08e", "2beed08eff22799e06fa32354cbe2863.fbx"),
    ("05_0434293e", "0434293e5196868c1e7037f018c8b52e.fbx"),
    ("06_d8114a2f", "d8114a2f9e26e151d26f8eac5ebbcab9.fbx"),
    ("07_7d3019fc", "7d3019fcdfeb6a7f57b8c983b9e893ef.fbx"),
    ("08_37929429", "37929429758365e2e04e78d63348fd71.fbx"),
    ("09_19b08966", "19b08966891a0630abb13956782a12c1.fbx"),
    ("10_7996719e", "7996719ee0a1bf4d2eed1c06106897ae.fbx"),
]

for label, fname in fbx_files:
    p = os.path.join(downloads, fname)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=p)
    m = bpy.data.objects.get('node_0')
    if not m:
        continue
    
    # World matrix bounds:
    world_corners = [m.matrix_world @ Vector(c) for c in m.bound_box]
    wx = [c.x for c in world_corners]
    wy = [c.y for c in world_corners]
    wz = [c.z for c in world_corners]
    
    dx = max(wx) - min(wx)
    dy = max(wy) - min(wy)
    dz = max(wz) - min(wz)
    
    print(f"[{label}] World Bounds: dx={dx:.3f}, dy={dy:.3f}, dz={dz:.3f} (Height={dz:.3f}) | Z range: [{min(wz):.3f}, {max(wz):.3f}]")
