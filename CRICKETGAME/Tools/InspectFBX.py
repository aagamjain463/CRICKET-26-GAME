import bpy
import sys
import os
import json

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

report = []

for idx, fbx_path in enumerate(fbx_files):
    filename = os.path.basename(fbx_path)
    print(f"\n=======================================================")
    print(f"[{idx+1}/10] INSPECTING: {filename}")
    print(f"=======================================================")
    
    # Reset scene
    bpy.ops.wm.read_factory_settings(use_empty=True)
    
    try:
        bpy.ops.import_scene.fbx(filepath=fbx_path)
    except Exception as e:
        print(f"ERROR importing {filename}: {e}")
        report.append({"filename": filename, "error": str(e)})
        continue
        
    objs = list(bpy.context.scene.objects)
    mesh_objs = [o for o in objs if o.type == 'MESH']
    armature_objs = [o for o in objs if o.type == 'ARMATURE']
    
    total_verts = sum(len(o.data.vertices) for o in mesh_objs)
    total_polys = sum(len(o.data.polygons) for o in mesh_objs)
    
    # Bounding box & dimensions
    dims = []
    names = []
    materials = []
    textures = []
    
    for o in mesh_objs:
        names.append(o.name)
        dims.append([round(o.dimensions.x, 3), round(o.dimensions.y, 3), round(o.dimensions.z, 3)])
        for slot in o.material_slots:
            if slot.material:
                mat = slot.material
                materials.append(mat.name)
                if mat.use_nodes and mat.node_tree:
                    for node in mat.node_tree.nodes:
                        if node.type == 'TEX_IMAGE' and node.image:
                            textures.append(node.image.name)
                            # Check if embedded or filepath
                            print(f"   Texture found: {node.image.name} (filepath: {node.image.filepath}, size: {node.image.size[:]})")

    bones = []
    for a in armature_objs:
        bones.extend([b.name for b in a.data.bones])
        
    info = {
        "index": idx + 1,
        "filename": filename,
        "size_mb": round(os.path.getsize(fbx_path) / (1024*1024), 2),
        "mesh_count": len(mesh_objs),
        "mesh_names": names,
        "armature_count": len(armature_objs),
        "bone_count": len(bones),
        "bones_sample": bones[:15],
        "total_verts": total_verts,
        "total_polys": total_polys,
        "dimensions": dims,
        "materials": list(set(materials)),
        "textures": list(set(textures)),
    }
    
    print(f"Result for {filename}:")
    print(f"  Meshes: {names}")
    print(f"  Dimensions: {dims}")
    print(f"  Verts: {total_verts}, Polys: {total_polys}")
    print(f"  Armatures: {len(armature_objs)}, Bones: {len(bones)}")
    print(f"  Materials: {list(set(materials))}")
    print(f"  Textures: {list(set(textures))}")
    
    report.append(info)

with open("Tools/fbx_inspection_report.json", "w") as f:
    json.dump(report, f, indent=2)

print("\n\nAll 10 FBXs inspected and saved to Tools/fbx_inspection_report.json")
