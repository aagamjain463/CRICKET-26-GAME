"""Original full-body cricket foundation from the project's assembled Epic MetaHuman.
Offline authoring candidate only; no match approval. Keeps complete skin geometry and individual
finger weights. Garments and footwear are weighted to the same armature; no runtime fake limbs.
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'ArtSource/Premium/Foundation';OUT=ROOT/'ArtSource/Premium/FullBody';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
def load(path):
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),use_anim=False)
 return list(set(bpy.data.objects)-before)
body_objects=load(SRC/'Body.fbx');arm=next(o for o in body_objects if o.type=='ARMATURE');body=next(o for o in body_objects if o.type=='MESH')
face_objects=load(SRC/'Face.fbx');facearm=next(o for o in face_objects if o.type=='ARMATURE');face=next(o for o in face_objects if o.type=='MESH')
# Face inherits neck/head joints. Mobile master deliberately omits face correctives/morphs;
# original assembled assets are preserved for later cinematic facial animation.
for vertex in face.data.vertices:
 weights={}
 for g in vertex.groups:
  n=face.vertex_groups[g.group].name;b=facearm.data.bones.get(n)
  while b and b.name not in arm.data.bones:b=b.parent
  target=b.name if b else 'head';weights[target]=weights.get(target,0)+g.weight
 for g in list(vertex.groups):face.vertex_groups[g.group].remove([vertex.index])
 for n,w in weights.items():
  group=face.vertex_groups.get(n) or face.vertex_groups.new(name=n);group.add([vertex.index],w,'REPLACE')
world=face.matrix_world.copy();face.parent=arm;face.matrix_world=world
for mod in face.modifiers:
 if mod.type=='ARMATURE':mod.object=arm
bpy.data.objects.remove(facearm,do_unlink=True)
def mat(name,c):
 m=bpy.data.materials.new(name);m.diffuse_color=(*c,1);m.use_nodes=True
 node=m.node_tree.nodes['Principled BSDF'];node.inputs['Base Color'].default_value=(*c,1);node.inputs['Roughness'].default_value=.84;return m
jersey_mat=mat('Jersey',(.04,.20,.35));trouser_mat=mat('Trousers',(.026,.085,.14))
def garment(name,accept,thickness,material):
 o=body.copy();o.data=body.data.copy();bpy.context.collection.objects.link(o);o.name=name
 bm=bmesh.new();bm.from_mesh(o.data)
 reject=[v for v in bm.verts if not accept(o.matrix_world@v.co)]
 bmesh.ops.delete(bm,geom=reject,context='VERTS');bm.normal_update()
 # Modest physical ease in the garment's surface, preserving original weights/UVs.
 inv=o.matrix_world.to_3x3().inverted()
 for v in bm.verts:
  n=(o.matrix_world.to_3x3()@v.normal).normalized();n.z*=.22
  v.co+=inv@(n*thickness)
 bm.to_mesh(o.data);bm.free();o.data.materials.clear();o.data.materials.append(material)
 for p in o.data.polygons:p.material_index=0;p.use_smooth=True
 return o
jersey=garment('C26_Jersey',lambda p: .975<p.z<1.515 and (abs(p.x)<.245 or p.z>1.255),.013,jersey_mat)
trousers=garment('C26_Trousers',lambda p:.095<p.z<1.025 and abs(p.x)<.32,.016,trouser_mat)
# Fit the project's original authored cricket shoes in bind pose and rigidly weight to foot bones.
shoes=[]
for side in ['L','R']:
 imported=load(ROOT/f'ArtSource/Exports/Equipment/SM_C26_Shoe_{side}.fbx')
 o=next(o for o in imported if o.type=='MESH');o.name='C26_Shoe_'+side
 # Authored shoes use +X forward. MetaHuman in this FBX faces -Y.
 o.rotation_euler=(0,0,-math.pi/2);bpy.context.view_layer.update()
 verts=[o.matrix_world@v.co for v in o.data.vertices]
 low=Vector([min(p[i] for p in verts) for i in range(3)]);high=Vector([max(p[i] for p in verts) for i in range(3)])
 target=arm.matrix_world@arm.data.bones['foot_'+side.lower()].head_local
 center=(low+high)*.5
 o.location+=Vector((target.x,-.075,.064))-center
 world=o.matrix_world.copy();o.parent=arm;o.matrix_world=world
 o.vertex_groups.clear();g=o.vertex_groups.new(name='foot_'+side.lower());g.add(list(range(len(o.data.vertices))),1,'REPLACE')
 o.modifiers.new('CanonicalSkin','ARMATURE').object=arm;shoes.append(o)
# Root bone instead of relying on FBX's armature object name as an implicit skeleton root.
bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
if 'root' not in arm.data.bones:
 bpy.ops.object.mode_set(mode='EDIT');r=arm.data.edit_bones.new('root');r.head=(0,0,0);r.tail=(0,0,.06)
 for b in arm.data.edit_bones:
  if b!=r and b.parent is None:b.parent=r
 bpy.ops.object.mode_set(mode='OBJECT')
arm.name='Armature'
keep=[arm,body,face,jersey,trousers]+shoes
# Bake ONE complete bind mesh; the original body/face source assets remain untouched.
bpy.ops.object.select_all(action='DESELECT')
for o in keep:o.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'C26_FullBody_Candidate.blend'))
bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_C26_FullBody_Candidate.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,use_mesh_modifiers=False)
report={'status':'candidate_requires_visual_approval','source':'assembled original MetaHuman + original project cricket shoes','parts':[]}
for o in keep:
 if o.type=='MESH':
  o.data.calc_loop_triangles();report['parts'].append({'name':o.name,'vertices':len(o.data.vertices),'triangles':len(o.data.loop_triangles),'unweighted':sum(not v.groups for v in o.data.vertices)})
(OUT/'build-report.json').write_text(json.dumps(report,indent=2));print('C26_FULL_BODY',report)
