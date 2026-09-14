"""Read-only source audit. Run with Blender --background --python this_file."""
import json
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'ArtSource/Premium/HeroHands'
DIGITS = ('thumb', 'index', 'middle', 'ring', 'pinky')


def inspect():
    OUT.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'ArtSource/Premium/FullBody/C26_Athlete_Review.blend'
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    report = {'source': str(source.relative_to(ROOT)), 'rig': rig.name,
              'bone_count': len(rig.data.bones), 'rig_scale': list(rig.scale),
              'constraints': [(p.name, c.type) for p in rig.pose.bones for c in p.constraints],
              'drivers': [d.data_path for d in rig.animation_data.drivers] if rig.animation_data else [],
              'bones': {}, 'meshes': []}
    for b in rig.data.bones:
        if any(b.name.startswith(n) for n in (*DIGITS, 'hand_', 'lowerarm', 'upperarm')):
            report['bones'][b.name] = {'head_cm': list(b.head_local), 'tail_cm': list(b.tail_local),
                'parent': b.parent.name if b.parent else None,
                'axes': [list(row) for row in b.matrix_local.to_3x3()]}
    for obj in [o for o in bpy.data.objects if o.type == 'MESH']:
        groups = {g.index: g.name for g in obj.vertex_groups}
        digits = {name: 0 for name in groups.values() if name.startswith(DIGITS)}
        totals = []
        for v in obj.data.vertices:
            hand = False
            for g in v.groups:
                name = groups[g.group]
                if name in digits and g.weight > .001:
                    digits[name] += 1
                hand |= name.startswith((*DIGITS, 'hand_')) and g.weight > .001
            if hand:
                totals.append(sum(g.weight for g in v.groups))
        report['meshes'].append({'name': obj.name, 'vertices': len(obj.data.vertices),
            'polygons': len(obj.data.polygons), 'materials': [m.name if m else None for m in obj.data.materials],
            'digit_weighted_vertices': digits, 'hand_weight_sum_range': [min(totals), max(totals)] if totals else [],
            'modifiers': [(m.name, m.type) for m in obj.modifiers]})
    (OUT / 'source-audit.json').write_text(json.dumps(report, indent=2))
    print('HAND_AUDIT', json.dumps({k: v for k, v in report.items() if k != 'bones'}))
    for n in ['hand_l', 'index_01_l', 'index_02_l', 'index_03_l', 'middle_01_l', 'pinky_01_l', 'thumb_01_l', 'thumb_02_l', 'thumb_03_l']:
        print(n, report['bones'][n])
    # Unmodified source topology in neutral studio solid shading; no material edits.
    rig.animation_data_clear()
    for p in rig.pose.bones:
        p.matrix_basis.identity()
    bpy.context.view_layer.update()
    hand = rig.matrix_world @ rig.pose.bones['hand_l'].head
    middle = rig.matrix_world @ rig.pose.bones['middle_01_l'].head
    index = rig.matrix_world @ rig.pose.bones['index_01_l'].head
    pinky = rig.matrix_world @ rig.pose.bones['pinky_01_l'].head
    center = hand.lerp(middle, .95)
    normal = (middle - hand).cross(pinky - index).normalized()
    data = bpy.data.cameras.new('AuditOnly')
    camera = bpy.data.objects.new('AuditOnly', data)
    bpy.context.collection.objects.link(camera)
    camera.location = center + normal * .6
    camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
    data.type = 'ORTHO'
    data.ortho_scale = .28
    scene = bpy.context.scene
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'SINGLE'
    scene.display.shading.single_color = (.55, .55, .55)
    scene.display.shading.show_cavity = True
    scene.render.resolution_x = scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.filepath = str(OUT / 'source-open-hand.png')
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(ROOT / 'ArtSource/Exports/Equipment/SM_C26_Bat_Hero.fbx'))
    bat = []
    for o in bpy.data.objects:
        if o.type == 'MESH':
            pts = [o.matrix_world @ v.co for v in o.data.vertices]
            bat.append({'name': o.name, 'dimensions_m': list(o.dimensions),
                        'min': [min(p[i] for p in pts) for i in range(3)],
                        'max': [max(p[i] for p in pts) for i in range(3)],
                        'materials': [m.name if m else None for m in o.data.materials]})
    report['bat'] = bat
    (OUT / 'source-audit.json').write_text(json.dumps(report, indent=2))
    print('BAT_AUDIT', json.dumps(bat))


if __name__ == '__main__':
    inspect()
