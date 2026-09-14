"""Full-editor prep for hero material A/B captures. TRANSIENT inspection only.

Reads C26_MATERIAL_VIEW (daylight | side | closeup). Loads the existing
character review level, frames its first camera on the hero head, and sets a
fixed key + fill + sky rig per view, then saves. The review level is restored
with `git checkout` after all captures, so no camera/light/map change ships.
Game capture uses the existing C26ReviewCapture path: no game cameras,
materials or animation systems are touched.
"""
import os
import unreal as u

VIEW = os.environ.get('C26_MATERIAL_VIEW', 'daylight')
SRC = '/Game/Cricket26/Characters/Debug/L_C26_CharacterReview'

levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level(SRC), 'load review level failed'
actors = u.get_editor_subsystem(u.EditorActorSubsystem)

hero = None
cameras = []
key = None
fills = []
for actor in actors.get_all_level_actors():
    if isinstance(actor, u.SkeletalMeshActor) and hero is None:
        hero = actor
    elif isinstance(actor, u.CameraActor):
        cameras.append(actor)
    elif isinstance(actor, u.DirectionalLight):
        if key is None:
            key = actor
        else:
            fills.append(actor)
assert hero and cameras and key, 'review level missing hero/camera/key'
camera = cameras[0]

body = hero.get_component_by_class(u.SkeletalMeshComponent)
mesh = u.load_asset('/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate')
body.set_skeletal_mesh(mesh)
pose = u.C26CharacterProfile.bind_pose(mesh)
head = pose['head'].translation + hero.get_actor_location()
foot = pose['foot_l'].translation
toe = pose['ball_l'].translation
forward = u.Vector(toe.x - foot.x, toe.y - foot.y, 0)
forward.normalize()
u.log('C26_MATERIAL_HEAD ' + str(head) + ' FORWARD ' + str(forward))

# Fixed rig per view. Intensities mirror the match Day profile (key 6.4) so the
# hero is judged under the same daylight it plays in, not a studio softbox.
RIGS = {
    'daylight': {'key_rot': u.Rotator(pitch=-52, yaw=-35, roll=0), 'key_lux': 6.4,
                 'fill_lux': 2.2, 'sky_lux': 1.3, 'dist': 260.0, 'height': 12.0, 'fov': 35.0},
    'side': {'key_rot': u.Rotator(pitch=-25, yaw=-125, roll=0), 'key_lux': 5.0,
             'fill_lux': 1.5, 'sky_lux': 1.0, 'dist': 260.0, 'height': 12.0, 'fov': 35.0},
    'closeup': {'key_rot': u.Rotator(pitch=-45, yaw=-30, roll=0), 'key_lux': 6.4,
                'fill_lux': 2.2, 'sky_lux': 1.3, 'dist': 70.0, 'height': 4.0, 'fov': 30.0},
}
rig = RIGS[VIEW]
key.set_actor_rotation(rig['key_rot'], False)
keycomp = key.get_component_by_class(u.DirectionalLightComponent)
keycomp.set_intensity(rig['key_lux'])
keycomp.set_editor_property('light_source_angle', 1.0)
for fill in fills:
    fill.get_component_by_class(u.DirectionalLightComponent).set_intensity(rig['fill_lux'])

sky = None
for actor in actors.get_all_level_actors():
    if isinstance(actor, u.SkyLight):
        sky = actor
        break
if sky is None:
    sky = actors.spawn_actor_from_class(u.SkyLight, head + u.Vector(0, 0, 200))
sky.get_component_by_class(u.SkyLightComponent).set_intensity(rig['sky_lux'])

eye = head + forward * rig['dist'] + u.Vector(0, 0, rig['height'])
camera.set_actor_location(eye, False, False)
camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye, head), False)
u.log('C26_MATERIAL_RIG key=' + str(rig['key_rot']) + ' lux=' + str(rig['key_lux']))
cam = camera.get_component_by_class(u.CameraComponent)
cam.set_field_of_view(rig['fov'])
cam.set_editor_property('constrain_aspect_ratio', False)

assert levels.save_current_level(), 'save review level failed'
u.log('C26_MATERIAL_PREP_DONE view=' + VIEW + ' eye=' + str(eye))
