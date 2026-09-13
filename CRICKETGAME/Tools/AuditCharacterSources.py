"""Read-only Blender geometry/skin audit. Run with Blender --background --python this-file."""
import bpy,json,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=[]
files=[ROOT/'ArtSource/Exports/C26_MatchAthlete_v003.fbx',ROOT/'ArtSource/Exports/C26_KitBase_v001.fbx']+sorted((ROOT/'ArtSource/Exports/PlayersSkeletal').glob('*.fbx'))
for path in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path),use_anim=False)
    entry={'source':str(path.relative_to(ROOT)),'meshes':[],'rigs':[]}
    for obj in bpy.context.scene.objects:
        if obj.type=='ARMATURE':
            entry['rigs'].append({'name':obj.name,'bones':len(obj.data.bones),'rest_bones':{b.name:list(obj.matrix_world@b.head_local) for b in obj.data.bones if any(s in b.name for s in ['Hips','Head','Foot','UpLeg','Hand','ForeArm'])}})
        if obj.type!='MESH':continue
        vs=[obj.matrix_world@v.co for v in obj.data.vertices]
        weights={g.name:[] for g in obj.vertex_groups}
        for v,p in zip(obj.data.vertices,vs):
            for g in v.groups:
                if g.weight>.1: weights[obj.vertex_groups[g.group].name].append(p.z)
        obj.data.calc_loop_triangles()
        entry['meshes'].append({'name':obj.name,'vertices':len(vs),'triangles':len(obj.data.loop_triangles),'bounds_m':[[min(v[i] for v in vs),max(v[i] for v in vs)] for i in range(3)],'vertices_z_below_0_8m':sum(v.z<.8 for v in vs),'unweighted':sum(not v.groups or sum(g.weight for g in v.groups)<.01 for v in obj.data.vertices),'materials':[m.name if m else None for m in obj.data.materials],'weights':{k:{'count':len(v),'z_range':[min(v),max(v)]} for k,v in weights.items() if v}})
    rows.append(entry)
out=ROOT/'Artifacts/CharacterAudit/source-geometry.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(rows,indent=2))
print('C26_SOURCE_AUDIT',out)
