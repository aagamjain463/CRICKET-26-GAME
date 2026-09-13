"""Original full-body cricket foundation from the project's assembled Epic MetaHuman.
Offline authoring candidate only; no match approval. Keeps complete skin geometry and individual
finger weights. Garments and footwear are weighted to the same armature; no runtime fake limbs.
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'ArtSource/Premium/Foundation';OUT=ROOT/'ArtSource/Premium/FullBody';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
def load(path):
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),use_anim=False)
 return list(set(bpy.data.objects)-before)
body_objects=load(SRC/'Body.fbx');arm=next(o for o in body_objects if o.type=='ARMATURE');body=next(o for o in body_objects if o.type=='MESH')
face_objects=load(SRC/'Face.fbx');facearm=next(o for o in face_objects if o.type=='ARMATURE');face=next(o for o in face_objects if o.type=='MESH')
# UE's FBX exporter writes raw section indices but ignores LODMaterialMap. Restore the
# exact resolved source-section assignments recorded by the native audit, before mesh joining.
manifest=json.loads((SRC/'manifest.json').read_text())
face_info=next(x for x in manifest['meshes'] if x['label']=='Face')
assert 'export_material_map' in face_info,'Run Tools/AuditFoundationMaterialMap.py first'
original_materials=list(face.data.materials)
for index,path in face_info['export_material_map'].items():
 name=path.rsplit('.',1)[-1]
 material=next((m for m in original_materials if m.name==name),None)
 assert material,'Missing exported material '+name
 face.data.materials[int(index)]=material
skin_tree=KDTree(len(body.data.vertices))
for v in body.data.vertices:skin_tree.insert(body.matrix_world@v.co,v.index)
skin_tree.balance()
# Face inherits neck/head joints. Mobile master deliberately omits face correctives/morphs;
# original assembled assets are preserved for later cinematic facial animation.
for vertex in face.data.vertices:
 weights={}
 for g in vertex.groups:
  n=face.vertex_groups[g.group].name;b=facearm.data.bones.get(n)
  while b and b.name not in arm.data.bones:b=b.parent
  target=b.name if b else 'head';weights[target]=weights.get(target,0)+g.weight
 for g in list(vertex.groups):face.vertex_groups[g.group].remove([vertex.index])
 # Neck/shoulder yoke must follow the body, not rigid facial descendants of Head.
 point=face.matrix_world@vertex.co
 if point.z<1.63:
  _,index,_=skin_tree.find(point);source_vertex=body.data.vertices[index]
  blend=max(0,min(1,(1.63-point.z)/.10));blend=blend*blend*(3-2*blend)
  weights={n:w*(1-blend) for n,w in weights.items()}
  for g in source_vertex.groups:
   n=body.vertex_groups[g.group].name;weights[n]=weights.get(n,0)+g.weight*blend
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
# Both MetaHuman parts supply skin at the shoulder yoke; Body alone ends below the neck.
bpy.ops.object.select_all(action='DESELECT')
source_parts=[]
for original in [body,face]:
 part=original.copy();part.data=original.data.copy();bpy.context.collection.objects.link(part);part.select_set(True);source_parts.append(part)
bpy.context.view_layer.objects.active=source_parts[0];bpy.ops.object.join();garment_source=bpy.context.object;garment_source.name='GarmentConstructionSkin'

def garment(name,accept,thickness,material):
 o=garment_source.copy();o.data=garment_source.data.copy();bpy.context.collection.objects.link(o);o.name=name
 bm=bmesh.new();bm.from_mesh(o.data)
 # FBX splits UV/normals seams: weld BEFORE treating boundaries as garment openings.
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0015/max(o.matrix_world.to_scale()))
 reject=[v for v in bm.verts if not accept(o.matrix_world@v.co)]
 bmesh.ops.delete(bm,geom=reject,context='VERTS');bm.normal_update()
 # Modest physical ease in the garment's surface, preserving original weights/UVs.
 inv=o.matrix_world.to_3x3().inverted()
 for v in bm.verts:
  n=(o.matrix_world.to_3x3()@v.normal).normalized();n.z*=.22
  v.co+=inv@(n*thickness)
 # Flatten garment openings to authored seam planes instead of leaving topology stair steps.
 boundary=[v for v in bm.verts if any(e.is_boundary for e in v.link_edges)]
 for v in boundary:
  p=o.matrix_world@v.co
  if name=='C26_Trousers':p.z=.075 if p.z<.3 else 1.025
  elif p.z<1.1:p.z=.975
  elif abs(p.x)>.23:
   sign=1 if p.x>0 else -1
   shoulder=arm.matrix_world@arm.data.bones['upperarm_'+('l' if sign>0 else 'r')].head_local
   elbow=arm.matrix_world@arm.data.bones['lowerarm_'+('l' if sign>0 else 'r')].head_local
   axis=(elbow-shoulder).normalized();plane=shoulder+(elbow-shoulder)*.60
   p-=axis*(p-plane).dot(axis)
  elif p.z>1.56:p.z=1.595
  v.co=o.matrix_world.inverted()@p
 # Fabric hangs across the torso rather than tracing the skin beneath the chest.
 if name=='C26_Jersey':
  for v in bm.verts:
   p=o.matrix_world@v.co
   if abs(p.x)<.20 and .99<p.z<1.46 and p.y<-.04:
    p.y=min(p.y,-.16);v.co=o.matrix_world.inverted()@p
 bm.to_mesh(o.data);bm.free();o.data.materials.clear();o.data.materials.append(material)
 for p in o.data.polygons:p.material_index=0;p.use_smooth=True
 # Imported split normals describe bare skin, not the reshaped garment surface.
 o.data.normals_split_custom_set([(0.,0.,0.)]*len(o.data.loops))
 return o
def shirt_region(p):
 if not .975<p.z<1.595:return False
 if abs(p.x)<.23:return True
 side='l' if p.x>0 else 'r';a=arm.matrix_world@arm.data.bones['upperarm_'+side].head_local;b=arm.matrix_world@arm.data.bones['lowerarm_'+side].head_local
 return (p-a).dot((b-a).normalized())<(b-a).length*.60
jersey=garment('C26_Jersey',shirt_region,.019,jersey_mat)
trousers=garment('C26_Trousers',lambda p:.075<p.z<1.025 and abs(p.x)<.32,.016,trouser_mat)
bpy.data.objects.remove(garment_source,do_unlink=True)
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
 valid={b.name for b in arm.data.bones if b.name=='foot_'+side.lower() or any(a.name=='foot_'+side.lower() for a in b.parent_recursive)}
 foot=[body.matrix_world@v.co for v in body.data.vertices if sum(g.weight for g in v.groups if body.vertex_groups[g.group].name in valid)>.65]
 assert foot,'Foot skin required for fitting'
 foot_min=Vector([min(p[i] for p in foot) for i in range(3)]);foot_max=Vector([max(p[i] for p in foot) for i in range(3)])
 desired=(foot_max-foot_min)*1.08;desired.z=max(desired.z,.13)
 dims=high-low;factors=Vector([desired[i]/dims[i] for i in range(3)])
 # Change vertices in WORLD coordinates so imported rotations/scales cannot swap length/width.
 inv=o.matrix_world.inverted();oldcenter=(low+high)*.5;center=(foot_min+foot_max)*.5;center.z=desired.z*.5-.005
 for v in o.data.vertices:
  delta=o.matrix_world@v.co-oldcenter
  v.co=inv@(center+Vector([delta[i]*factors[i] for i in range(3)]))
 bpy.context.view_layer.update()
 world=o.matrix_world.copy();o.parent=arm;o.matrix_world=world
 o.vertex_groups.clear();g=o.vertex_groups.new(name='foot_'+side.lower());g.add(list(range(len(o.data.vertices))),1,'REPLACE')
 o.modifiers.new('CanonicalSkin','ARMATURE').object=arm;shoes.append(o)
# Precise footwear occlusion: retain ankle/calf skin, remove only skin below the shoe collar.
# The fitted, skinned shoes supply the visible feet; source barefoot MetaHuman is preserved.
bm=bmesh.new();bm.from_mesh(body.data)
bmesh.ops.delete(bm,geom=[v for v in bm.verts if (body.matrix_world@v.co).z<.075],context='VERTS')
bm.to_mesh(body.data);bm.free()
# Share shoe materials across sides to avoid duplicate material draw sections.
for i,m in enumerate(shoes[1].data.materials):
 clean=m.name.rsplit('.',1)[0]
 other=next((x for x in shoes[0].data.materials if x.name==clean),None)
 if other:shoes[1].data.materials[i]=other
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
