"""Original Eclipse Oval modules. Execute in the connected Blender; metres, Z up.

Creates its own scene without modifying the open character scene. Exports selected
meshes only. No third-party content, collision, skeletal rigs or paid generation.
"""
import bpy
import math
import os
from mathutils import Vector

ROOT = '/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME'
scene = bpy.data.scenes.new('C26_EclipseOval_World_v001')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
out = os.path.join(ROOT, 'ArtSource/Exports/Stadium')
os.makedirs(out, exist_ok=True)


class Mesh:
    def __init__(self):
        self.v, self.f, self.c = [], [], []

    def face(self, points, color):
        start = len(self.v)
        self.v.extend(points)
        self.f.append(tuple(range(start, start + len(points))))
        self.c.extend([color] * len(points))

    def box(self, center, size, color=(1, 1, 1, 1)):
        p = Vector(center)
        x, y, z = [s / 2 for s in size]
        pts = [p + Vector((a*x, b*y, c*z)) for a,b,c in
               [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            self.face([pts[i] for i in ids], color)

    def rod(self, a, b, radius, sides=6, color=(1,1,1,1), end_radius=None):
        a,b = Vector(a),Vector(b)
        z = (b-a).normalized()
        x = z.cross(Vector((0,1,0)) if abs(z.y)<.9 else Vector((1,0,0))).normalized()
        y = z.cross(x)
        rings = [[p + (x*math.cos(i*math.tau/sides)+y*math.sin(i*math.tau/sides))*r
                  for i in range(sides)] for p,r in [(a,radius),(b,end_radius or radius)]]
        for i in range(sides):
            j=(i+1)%sides
            self.face([rings[0][i],rings[0][j],rings[1][j],rings[1][i]],color)
        self.face(list(reversed(rings[0])),color)
        self.face(rings[1],color)

    def export(self, name):
        mesh=bpy.data.meshes.new(name)
        mesh.from_pydata(self.v,[],self.f)
        mesh.update()
        ob=bpy.data.objects.new(name,mesh)
        scene.collection.objects.link(ob)
        colors=mesh.color_attributes.new(name='Color',type='FLOAT_COLOR',domain='POINT')
        for i,c in enumerate(self.c): colors.data[i].color=c
        # Box projection UVs: packed source mesh suitable for a shared material.
        uv=mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            axis=max(range(3),key=lambda i:abs(poly.normal[i]))
            pair=[i for i in range(3) if i!=axis]
            for li in poly.loop_indices:
                p=mesh.vertices[mesh.loops[li].vertex_index].co
                uv.data[li].uv=(p[pair[0]]*.5,p[pair[1]]*.5)
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active=ob
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(out,name+'.fbx'),use_selection=True,
            object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
            axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',
            add_leaf_bones=False,bake_anim=False)
        mesh.calc_loop_triangles()
        print('C26_MESH',name,'triangles',len(mesh.loop_triangles),'metres',tuple(round(x,3) for x in ob.dimensions))
        return ob


# Four-chord tapered lattice, cross-braced in 4 m bays, with catwalk and lamp yoke.
tower=Mesh()
def chord(corner,z):
    width=2.35+(0.85-2.35)*min(z/40,1)
    return (corner[0]*width,corner[1]*width,z)
corners=[(-1,-1),(1,-1),(1,1),(-1,1)]
for c in corners:
    tower.box((c[0]*2.35,c[1]*2.35,.25),(1.15,1.15,.5))
    tower.rod(chord(c,.4),chord(c,40),.16,8,end_radius=.085)
for k in range(10):
    lo,hi=.7+k*3.9,.7+(k+1)*3.9
    for j,c in enumerate(corners):
        d=corners[(j+1)%4]
        tower.rod(chord(c,lo),chord(d,hi),.055,5)
        tower.rod(chord(d,lo),chord(c,hi),.055,5)
        tower.rod(chord(c,hi),chord(d,hi),.065,5)
tower.box((0,0,39.9),(10.0,2.4,.22))
for x in [-4.8,4.8]:
    tower.rod((x,1.1,40),(x,1.1,41.1),.045,6)
tower.rod((-4.8,1.1,41.1),(4.8,1.1,41.1),.045,6)
for z in [40.6,42.1,43.6,45.1]: tower.box((0,.1,z),(10,.16,.14))
for x in [-4.85,0,4.85]: tower.box((x,.1,42.8),(.16,.28,4.8))
# Ladder and two intermediate service platforms.
for z in [16,28]: tower.box((0,0,z),(3.7,3.7,.16))
for x in [-.25,.25]: tower.rod((x,1,1),(x,1,39.7),.035,5)
for k in range(98): tower.rod((-.25,1,.7+k*.4),(.25,1,.7+k*.4),.022,4)
tower.export('SM_Eclipse_FloodlightTower')
lamps=Mesh()
housings=Mesh()
for row in range(5):
    for col in range(10):
        p=((col-4.5)*.96,-.19,40.9+row*.9)
        housings.box((p[0],p[1]+.08,p[2]),(.86,.38,.72))
        lamps.box((p[0],p[1]-.14,p[2]),(.74,.04,.58))
housings.export('SM_Eclipse_LampHousing')
lamps.export('SM_Eclipse_LampArray')

# One seated adult with silhouette-readable head, shoulders, thighs and lower legs.
# Vertex alpha masks the shirt for per-instance clothing variation in Unreal.
person=Mesh()
skin=(.42,.255,.155,0)
trousers=(.035,.046,.065,0)
shirt=(1,1,1,1)
person.rod((0,0,.53),(0,.02,.98),.225,5,shirt,end_radius=.195)
person.rod((0,.02,1.035),(0,.015,1.265),.105,6,skin,end_radius=.085)
for side in [-1,1]:
    person.rod((side*.20,0,.93),(side*.25,-.15,.62),.068,4,shirt)
    person.rod((side*.25,-.15,.62),(side*.14,-.29,.59),.052,4,skin)
    person.rod((side*.12,0,.52),(side*.12,-.33,.49),.095,4,trousers)
    person.rod((side*.12,-.33,.49),(side*.12,-.35,.09),.067,4,trousers)
person.export('SM_Eclipse_SeatedSpectator')
seat=Mesh()
seat.box((0,0,.45),(.44,.43,.07))
seat.box((0,.20,.70),(.44,.07,.45))
seat.export('SM_Eclipse_StadiumSeat')

# Low trapezoidal boundary pad, 2.8 m long, white rope stays at the scoring edge.
pad=Mesh()
v=[(-1.4,-.22,0),(1.4,-.22,0),(1.4,.22,0),(-1.4,.22,0),
   (-1.4,-.12,.25),(1.4,-.12,.25),(1.4,.12,.25),(-1.4,.12,.25)]
for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
    pad.face([Vector(v[i]) for i in ids],(1,1,1,1))
pad.export('SM_Eclipse_BoundaryCushion')
bpy.data.libraries.write(os.path.join(ROOT,'ArtSource/Blender/Stadium/C26_EclipseOval_World_v001.blend'),{scene},path_remap='RELATIVE',fake_user=True,compress=True)
print('C26_WORLD_MESHES_COMPLETE')
