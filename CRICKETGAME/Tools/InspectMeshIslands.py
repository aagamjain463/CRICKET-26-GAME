import bpy
import bmesh
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
    
    m = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    
    # Separate by loose parts
    bpy.context.view_layer.objects.active = m
    m.select_set(True)
    bpy.ops.mesh.separate(type='LOOSE')
    
    parts = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    parts.sort(key=lambda o: len(o.data.vertices), reverse=True)
    
    print(f"\n==========================================")
    print(f"ASSET: {label} -> Total Loose Parts: {len(parts)}")
    for i, p in enumerate(parts[:8]):
        dims = p.dimensions
        bbox = [p.matrix_world @ Vector(b) for b in p.bound_box]
        min_y = min(b.y for b in bbox)
        max_y = max(b.y for b in bbox)
        print(f"   Part {i+1}: {len(p.data.vertices):6d} verts | Dims: ({dims.x:.2f}, {dims.y:.2f}, {dims.z:.2f}) | Y: [{min_y:.2f}, {max_y:.2f}]")

