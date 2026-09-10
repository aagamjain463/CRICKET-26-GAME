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
    
    # Check Y slices (height from 0 to max_y)
    max_y = max(v.co.y for v in m.data.vertices)
    min_y = min(v.co.y for v in m.data.vertices)
    h = max_y - min_y
    
    # Sample width (X) and depth (Z) at 10%, 30%, 50%, 70%, 90% of height
    slices = [0.1, 0.3, 0.5, 0.7, 0.9]
    slice_info = []
    
    for s in slices:
        y_val = min_y + h * s
        tol = h * 0.02
        verts_in_slice = [v.co for v in m.data.vertices if abs(v.co.y - y_val) < tol]
        if verts_in_slice:
            xs = [v.x for v in verts_in_slice]
            zs = [v.z for v in verts_in_slice]
            span_x = max(xs) - min(xs)
            span_z = max(zs) - min(zs)
            slice_info.append(f"y={s*100:.0f}%: wx={span_x:.2f}, dz={span_z:.2f}")
        else:
            slice_info.append(f"y={s*100:.0f}%: empty")
            
    print(f"[{label}] Height={h:.3f}m | Slices: {' | '.join(slice_info)}")
