"""Read-only UE asset/map audit. Run using UnrealEditor-Cmd -run=pythonscript."""
import unreal as u

lib = u.EditorAssetLibrary
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
level.load_level('/Game/Cricket26/Maps/L_SuperOver')
actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    u.log('C26_AUDIT actor %s class=%s transform=%s' % (
        actor.get_name(), actor.get_class().get_name(), actor.get_actor_transform()))
    for component in actor.get_components_by_class(u.MeshComponent):
        u.log('C26_AUDIT component %s transform=%s materials=%s' % (
            component.get_name(), component.get_world_transform(),
            [str(m) for m in component.get_materials()]))
for name in ['M_Surface', 'M_Grass', 'M_Pitch', 'M_Willow']:
    mat = lib.load_asset('/Game/Cricket26/Materials/' + name)
    u.log('C26_AUDIT material %s expressions=%s two_sided=%s' % (
        name, u.MaterialEditingLibrary.get_num_material_expressions(mat),
        mat.get_editor_property('two_sided')))
mesh = lib.load_asset('/Game/Cricket26/Characters/SK_Cricketer')
u.log('C26_AUDIT skeletal bounds=%s' % mesh.get_bounds())
for slot in mesh.get_editor_property('materials'):
    u.log('C26_AUDIT slot %s material=%s' % (slot.material_slot_name, slot.material_interface))
registry = u.AssetRegistryHelpers.get_asset_registry()
for asset in registry.get_assets_by_path('/Game/Cricket26', recursive=True):
    if 'Blueprint' in str(asset.asset_class_path):
        u.log('C26_AUDIT cricket blueprint %s' % asset.package_name)
u.log('C26_AUDIT_COMPLETE')
