"""Reimport refined SK_C26_Athlete_Review and set up canonical sockets."""
import hashlib
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
LIB = u.EditorAssetLibrary
FOLDER = '/Game/Cricket26/Characters/Bodies'
NAME = 'SK_C26_Athlete_Review'

canonical = u.load_asset(FOLDER + '/SK_C26_FullBody_Candidate')
assert canonical, 'Canonical SK_C26_FullBody_Candidate not found'
skeleton = canonical.get_editor_property('skeleton')
assert skeleton, 'Skeleton not found'

options = u.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal = True
options.import_animations = options.import_materials = options.import_textures = False
options.skeleton = skeleton
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
options.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose', False)
options.skeletal_mesh_import_data.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)

fbx_path = ROOT / 'ArtSource/Premium/FullBody' / (NAME + '.fbx')
assert fbx_path.exists(), f'FBX not found: {fbx_path}'
digest = hashlib.sha256(fbx_path.read_bytes()).hexdigest()

task = u.AssetImportTask()
task.filename = str(fbx_path)
task.destination_path, task.destination_name = FOLDER, NAME
task.automated = task.replace_existing = True
task.options = options

u.log("Importing SK_C26_Athlete_Review.fbx...")
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

mesh = u.load_asset(FOLDER + '/' + NAME)
assert mesh and mesh.get_editor_property('skeleton') == skeleton, 'Mesh import failed or wrong skeleton'

# Map materials from canonical candidate
materials = {str(s.material_slot_name): s.material_interface for s in canonical.get_editor_property('materials')}
slots = list(mesh.get_editor_property('materials'))
for slot in slots:
    label = str(slot.material_slot_name)
    if label in materials:
        slot.material_interface = materials[label]
    else:
        u.log_warning(f"Unmapped material slot: {label}")
mesh.set_editor_property('materials', slots)

# Regenerate LODs
editor = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
if editor:
    editor.regenerate_lod(mesh, 4, False, False)

# Set up all required sockets
sockets = [
    ('Helmet', 'head', u.Transform(location=u.Vector(10.5, 0.8, 0.0),
                                  rotation=u.Rotator(pitch=0.0, yaw=90.0, roll=-90.0),
                                  scale=u.Vector(1, 1, 1))),
    ('BatGrip_L', 'hand_l', u.Transform()),
    ('BatGrip_R', 'hand_r', u.Transform()),
    ('Glove_L', 'hand_l', u.Transform()),
    ('Glove_R', 'hand_r', u.Transform()),
    ('PadMount_L', 'calf_l', u.Transform()),
    ('PadMount_R', 'calf_r', u.Transform()),
    ('BallHand_L', 'hand_l', u.Transform(location=u.Vector(0.0, 7.0, 0.0))),
    ('BallHand_R', 'hand_r', u.Transform(location=u.Vector(0.0, 7.0, 0.0))),
]

for sock_name, bone_name, transform in sockets:
    ok = u.C26CharacterProfile.set_equipment_socket(mesh, sock_name, bone_name, transform)
    assert ok, f"Failed to set socket {sock_name} on {bone_name}"
    u.log(f"Set socket {sock_name} on {bone_name}")

LIB.set_metadata_tag(mesh, 'C26.SourceSHA256', digest)
LIB.save_loaded_asset(skeleton, only_if_is_dirty=False)
LIB.save_loaded_asset(mesh, only_if_is_dirty=False)

errors = u.C26CharacterProfile.inspect_body(mesh)
u.log(f"InspectBody errors: {errors if errors else 'PASS (0 errors)'}")

u.log("C26_REIMPORT_ATHLETE_REVIEW_SUCCESS")
