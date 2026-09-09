"""Blender: preserve the imported source scene; export a kit-specific mesh copy.

Run after importing the existing base FBX into C26_SourceAudit. This removes only
skin hidden by full-length trousers, retaining the original body in the source scene.
No animation is generated and no source FBX is overwritten.
"""
import bpy
import bmesh
import os

project = '/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME'
source = bpy.data.scenes['C26_SourceAudit']
assert 'C26_KitBase' not in bpy.data.scenes, 'Existing cleanup scene: inspect before rerunning'
target = bpy.data.scenes.new('C26_KitBase')
copies = {}
for original in source.objects:
    copy = original.copy()
    copy.data = original.data.copy()
    target.collection.objects.link(copy)
    copies[original] = copy
for original, copy in copies.items():
    if original.parent:
        copy.parent = copies[original.parent]
    for modifier in copy.modifiers:
        if modifier.type == 'ARMATURE':
            modifier.object = copies[modifier.object]

body = copies[source.objects['Body']]
leg_groups = {g.index for g in body.vertex_groups
              if any(part in g.name for part in ('UpLeg', 'Leg', 'Foot', 'Toe'))}
hidden = {v.index for v in body.data.vertices
          if sum(g.weight for g in v.groups if g.group in leg_groups) > .65}
assert 100 < len(hidden) < len(body.data.vertices) // 2, 'Unexpected skin coverage'
mesh = bmesh.new()
mesh.from_mesh(body.data)
mesh.verts.ensure_lookup_table()
bmesh.ops.delete(mesh, geom=[mesh.verts[i] for i in hidden], context='VERTS')
mesh.to_mesh(body.data)
mesh.free()
body.data.update()

bpy.context.window.scene = target
for obj in target.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = copies[source.objects['Armature']]
blend = project + '/ArtSource/Blender/Characters/C26_KitBase_v001.blend'
fbx = project + '/ArtSource/Exports/C26_KitBase_v001.fbx'
assert not os.path.exists(blend) and not os.path.exists(fbx), 'Never overwrite art source'
os.makedirs(os.path.dirname(blend), exist_ok=True)
os.makedirs(os.path.dirname(fbx), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=blend)
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True,
                         object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                         bake_anim=False, mesh_smooth_type='FACE', axis_forward='-Z', axis_up='Y')
print('C26_KIT_EXPORT removed_hidden_skin_vertices=%d body_vertices=%d path=%s'
      % (len(hidden), len(body.data.vertices), fbx))
