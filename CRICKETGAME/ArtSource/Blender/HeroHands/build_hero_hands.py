# Isolated hero-hands grip study. Original skeleton, weights, mesh, bat scale untouched.
# Manual anatomically-driven curls (c26_rig local-X convention) + two-bone arms + bone-relative bat.
# No shared code or runtime modified.
import json, math, sys
from pathlib import Path
import bpy
from mathutils import Matrix, Quaternion, Vector, Euler
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/HeroHands'
SOURCE = ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend'
BAT = ROOT / 'ArtSource/Exports/Equipment/SM_C26_Bat_Hero.fbx'
DEG = math.pi/180.0

FINGERS = {
 'index':  (52, 68, 36, 3),
 'middle': (55, 70, 38, 1),
 'ring':   (53, 69, 37, -2),
 'pinky':  (48, 64, 34, -4),
}
THUMB = (12, 10, 8)
THUMB_OPP = 24
CANT = {'l': 18.0, 'r': 24.0}
GRIP_TOP = Vector((3.0, -23.0, 114.0))
HAND_GAP = 8.3
SHAFT = Vector((0,0,1))

def update():
    bpy.context.view_layer.update()

def anatomical_frame(rig, side):
    B = rig.data.bones
    wrist = B['hand_'+side].head_local.copy()
    fwd = (B['middle_01_'+side].head_local - wrist).normalized()
    ac = B['pinky_01_'+side].head_local - B['index_01_'+side].head_local
    ac = (ac - fwd*ac.dot(fwd)).normalized()
    palm = (fwd.cross(ac)).normalized()
    if palm.dot(B['thumb_02_'+side].head_local - wrist) < 0:
        palm.negate()
    return wrist, fwd, ac, palm

def pose_fingers(rig, side):
    # PROBED: local X = twist (no-op), Y = spread, Z = flexion on this FBX rig.
    for d,(a1,a2,a3,spread) in FINGERS.items():
        for i,ang in enumerate((a1,a2,a3),1):
            pb = rig.pose.bones.get(d+'_0'+str(i)+'_'+side)
            if not pb: continue
            pb.rotation_mode='XYZ'
            pb.rotation_euler = Euler((0, (spread*DEG if i==1 else 0), ang*DEG), 'XYZ')
    sgn = -1 if side=='l' else 1
    for i,ang in enumerate(THUMB,1):
        pb = rig.pose.bones.get('thumb_0'+str(i)+'_'+side)
        if not pb: continue
        pb.rotation_mode='XYZ'
        if i==1:
            pb.rotation_euler = Euler((0, sgn*THUMB_OPP*DEG, ang*DEG), 'XYZ')
        else:
            pb.rotation_euler = Euler((0, 0, ang*DEG), 'XYZ')
    update()

def aim(rig, bone, child, target):
    update()
    pb = rig.pose.bones[bone]
    head = pb.head.copy()
    q = (rig.pose.bones[child].head-head).rotation_difference(Vector(target)-head)
    pb.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ pb.matrix
    update()

def arm_to(rig, side, target, pole):
    u = 'upperarm_'+side; l = 'lowerarm_'+side; h = 'hand_'+side
    a = rig.pose.bones[u].head.copy()
    l1 = (rig.data.bones[l].head_local-rig.data.bones[u].head_local).length
    l2 = (rig.data.bones[h].head_local-rig.data.bones[l].head_local).length
    d = Vector(target)-a
    assert d.length < (l1+l2)*0.999, side+' wrist out of reach'
    dist = d.length; dn = d.normalized()
    ca = max(-1,min(1,(l1*l1+dist*dist-l2*l2)/(2*l1*dist)))
    axis = dn.cross(Vector(pole)-a)
    if axis.length < 1e-6: axis = dn.cross(Vector((0,1,0)))
    axis.normalize()
    elbow = a + (Quaternion(axis, math.acos(ca)) @ dn)*l1
    aim(rig,u,l,elbow); aim(rig,l,h,target)

def render_all(rig, bat, center):
    sc = bpy.context.scene
    sc.render.engine='BLENDER_WORKBENCH'
    sc.display.shading.light='STUDIO'; sc.display.shading.color_type='OBJECT'
    sc.display.shading.show_cavity=True
    sc.render.resolution_x=sc.render.resolution_y=1200
    sc.render.resolution_percentage=100
    for o in bpy.data.objects:
        if o.type=='MESH':
            o.color=(.52,.52,.52,1) if o!=bat else (.16,.16,.16,1)
    col = bpy.data.collections.new('INSPECTION_ONLY'); sc.collection.children.link(col)
    views=[('01_front',(-0.25,-1,.12),.30),('02_back',(1,.35,.25),.30),('03_thumb_side',(1,-.15,.12),.30),('04_three_quarter',(1,-1,.6),.32),('05_player',(2.2,-4.5,1.8),2.0)]
    for name,di,span in views:
        cd=bpy.data.cameras.new(name); co=bpy.data.objects.new(name,cd); col.objects.link(co)
        tgt=center if name!='05_player' else Vector((0,0,.9))
        co.location=tgt+Vector(di).normalized()*3
        co.rotation_euler=(tgt-co.location).to_track_quat('-Z','Y').to_euler()
        cd.type='ORTHO'; cd.ortho_scale=span
        sc.camera=co; sc.render.filepath=str(OUT/(name+'.png'))
        bpy.ops.render.render(write_still=True)
    sc.camera=bpy.data.objects['04_three_quarter']

def qa_proximity(rig, body, bat):
    dg=bpy.context.evaluated_depsgraph_get(); dg.update()
    bm=bat.evaluated_get(dg).to_mesh()
    verts=[bat.matrix_world @ v.co for v in bm.vertices]
    inv=rig.matrix_world.inverted()
    bv=BVHTree.FromPolygons([inv @ v for v in verts],[tuple(p.vertices) for p in bm.polygons],all_triangles=False,epsilon=1e-6)
    groups={g.index:g.name for g in body.vertex_groups}
    rep={'min_dist_cm':1e9,'per_digit':{}}
    ev=body.evaluated_get(dg); M=ev.to_mesh()
    for d in ('index','middle','ring','pinky','thumb'):
        ids=[v.index for v in body.data.vertices if sum(g.weight for g in v.groups if groups[g.group].startswith(d+'_0'))>0.5]
        if not ids: continue
        worst=1e9
        for i in ids[::3]:
            p=inv @ body.matrix_world @ M.vertices[i].co
            hit=bv.find_nearest(p)
            if hit[0] is not None: worst=min(worst,hit[2])
        rep['per_digit'][d]={'min_dist_cm':worst,'samples':len(ids[::3])}
        rep['min_dist_cm']=min(rep['min_dist_cm'],worst)
    bat.evaluated_get(dg).to_mesh_clear(); body.evaluated_get(dg).to_mesh_clear()
    return rep

def build():
    OUT.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    bpy.context.preferences.filepaths.save_version=0
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    body=next(o for o in bpy.data.objects if o.type=='MESH' and any(m and m.name.startswith('MI_Body') for m in o.data.materials))
    rig.animation_data_clear()
    for pb in rig.pose.bones: pb.matrix_basis.identity()
    update()
    frames={s:anatomical_frame(rig,s) for s in 'lr'}
    for s in 'lr': pose_fingers(rig,s)
    for side in 'lr':
        wrist,fwd,ac,palm=frames[side]
        cant=CANT[side]*DEG; sgn=-1 if side=='l' else 1
        t_ac=Vector((-sgn*math.sin(cant),0,-math.cos(cant)))
        t_fw=Vector((sgn*math.cos(cant),0,-math.sin(cant)))
        src=Matrix((fwd,ac,palm)).transposed()
        t_pa=t_fw.cross(t_ac)*src.determinant()
        R=Matrix((t_fw,t_ac,t_pa)).transposed() @ src.inverted()
        assert R.determinant()>.99
        cyl=wrist+fwd*6.7+palm*2.9
        ctr=GRIP_TOP+SHAFT*(0 if side=='l' else -HAND_GAP)
        wt=ctr - R @ (cyl-wrist)
        arm_to(rig,side,wt,wt-t_fw*25+Vector((0,0,4)))
        pb=rig.pose.bones['hand_'+side]
        pb.matrix=Matrix.Translation(wt) @ (R @ pb.bone.matrix_local.to_3x3()).to_4x4()
        update()
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(BAT))
    bat=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
    bat.name='C26_HeroHands_Bat'
    bat_world=Matrix.Translation(rig.matrix_world @ (GRIP_TOP+SHAFT*3.0)) @ Matrix.Rotation(-math.pi/2,4,'Z')
    bat.matrix_world=bat_world; update()
    hw=rig.matrix_world @ rig.pose.bones['hand_l'].matrix
    off=hw.inverted() @ bat_world
    bat.parent=rig; bat.parent_type='BONE'; bat.parent_bone='hand_l'
    update(); bat.matrix_world=bat_world; update()
    rig.name='C26_HeroHands_Rig'; body.name='C26_HeroHands_Body'
    rig['milestone']='HeroHands neutral two-hand grip; original skeleton/weights'
    bpy.context.scene.frame_start=bpy.context.scene.frame_end=1
    center=rig.matrix_world @ (GRIP_TOP-SHAFT*4.0)
    render_all(rig,bat,center)
    try:
        q=qa_proximity(rig,body,bat)
    except Exception as e:
        q={'qa_error':str(e)}
    report={'source':str(SOURCE.relative_to(ROOT)),'approved_for_match':False,'skeleton_changed':False,'weights_changed':False,'mesh_changed':False,'finger_pose_deg':{k:{'MCP':v[0],'PIP':v[1],'DIP':v[2],'splay':v[3]} for k,v in FINGERS.items()},'thumb_deg':{'flex':list(THUMB),'opposition':THUMB_OPP},'cant_deg':CANT,'grip_top_cm':list(GRIP_TOP),'hand_gap_cm':HAND_GAP,'bat':str(BAT.relative_to(ROOT)),'bat_scale_changed':False,'bat_attachment_bone':'hand_l','bat_offset_in_hand_space':[list(r) for r in off],'qa':q}
    (OUT/'build-report.json').write_text(json.dumps(report,indent=2))
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'C26_HeroHands_Review.blend'))
    bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); body.select_set(True); bat.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_C26_HeroHands_Grip.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,apply_unit_scale=True,use_mesh_modifiers=False)
    print('HERO_HANDS_DONE')

if __name__=='__main__':
    build()
