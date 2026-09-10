"""Original lightweight cricket garments, authored on the shipped 67-bone rig.
Run via Blender MCP after opening C26_KitBase_v001.blend. Never overwrites the base.
"""
import bpy, math, os
from mathutils import Vector, Matrix
from mathutils.kdtree import KDTree
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def material(name, colour):
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color=(*colour,1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*colour,1);p.inputs['Roughness'].default_value=.82
    return m

def loft(name, rows, axis='Y', sides=32):
    v=[];f=[]
    for x,y,z,rx,rz in rows:
        for j in range(sides):
            a=2*math.pi*j/sides
            if axis=='Y': v.append((x+rx*math.cos(a),y,z+rz*math.sin(a)))
            else: v.append((x,y+rx*math.cos(a),z+rz*math.sin(a)))
    for r in range(len(rows)-1):
        for j in range(sides):
            a=r*sides+j;b=r*sides+(j+1)%sides
            f.append((a,b,b+sides,a+sides))
    f.extend([tuple(reversed(range(sides))),tuple((len(rows)-1)*sides+j for j in range(sides))])
    me=bpy.data.meshes.new(name);me.from_pydata(v,[],f);me.update()
    import bmesh
    bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
    o=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(o)
    return o

def union(parts,name,voxel=2.0,relax=4):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join()
    o=parts[0];o.name=name
    rem=o.modifiers.new('Stitched garment volume','REMESH');rem.mode='VOXEL';rem.voxel_size=voxel;rem.use_smooth_shade=True
    bpy.ops.object.modifier_apply(modifier=rem.name)
    smooth=o.modifiers.new('Cloth surface relaxation','SMOOTH');smooth.factor=.6;smooth.iterations=relax
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    dec=o.modifiers.new('Mobile garment topology','DECIMATE');dec.ratio=.24
    bpy.ops.object.modifier_apply(modifier=dec.name)
    for p in o.data.polygons:p.use_smooth=True
    return o

def weight(o,arm,source=None):
    groups={b.name:o.vertex_groups.new(name=b.name) for b in arm.data.bones}
    if source:
        kd=KDTree(len(source.data.vertices))
        for v in source.data.vertices:kd.insert(v.co,v.index)
        kd.balance()
        for v in o.data.vertices:
            weights={}
            for co,i,d in kd.find_n(v.co,4):
                influence=1/max(d,.25)**2
                for g in source.data.vertices[i].groups:
                    n=source.vertex_groups[g.group].name
                    if n in groups:weights[n]=weights.get(n,0)+g.weight*influence
            best=sorted(weights.items(),key=lambda x:-x[1])[:4];total=sum(w for n,w in best)
            for n,w in best:groups[n].add([v.index],w/total,'REPLACE')
    else:
        for v in o.data.vertices:
            x,y,z=v.co;side='Left' if x>=0 else 'Right'
            if y>193:weights={'Hips':1}
            elif y>168:
                a=(193-y)/25;weights={'Hips':1-a,side+'UpLeg':a}
            elif y>130:weights={side+'UpLeg':1}
            elif y>92:
                a=(130-y)/38;weights={side+'UpLeg':1-a,side+'Leg':a}
            else:weights={side+'Leg':1}
            for n,w in weights.items():
                if w>0:groups['mixamorig:'+n].add([v.index],w,'REPLACE')
    o.parent=arm;o.matrix_parent_inverse=Matrix.Identity(4);o.matrix_basis=Matrix.Identity(4)
    o.modifiers.new('Shipped skeleton','ARMATURE').object=arm
    # Simple cylindrical UVs, useful for fabric scale and future team textures.
    uv=o.data.uv_layers.new(name='KitUV')
    for p in o.data.polygons:
        for li in p.loop_indices:
            co=o.data.vertices[o.data.loops[li].vertex_index].co
            uv.data[li].uv=(.5+math.atan2(co.z,co.x)/(2*math.pi),co.y/380)

def build():
    arm=bpy.data.objects['Armature.001'];scene=next(s for s in bpy.data.scenes if arm.name in s.objects)
    # Idempotent: this runs against a scene that may already carry a previous pass's garments.
    for stale in ['C26_Jersey','C26_Trousers']:
        o=bpy.data.objects.get(stale)
        if o:bpy.data.objects.remove(o,do_unlink=True)
    window=bpy.context.window or next(w for wm in bpy.data.window_managers for w in wm.windows)
    window.scene=scene;bpy.context.view_layer.update()
    top=bpy.data.objects['Tops.001']
    # Rows are measured off the shipped skin, not estimated. Sampling Body.001 in 4-unit bands
    # gives a half-width of 32.5 across the trapezius at y=310-314 and 18.6 at the neck base
    # (y=318); the shoulder joint sits at x=38.6, y=299.9. The first version of this garment
    # narrowed to 15 units by y=309, so from y=306 upward the shirt was inside the body and the
    # skin punched straight through it: every athlete rendered with bare shoulders and two
    # detached sleeve caps floating on the deltoids. The yoke below covers the shelf.
    torso=loft('Jersey torso',[(0,204,0,34,24),(0,216,-1,35,25),(0,244,-3,37,25),(0,272,-5,43,26),
                               (0,289,-7,46,26),(0,300,-8,46,25),(0,306,-7,43,21),(0,311,-6,35,18),
                               (0,316,-5,24,15),(0,322,-4,19,13)])
    sleeves=[]
    for sign in [-1,1]:
        # The inboard ring is deliberately buried inside the torso so the voxel remesh fuses the
        # sleeve into the yoke as one shoulder. Starting it outboard of the joint, as before, left
        # the union with nothing to weld and produced the floating cap.
        # Vertical radii are held under the torso's own profile at the same x, so the sleeve never
        # pushes a ridge above the shoulder line -- which is what shoulder pads look like.
        sleeves.append(loft('Jersey sleeve',[(sign*24,296,-9,20,23),(sign*42,297,-10,19,21),(sign*60,298,-10,16,18),(sign*78,299,-10,15,16)],axis='X',sides=24))
    # A garment that has to read at replay close-up gets two more relaxation passes than the
    # trousers do: the voxel weld between sleeve and yoke leaves a seam the smooth pass removes.
    jersey=union([torso]+sleeves,'C26_Jersey',2.1,relax=6)
    # Open neck and sleeve cuffs; the skin underneath is the shipped head/forearms.
    import bmesh
    bm=bmesh.new();bm.from_mesh(jersey.data)
    for co,no in [((76,0,0),(1,0,0)),((-76,0,0),(-1,0,0)),((0,206,0),(0,-1,0))]:
        bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.001,plane_co=co,plane_no=no,clear_outer=True)
    bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.co.y>318 and abs(v.co.x)<22],context='VERTS')
    bm.to_mesh(jersey.data);bm.free()
    jersey.data.materials.append(material('Jerseymat',(.03,.345,.395)))
    weight(jersey,arm,top)
    pelvis=loft('Trouser waist',[(0,213,-1,35,25),(0,198,0,36,26),(0,185,1,32,24),(0,176,1,26,20)])
    legs=[]
    for s in [-1,1]:
        legs.append(loft('Trouser leg',[(s*18.4,196,1,21,25),(s*18.4,177,1,21,23),(s*18.2,152,0,19,21),(s*18.1,126,0,17,18),(s*18.1,110,-.3,16.5,17),(s*18.3,91,-1,16,16.5),(s*18.4,63,-3,14.5,15),(s*18.5,31,-5,12.5,13)],sides=24))
    trouser=union([pelvis]+legs,'C26_Trousers',1.9)
    trouser.data.materials.append(material('Trousermat',(.56,.545,.50)));weight(trouser,arm)
    # Dedicated eye material avoids recolouring eyeballs as skin in the runtime kit bind.
    eyes=bpy.data.objects.get('Eyes.001')
    if eyes:eyes.data.materials.clear();eyes.data.materials.append(material('Eyesmat',(.26,.20,.16)))
    # Select ONLY the intended rig, skin and authored kit. The source includes other audit scenes.
    keep=[arm,bpy.data.objects['Body.001'],bpy.data.objects['Eyes.001'],bpy.data.objects['Eyelashes.001'],jersey,trouser]
    for o in scene.objects:o.hide_set(o not in keep)
    for o in keep:o.hide_set(False)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    for o in keep:o.select_set(True)
    bpy.context.view_layer.objects.active=arm
    export=ROOT+'/Exports/C26_MatchAthlete_v003.fbx'
    bpy.ops.export_scene.fbx(filepath=export,use_selection=True,apply_unit_scale=True,global_scale=1.0,object_types={'MESH','ARMATURE'},mesh_smooth_type='FACE',use_mesh_modifiers=False,add_leaf_bones=False,bake_anim=False,bake_space_transform=False,use_armature_deform_only=False)
    bpy.ops.wm.save_as_mainfile(filepath=ROOT+'/Blender/Characters/C26_MatchAthlete_v003.blend')
    for o in [jersey,trouser]:
        o.data.calc_loop_triangles();print('C26_MATCH_KIT',o.name,len(o.data.vertices),len(o.data.loop_triangles), 'unweighted',sum(not v.groups for v in o.data.vertices))
    print('C26_MATCH_KIT_EXPORTED',export)
    return keep
