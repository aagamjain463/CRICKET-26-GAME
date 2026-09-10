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
    m = bpy.data.objects['node_0']
    bpy.context.view_layer.objects.active = m
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    
    verts = [Vector(v.co) for v in m.data.vertices]
    min_x, max_x = min(v.x for v in verts), max(v.x for v in verts)
    min_y, max_y = min(v.y for v in verts), max(v.y for v in verts)
    min_z, max_z = min(v.z for v in verts), max(v.z for v in verts)
    
    # Calculate aspect ratio of stance
    width = max_x - min_x
    depth = max_y - min_y
    height = max_z - min_z
    
    print(f"[{label}] W={width:.2f} D={depth:.2f} H={height:.2f} ratio W/H={width/height:.2f} D/H={depth/height:.2f}")
