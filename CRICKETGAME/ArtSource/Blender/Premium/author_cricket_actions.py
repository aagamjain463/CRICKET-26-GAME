"""Bake the cricket action library to FBX, one file per clip, plus a manifest for the importer.

Run headless:
  /Applications/Blender.app/Contents/MacOS/Blender --background \
      --python ArtSource/Blender/Premium/author_cricket_actions.py

The clips are authored on the candidate rig itself, so Unreal imports them straight onto
SK_C26_FullBody_Candidate_Skeleton with no retarget in the path.
"""
import bpy, json, sys
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'ArtSource/Blender/Premium'))
import c26_rig as rig_lib           # noqa: E402
import c26_actions as actions       # noqa: E402

OUT = ROOT / 'ArtSource/Premium/AnimationSources/Cricket'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--only', help='Comma-separated clip keys; preserve other source entries')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
selected = set(args.only.split(',')) if args.only else None
old_manifest = {entry['name']: entry for entry in json.loads((OUT / 'manifest.json').read_text())} if (OUT / 'manifest.json').exists() else {}

rig = rig_lib.load(ROOT / 'ArtSource/Premium/FullBody/C26_FullBody_Candidate.blend', keep_mesh=False)

# The FBX exporter is happiest with a skinned mesh present, and a three-vertex proxy keeps 63 files
# small. It is editor-only scaffolding and never reaches the game.
bpy.ops.object.mode_set(mode='OBJECT')
mesh = bpy.data.meshes.new('C26_AnimProxy')
mesh.from_pydata([(-0.02, 0, 0), (0.02, 0, 0), (0, 0, 0.04)], [], [(0, 1, 2)])
mesh.update()
proxy = bpy.data.objects.new('SOURCE_PROXY_NOT_PLAYER', mesh)
bpy.context.collection.objects.link(proxy)
proxy.parent = rig
proxy.matrix_parent_inverse = rig.matrix_world.inverted()
proxy.vertex_groups.new(name='root').add([0, 1, 2], 1.0, 'REPLACE')
proxy.modifiers.new('Skin', 'ARMATURE').object = rig
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='POSE')

manifest = []
for clip in actions.build_manifest():
    name = clip['name']
    if selected is not None and name not in selected:
        if name not in old_manifest:
            continue  # new clip outside this --only run: authored when selected
        manifest.append(old_manifest[name])
        continue
    action = rig_lib.bake(rig, f'A_C26_{name}', clip['keys'], loop=clip.get('loop', False),
                          dense=clip.get('dense', False))
    path = OUT / f'A_C26_{name}.fbx'
    rig_lib.export(rig, action, path)
    start, end = action.frame_range
    manifest.append({
        'name': name,
        'asset': f'A_C26_{name}',
        'fbx': str(path.relative_to(ROOT)),
        'event': clip.get('event'),
        'event_time': round(clip.get('contact', 0) / rig_lib.FPS, 4) if clip.get('event') else None,
        'loop': bool(clip.get('loop', False)),
        'ground_speed': clip.get('speed', 0.0),
        'length': round((end - start) / rig_lib.FPS, 4),
        'fps': rig_lib.FPS,
    })
    print('C26_CLIP', name, 'frames', int(start), int(end), 'event', clip.get('event'))

(OUT / 'manifest.json').write_text(json.dumps(manifest, indent=1))
print('C26_AUTHORED', len(manifest), 'clips ->', OUT)
