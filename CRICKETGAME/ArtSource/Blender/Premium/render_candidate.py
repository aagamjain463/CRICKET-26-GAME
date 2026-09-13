"""Render the actual candidate for anatomy and garment review; does not alter the saved mesh."""
import bpy,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[3]
bpy.ops.wm.open_mainfile(filepath=str(root/'ArtSource/Premium/FullBody/C26_FullBody_Candidate.blend'))
scene=bpy.context.scene
# Neutral inspection materials where FBX omitted texture references. Geometry QA only.
for mat in bpy.data.materials:
 if mat.name.startswith(('MI_Body','MI_Face_Skin')):
  mat.use_nodes=True;n=mat.node_tree.nodes.get('Principled BSDF')
  if n:
   for link in list(n.inputs['Base Color'].links):mat.node_tree.links.remove(link)
   n.inputs['Base Color'].default_value=(.43,.24,.16,1);n.inputs['Roughness'].default_value=.65;n.inputs['Metallic'].default_value=0
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.world=scene.world or bpy.data.worlds.new('ReviewWorld');scene.world.color=(.3,.3,.3)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.006));floor=bpy.context.object
m=bpy.data.materials.new('ReviewFloor');m.diffuse_color=(.11,.13,.15,1);floor.data.materials.append(m)
for loc,power,size in [((2,-3,4),500,4),((-3,-1,3),300,3),((0,3,3),400,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc);lamp=bpy.context.object;lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size;lamp.rotation_euler=(Vector((0,0,1))-lamp.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2,-4,2));cam=bpy.context.object;cam.rotation_euler=(Vector((0,-.02,.94))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=2.05;scene.camera=cam
scene.render.resolution_x=720;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.filepath=str(root/'Artifacts/CharacterAudit/full-body-review.png');bpy.ops.render.render(write_still=True)
