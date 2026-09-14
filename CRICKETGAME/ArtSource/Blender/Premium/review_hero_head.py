"""Head-only geometry evidence. Temporary clay/eye displays; never saves the scene.

Blender --background --python review_hero_head.py -- --source <blend> --out <dir>
The inspection camera and display materials exist only in this background process.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def bounds(points):
    return [[min(p[a] for p in points) for a in range(3)],
            [max(p[a] for p in points) for a in range(3)]]


def display_material(name, color, roughness=.65):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


def eye_display(name, center):
    """Continuous neutral iris marker on the actual globe, for socket inspection."""
    mat = display_material(name, (.65, .64, .59), .32)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    geometry = nodes.new('ShaderNodeNewGeometry')
    offset = nodes.new('ShaderNodeVectorMath')
    offset.operation = 'SUBTRACT'
    offset.inputs[1].default_value = center
    links.new(geometry.outputs['Position'], offset.inputs[0])
    planar = nodes.new('ShaderNodeVectorMath')
    planar.operation = 'MULTIPLY'
    planar.inputs[1].default_value = (1, 0, 1)
    links.new(offset.outputs['Vector'], planar.inputs[0])
    length = nodes.new('ShaderNodeVectorMath')
    length.operation = 'LENGTH'
    links.new(planar.outputs['Vector'], length.inputs[0])
    scale = nodes.new('ShaderNodeMath')
    scale.operation = 'DIVIDE'
    scale.inputs[1].default_value = .015
    links.new(length.outputs['Value'], scale.inputs[0])
    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
    ramp.color_ramp.elements[0].color = (.002, .002, .002, 1)
    for radius, color in [(.0021, (.002, .002, .002, 1)), (.0023, (.055, .037, .020, 1)),
                          (.0055, (.055, .037, .020, 1)), (.0058, (.65, .64, .59, 1))]:
        ramp.color_ramp.elements.new(radius / .015).color = color
    links.new(scale.outputs[0], ramp.inputs[0])
    links.new(ramp.outputs['Color'], nodes.get('Principled BSDF').inputs['Base Color'])
    return mat


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--views', nargs='+', default=['front', 'profile', 'three_quarter', 'close_up'])
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
    args.out.mkdir(parents=True, exist_ok=True)
    face = bpy.data.objects['SKM_MH_C26_Player_001_FaceMesh.001']
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    sections = {}
    for i, mat in enumerate(face.data.materials):
        ids = {v for p in face.data.polygons if p.material_index == i for v in p.vertices}
        if ids:
            sections[i] = {'material': mat.name, 'vertices': len(ids),
                           'bounds_m': bounds([face.matrix_world @ face.data.vertices[v].co for v in ids])}
    report = {'source': args.source.name, 'vertices': len(face.data.vertices),
              'polygons': len(face.data.polygons), 'sections': sections,
              'shape_keys': list(face.data.shape_keys.key_blocks.keys()) if face.data.shape_keys else [],
              'armature': rig.name, 'bones': len(rig.data.bones),
              'weighted_groups': sorted({face.vertex_groups[g.group].name for v in face.data.vertices
                                         for g in v.groups if g.weight > .00001})}
    (args.out / 'geometry.json').write_text(json.dumps(report, indent=2))
    # Neutral geometry display only: imported Unreal shaders are not Blender shaders.
    clay = display_material('Inspection_Clay', (.34, .30, .26))
    sclera = display_material('Inspection_Sclera', (.65, .64, .59), .3)
    iris = display_material('Inspection_Iris', (.055, .037, .020), .4)
    pupil = display_material('Inspection_Pupil', (.002, .002, .002), .25)
    dark = display_material('Inspection_MouthInterior', (.035, .027, .023))
    teeth = display_material('Inspection_Teeth', (.55, .52, .44))
    old_sections = [p.material_index for p in face.data.polygons]
    face.data.materials.clear()
    for mat in [clay, sclera, iris, pupil, dark, teeth]:
        face.data.materials.append(mat)
    for section in (3, 4):
        bb = sections[section]['bounds_m']
        face.data.materials.append(eye_display('Inspection_Eye_' + str(section),
                                               (Vector(bb[0]) + Vector(bb[1])) * .5))
    hidden = []
    for poly, section in zip(face.data.polygons, old_sections):
        poly.material_index = 0
        if section in (3, 4):
            poly.material_index = 6 + section - 3
        elif section == 1:
            poly.material_index = 5
        elif section == 2:
            poly.material_index = 4
        elif section in (5, 6, 7, 8, 9):
            hidden.append(poly.index)
    # Remove translucent wetness/occlusion and lash cards from this throwaway display:
    # the actual anatomical lid must fit without an opaque eye-shell hiding it.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(face.data)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in hidden], context='FACES')
    bm.to_mesh(face.data)
    bm.free()
    for o in bpy.data.objects:
        if o.type == 'MESH' and o != face:
            o.hide_render = True
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.world = bpy.data.worlds.new('Inspection_World')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.12, .14, .17, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .35
    target = Vector((0, -.028, 1.705))
    for loc, power, size in [((-.55, -.8, 2.2), 38, .55), ((.7, -.4, 1.85), 16, .6), ((.3, .5, 2.05), 42, .5)]:
        bpy.ops.object.light_add(type='AREA', location=loc)
        lamp = bpy.context.object
        lamp.data.energy, lamp.data.size = power, size
        lamp.rotation_euler = (target - lamp.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    camera.data.type = 'ORTHO'
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = 1000, 1100
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    views = {'front': ((0, -1, 0), target, .285),
             'profile': ((1, 0, 0), target, .285),
             'three_quarter': ((.65, -.76, .02), target, .285),
             'close_up': ((.15, -1, .02), Vector((0, -.095, 1.696)), .155)}
    for name in args.views:
        offset, focus, scale = views[name]
        camera.location = focus + Vector(offset)
        camera.rotation_euler = (focus - camera.location).to_track_quat('-Z', 'Y').to_euler()
        camera.data.ortho_scale = scale
        scene.render.filepath = str(args.out.resolve() / (name + '.png'))
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    main()
