import bpy

bpy.ops.wm.open_mainfile(filepath="ArtSource/Blender/Characters/C26_MatchAthlete_v003.blend")
print("Objects in C26_MatchAthlete_v003.blend:")
for o in bpy.data.objects:
    print(f"  {o.name} (type: {o.type})")
    if o.type == 'ARMATURE':
        print(f"    Bones: {len(o.data.bones)}")
        print(f"    Root bone: {o.data.bones[0].name if o.data.bones else 'none'}")
    if o.type == 'MESH':
        print(f"    Verts: {len(o.data.vertices)}, Materials: {[m.name for m in o.data.materials if m]}")
        print(f"    Vertex groups: {len(o.vertex_groups)}")
