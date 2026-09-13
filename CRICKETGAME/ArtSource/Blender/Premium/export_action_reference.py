"""Recover each authored FBX's own bind rig. Tiny reference geometry is editor-only,
never a character body. Export mesh and motion together so import cannot reuse a wrong bind pose."""
import bpy
from pathlib import Path
root=Path(__file__).resolve().parents[3];out=root/'ArtSource/Premium/AnimationSources';out.mkdir(parents=True,exist_ok=True)
for name in ['A_C26_BattingDrive','A_C26_BowlingPace']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(root/'ArtSource/Exports/Animations'/f'{name}.fbx'))
 arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
 mesh=bpy.data.meshes.new('EditorReferenceOnly');mesh.from_pydata([(-.05,0,0),(.05,0,0),(0,0,.1)],[],[(0,1,2)]);mesh.update()
 obj=bpy.data.objects.new('SOURCE_REFERENCE_NOT_PLAYER',mesh);bpy.context.collection.objects.link(obj)
 obj.parent=arm;obj.matrix_parent_inverse=arm.matrix_world.inverted()
 bone=next(b for b in arm.data.bones if b.parent is None);g=obj.vertex_groups.new(name=bone.name);g.add([0,1,2],1,'REPLACE');obj.modifiers.new('SourceSkin','ARMATURE').object=arm
 action=arm.animation_data.action;assert action
 start,end=action.frame_range;bpy.context.scene.frame_start=int(start);bpy.context.scene.frame_end=int(end)
 bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);obj.select_set(True);bpy.context.view_layer.objects.active=arm
 bpy.ops.export_scene.fbx(filepath=str(out/f'{name}_Reference.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,apply_unit_scale=True)
 print('C26_ACTION_REFERENCE',name,start,end)
