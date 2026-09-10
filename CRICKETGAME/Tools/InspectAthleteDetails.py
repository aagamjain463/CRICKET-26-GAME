import bpy
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
    
    # Load texture image to sample colors
    tex_path = f"ArtSource/Raw/{label}/texture_pbr_20250901.png"
    img = None
    if bpy.data.images:
        img = bpy.data.images[0]
        
    uv_layer = m.data.uv_layers.active
    
    # Sample UV colors at various body regions
    # Head: y > 0.85 * h
    # Torso: 0.55 * h < y < 0.80 * h
    # Legs: 0.15 * h < y < 0.45 * h
    # Feet: y < 0.10 * h
    max_y = max(v.co.y for v in m.data.vertices)
    min_y = min(v.co.y for v in m.data.vertices)
    h = max_y - min_y
    
    # Check arm span (X at y around 0.6*h - 0.8*h)
    arm_verts = [v.co.x for v in m.data.vertices if 0.55 * h < (v.co.y - min_y) < 0.80 * h]
    min_arm_x = min(arm_verts) if arm_verts else 0
    max_arm_x = max(arm_verts) if arm_verts else 0
    arm_span = max_arm_x - min_arm_x
    
    # Check stance (feet X span at y < 0.15*h)
    feet_verts_x = [v.co.x for v in m.data.vertices if (v.co.y - min_y) < 0.15 * h]
    min_foot_x = min(feet_verts_x) if feet_verts_x else 0
    max_foot_x = max(feet_verts_x) if feet_verts_x else 0
    feet_span = max_foot_x - min_foot_x
    
    # Check forward depth (Z span at torso vs head vs legs)
    torso_verts_z = [v.co.z for v in m.data.vertices if 0.55 * h < (v.co.y - min_y) < 0.80 * h]
    head_verts_z = [v.co.z for v in m.data.vertices if (v.co.y - min_y) > 0.85 * h]
    
    torso_z_span = (max(torso_verts_z) - min(torso_verts_z)) if torso_verts_z else 0
    head_z_span = (max(head_verts_z) - min(head_verts_z)) if head_verts_z else 0
    
    print(f"[{label}]")
    print(f"   Height: {h:.3f}m | FeetSpan: {feet_span:.3f}m | ArmSpan: {arm_span:.3f}m")
    print(f"   TorsoZSpan: {torso_z_span:.3f}m | HeadZSpan: {head_z_span:.3f}m")
    
    # Analyze if holding an object (e.g. bat sticking out, or hands together)
    # Check if there are vertices far in front or below hands
    low_front_verts = [v.co for v in m.data.vertices if v.co.z > 0.15 and (v.co.y - min_y) < 0.5 * h]
    print(f"   Low/Front protrusion verts (bat/gear): {len(low_front_verts)}")
    
