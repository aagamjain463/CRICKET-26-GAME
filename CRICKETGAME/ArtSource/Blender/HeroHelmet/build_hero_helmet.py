# Round-2 isolated hero batter helmet. Does NOT modify shared equipment, body, or runtime.
# Same local frame + material slot names as SM_C26_Helmet_Hero so PlaceKit/Dress work unchanged.
# Source of truth for fit: measured landmarks of the Round-1 hero head, in helmet-local cm.
import bpy, math, sys, json
from pathlib import Path
from mathutils import Matrix, Vector
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from c26_build import Builder, material, tube, export_fbx

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/HeroHelmet'
SRC = ROOT / 'ArtSource/Premium/HeroHands/C26_HeroHands_Review.blend'
DEG = math.pi/180.0

def S(a,b,x):
    t=min(1.0,max(0.0,(x-a)/(b-a))); return t*t*(3-2*t)

def landmarks(rig, face):
    Hw = rig.matrix_world
    head = rig.data.bones['head'].head_local
    skull_l = head + Vector((0,0,10.5)) + Vector((0,-1,0))*0.8
    skull_w = Hw @ skull_l
    R = Matrix.Rotation(-math.pi/2,4,'Z')
    M = np.array(R.to_3x3())
    o = np.array(skull_w)
    s = float(Hw.to_scale().x)  # 0.01
    pts = np.array([Hw @ v.co for v in face.data.vertices])  # world m
    P = ((pts - o)/s) @ M.T  # helmet-local "cm"
    X,Y,Z = P[:,0],P[:,1],P[:,2]
    nose = float(X.max()); nape = float(X.min())
    top = float(Z.max())
    chin = float(Z[(np.abs(Y)<3)&(X>nose-8)].min())
    # eye line = mean Z of eyeball verts (exact, material-anchored)
    eye_pts = []
    for poly in face.data.polygons:
        nm = face.data.materials[poly.material_index].name if len(face.data.materials)>poly.material_index else ''
        if ('Eye_Left' in nm or 'Eye_Right' in nm) and 'Shell' not in nm and 'Lash' not in nm and 'Lacrimal' not in nm:
            for vi in poly.vertices:
                eye_pts.append(face.data.vertices[vi].co)
    assert eye_pts, 'no eyeball verts found'
    eye_w = np.array([Hw @ c for c in eye_pts])
    eye_l = ((eye_w - o)/s) @ M.T
    eye_z = float(eye_l[:,2].mean())
    best_w = float(np.abs(Y[(Z>eye_z-2)&(Z<eye_z+2)]).max())
    band = P[(np.abs(Y)>7.5)&(np.abs(Z-eye_z)<6)]
    earX = (float(band[:,0].min()),float(band[:,0].max()))
    earZ = (float(band[:,2].min()),float(band[:,2].max()))
    wide = float(np.abs(Y).max())
    return {'skull_w':skull_w,'R':R,'nose':nose,'nape':nape,'top':top,'chin':chin,
            'eye_z':eye_z,'face_w':best_w,'earX':earX,'earZ':earZ,'wide':wide}

eye_z0 = -2.2

def rim_height(a, L):
    # a=0 face, +-pi/2 sides, pi rear. Brow clears eyes, sides cover ears, rear rounded.
    front = L['eye_z']+2.6; side = L['earZ'][0]-0.8; rear = -7.0
    aa = abs((a+math.pi)%(2*math.pi)-math.pi)
    if aa < math.pi/2: return front+(side-front)*S(0.15,math.pi/2,aa)
    return side+(rear-side)*S(math.pi/2,math.pi,aa)

def shell_geo(L):
    RX = max(L['nose'],abs(L['nape']))+1.3
    RY = L['wide']+1.1
    RZ = L['top']+1.1
    return RX,RY,RZ

def dome(b, L, RX,RY,RZ, sides=56, rings=10, mat=0):
    rim=[]
    for j in range(sides):
        a=2*math.pi*j/sides
        e0=math.asin(max(-0.99,min(0.99,rim_height(a,L)/RZ)))
        rim.append((a,e0))
    out=[]
    for r in range(rings):
        t=r/(rings-1.0); ring=[]
        for a,e0 in rim:
            e=e0+(math.pi/2-e0)*(t**0.9)
            w=1.0-0.20*(math.cos(a)**2) if abs(a)<1.2 or abs(a-2*math.pi)<1.2 else 1.0
            ring.append((RX*math.cos(e)*math.cos(a),RY*w*math.cos(e)*math.sin(a),RZ*math.sin(e)))
        out.append(ring)
    b.loft(out,mat)
    return out[0],(RX,RY,RZ)

def build_shell(L):
    mats=[material('M_C26_HelmetShell',(0.055,0.300,0.345),0.26),
          material('M_C26_HelmetPeak',(0.048,0.250,0.290),0.24),
          material('M_C26_HelmetTrim',(0.030,0.032,0.038),0.62),
          material('M_C26_HelmetPad',(0.052,0.050,0.048),0.88),
          material('M_C26_HelmetBar',(0.360,0.372,0.392),0.30,metallic=0.85)]
    b=Builder()
    rim,R = dome(b,L,*shell_geo(L))
    sides=len(rim)
    lip,inner=[],[]
    for j,p in enumerate(rim):
        a=2*math.pi*j/sides; o=(math.cos(a),math.sin(a),0.0)
        lip.append((p[0]+o[0]*0.5,p[1]+o[1]*0.5,p[2]-0.8))
        inner.append((p[0]*0.90,p[1]*0.90,p[2]+0.7))
    b.loft([rim,lip],2); b.loft([lip,inner],3)
    # comfort band: full inner ring + thicker brow sector
    band=[[ (p[0]*0.86,p[1]*0.86,p[2]+0.7) for p in inner],
          [ (p[0]*0.86,p[1]*0.86,p[2]+2.6) for p in inner]]
    b.loft(band,3,closed=True)
    # peak: shorter, thinner, downturned
    top,bot=[],[]
    for i in range(6):
        u=i/5.0; tr,br=[],[]
        for c in range(13):
            v=c/12.0*2-1; w=8.2*(1-0.14*u*u)
            x=7.6+u*7.2*(1-0.18*v*v); z=-0.4-u*u*2.6+(v*v)*1.2*u
            tr.append((x,v*w,z+0.3)); br.append((x,v*w,z-0.3))
        top.append(tr); bot.append(br)
    for r in range(5):
        for c in range(12):
            b.quad(top[r][c],top[r][c+1],top[r+1][c+1],top[r+1][c],1)
            b.quad(bot[r][c+1],bot[r][c],bot[r+1][c],bot[r+1][c+1],1)
    for c in range(12): b.quad(top[5][c],top[5][c+1],bot[5][c+1],bot[5][c],2)
    # crest
    RX,RY,RZ=R; arc=[]
    for i in range(25):
        t=i/24.0; ang=-math.radians(6)+t*math.radians(192)
        arc.append((RX*math.cos(ang),0.0,RZ*math.sin(ang)))
    b.loft([[(p[0]*1.004,p[1]-0.55,p[2]*1.004) for p in arc],
            [(p[0]*1.022,p[1],p[2]*1.022) for p in arc],
            [(p[0]*1.004,p[1]+0.55,p[2]*1.004) for p in arc]],0,closed=False)
    # ear guards: pad dome + trim lip at measured ear centres
    for sgn in (-1,1):
        cx=(L['earX'][0]+L['earX'][1])/2; cz=(L['earZ'][0]+L['earZ'][1])/2
        q=max(0.05,1-(cx/RX)**2-(cz/RZ)**2)
        cy=sgn*(RY*math.sqrt(q)-0.2); rx,rz=2.8,3.2
        rings=[]
        for r in range(4):
            t=r/3.0; rings.append([(cx+rx*math.cos(2*math.pi*j/14)*math.sin(t*1.25),
                cy+sgn*0.9*t,cz+rz*math.sin(2*math.pi*j/14)*math.sin(t*1.25)) for j in range(14)])
        rings[0]=[(cx,cy+sgn*0.2,cz)]*14
        b.loft(rings,3); b.loft([rings[-1],[(p[0],p[1]+sgn*0.5,p[2]) for p in rings[-1]]],2)
    top_lug_z = L['eye_z']+0.9+2.2
    # chin strap + buckle
    chX,chZ = L['nose']-2.5, L['chin']-0.5
    for sgn in (-1,1):
        path=[(6.9,sgn*11.7,top_lug_z),(9.0,sgn*9.6,-12.0),(chX,sgn*1.1,chZ)]
        b.loft(tube(path,[0.38]*len(path),8),2)
    b.loft(tube([(chX,1.1,chZ),(chX,0.2,chZ-0.2)], [0.62]*2,8),4)
    # grille mount lugs (one per side, centred on the stem/shell crossing)
    for sgn in (-1,1):
        for z in (L["eye_z"]+3.1,):
            y=sgn*11.7; x=6.9
            v=[(x-0.7,y-0.7,z-0.7),(x+0.7,y-0.7,z-0.7),(x+0.7,y+0.7,z-0.7),(x-0.7,y+0.7,z-0.7),
               (x-0.7,y-0.7,z+0.7),(x+0.7,y-0.7,z+0.7),(x+0.7,y+0.7,z+0.7),(x-0.7,y+0.7,z+0.7)]
            f=[(0,1,2,3),(4,6,5),(4,5,1,0),(3,2,6,7),(0,3,7,4),(1,5,6,2)]
            base=len(b.v); b.v.extend(v)
            for q in f: b.f.append(tuple(base+i for i in q)); b.m.append(2); b.uv.append([(0,0),(1,0),(1,1),(0,1)])
    return b.build('SM_C26_Helmet_HeroR2',mats,smooth_angle=44.0)

def build_grille(L):
    mats=[material('M_C26_HelmetBar',(0.360,0.372,0.392),0.30,metallic=0.85),
          material('M_C26_HelmetTrim',(0.030,0.032,0.038),0.62)]
    b=Builder(); span=math.radians(58)
    top=L['eye_z']+0.9; reach=L['nose']+1.9
    for i in range(5):
        z=top-i*2.6; r=reach-i*0.22; path,rad=[],[]
        for k in range(13):
            t=k/12.0*2-1; a=t*span
            path.append((r*math.cos(a)*0.98,r*math.sin(a)*1.02,z+(1-math.cos(a))*2.2))
            rad.append(0.32)
        b.loft(tube(path,rad,10),0,cap_start=path[0],cap_end=path[-1])
    for sgn in (-1,1):
        path,rad=[(6.0,sgn*10.5,top+3.4)],[0.36]
        for k in range(8):
            t=k/7.0; a=sgn*span
            path.append((reach*math.cos(a)*0.98,reach*math.sin(a)*1.02,top+1.6-t*(top+1.6-(L['chin']-1.2))+(1-math.cos(a))*2.2))
            rad.append(0.36)
        b.loft(tube(path,rad,10),1,cap_start=path[0],cap_end=path[-1])
    path=[(reach+0.15,0,top-k/7.0*(top-(L['chin']-1.2))) for k in range(8)]
    b.loft(tube(path,[0.32]*8,10),0,cap_start=path[0],cap_end=path[-1])
    chinbar=[(reach-0.4-i*0.5, y, L['chin']-1.2) for i,y in enumerate([x*0.5 for x in range(-9,10)])]
    b.loft(tube(chinbar,[0.32]*len(chinbar),8),0,cap_start=chinbar[0],cap_end=chinbar[-1])
    return b.build('SM_C26_HelmetGrille_HeroR2',mats,smooth_angle=50.0)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SRC))
    bpy.context.preferences.filepaths.save_version=0
    rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    face=next(o for o in bpy.data.objects if o.type=='MESH' and any(m and 'Face' in m.name for m in o.data.materials))
    L=landmarks(rig,face)
    print('LANDMARKS',json.dumps({k:(list(v) if k in ('skull_w',) else v) for k,v in L.items() if k!='R'}))
    helm=build_shell(L); grille=build_grille(L)
    export_fbx([helm],str(OUT/'SM_C26_Helmet_HeroR2.fbx'))
    export_fbx([grille],str(OUT/'SM_C26_HelmetGrille_HeroR2.fbx'))
    # place on head (review only; runtime PlaceKit math unchanged)
    Mw=Matrix.Translation(Vector(L['skull_w'])) @ L['R']
    for o in (helm,grille):
        o.matrix_world=Mw; bpy.context.view_layer.update()
        o.parent=rig; o.parent_type='BONE'; o.parent_bone='head'
        bpy.context.view_layer.update(); o.matrix_world=Mw; bpy.context.view_layer.update()
    rig['milestone']='HeroHelmetR2: shell/grille/strap review only'
    sc=bpy.context.scene
    sc.render.engine='BLENDER_WORKBENCH'; sc.display.shading.light='STUDIO'; sc.display.shading.color_type='OBJECT'
    for o in bpy.data.objects:
        if o.type=='MESH':
            n=o.name
            o.color=(.12,.42,.47,1) if n.startswith('SM_C26_Helmet_HeroR2') else ((.62,.64,.68,1) if 'Grille' in n else (.55,.55,.55,1))
    sc.render.resolution_x=sc.render.resolution_y=1100; sc.render.resolution_percentage=100
    col=bpy.data.collections.new('HELMETINSP'); sc.collection.children.link(col)
    tgt=Vector(L['skull_w'])+Vector((0,0,-0.02))
    views=[('H_front',(0,-1,.1),.4),('H_profile',(1,-.08,.1),.4),('H_three_quarter',(.75,-1,.42),.44),('H_closeup',(.45,-1,.12),.24),('H_stance',(2.4,-4.2,1.6),2.0)]
    for name,di,span in views:
        cd=bpy.data.cameras.new(name); co=bpy.data.objects.new(name,cd); col.objects.link(co)
        t=tgt if name!='H_stance' else Vector((0,0,.9))
        co.location=t+Vector(di).normalized()*(2.0 if name!='H_stance' else 4.5)
        co.rotation_euler=(t-co.location).to_track_quat('-Z','Y').to_euler()
        cd.type='ORTHO'; cd.ortho_scale=span
        sc.camera=co; sc.render.filepath=str(OUT/(name+'.png'))
        bpy.ops.render.render(write_still=True)
    sc.camera=bpy.data.objects['H_three_quarter']
    rep={'source':str(SRC.relative_to(ROOT)),'approved_for_match':False,
      'frame':'origin skull pivot, +X face, +Z up (PlaceKit-compatible)',
      'slots':['M_C26_HelmetShell','M_C26_HelmetPeak','M_C26_HelmetTrim','M_C26_HelmetPad','M_C26_HelmetBar'],
      'landmarks_cm':{k:(round(v,2) if isinstance(v,float) else v) for k,v in L.items() if k not in ('skull_w','R')},
      'shell_radii_cm':[round(v,2) for v in shell_geo(L)],
      'grille':{'bars':5,'cross_radius_cm':0.32,'stem_radius_cm':0.36,'sides':10,'top_offset_above_eyes_cm':0.9,'nose_clearance_cm':1.9,'max_gap_cm':2.6},
      'parent':'head bone (review); runtime: existing PlaceKit Skull/Look, no code change',
      'signed_standoff':'shell=head+1.0..1.3cm, grille nose+1.9cm'}
    (OUT/'build-report.json').write_text(json.dumps(rep,indent=2))
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'C26_HeroHelmet_Review.blend'))
    print('HELMET_DONE')

if __name__=='__main__':
    main()
