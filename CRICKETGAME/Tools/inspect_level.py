
import unreal as u

world = u.EditorLevelLibrary.get_editor_world()
u.EditorLevelLibrary.load_level('/Game/Cricket26/Maps/L_SuperOver')
actors = u.EditorLevelLibrary.get_all_level_actors()

u.log(f"LEVEL ACTORS COUNT: {len(actors)}")
for a in actors:
    name = a.get_name()
    cls = a.get_class().get_name()
    loc = a.get_actor_location()
    scale = a.get_actor_scale3d()
    if any(k in name.lower() or k in cls.lower() for k in ['athlete', 'player', 'cricketer', 'mesh', 'bat', 'camera']):
        u.log(f"ACTOR: {name} ({cls}) at ({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f}), scale=({scale.x:.2f}, {scale.y:.2f}, {scale.z:.2f})")
