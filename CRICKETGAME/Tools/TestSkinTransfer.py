import bpy
import bmesh
from mathutils import Vector, Matrix
from mathutils.kdtree import KDTree
import os

BASE_BLEND = "ArtSource/Blender/Characters/C26_MatchAthlete_v003.blend"
SOURCE_FBX = "/Users/aagamjain/Downloads/d8114a2f9e26e151d26f8eac5ebbcab9.fbx"

# 1. Open the base athlete blend to get the reference armature and body mesh
bpy.ops.wm.open_mainfile(filepath=BASE_BLEND)
arm = bpy.data.objects.get('Armature')
ref_body = bpy.data.objects.get('Body.001') or bpy.data.objects.get('Body')
assert arm and ref_body, f"Base rig not found: arm={arm}, body={ref_body}"

# Record bounds of reference body
ref_verts = [arm.matrix_world @ v.co for v in ref_body.data.vertices]
ref_min_z = min(v.z for v in ref_verts)
ref_max_z = max(v.z for v in ref_verts)
ref_height = ref_max_z - ref_min_z
print(f"Reference body height: {ref_height:.3f}m, bounds Z: [{ref_min_z:.3f}, {ref_max_z:.3f}]")

# Build KDTree of reference body for skin weight transfer
print("Building KDTree from reference body...")
kd = KDTree(len(ref_body.data.vertices))
for v in ref_body.data.vertices:
    kd.insert(v.co, v.index)
kd.balance()

# 2. Import the new FBX asset
print(f"Importing {SOURCE_FBX}...")
bpy.ops.import_scene.fbx(filepath=SOURCE_FBX)
new_objs = [o for o in bpy.context.selected_objects if o.type == 'MESH']
assert new_objs, "No mesh imported from FBX"
new_mesh = new_objs[0]
new_mesh.name = "Hero_Batter_LOD0"

# Apply decimation from 1.5M to ~35k tris (ratio ~0.024)
print(f"Original verts: {len(new_mesh.data.vertices)}")
dec = new_mesh.modifiers.new("Decimate", "DECIMATE")
dec.ratio = 0.035
bpy.context.view_layer.objects.active = new_mesh
bpy.ops.object.modifier_apply(modifier="Decimate")
print(f"Decimated verts: {len(new_mesh.data.vertices)}")

print("Decimation test successful!")
