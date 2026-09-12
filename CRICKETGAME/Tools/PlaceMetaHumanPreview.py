"""Place one AC26MetaHumanPlayer instance in the real gameplay level at the striker's crease, for
visual validation (Phase H/I/J of the MetaHuman task). This is a plain level actor, not wired into
AC26MatchGameMode::BuildMatchActors() or the Athletes[] array the live match simulation drives --
it cannot affect scoring, replays, or the rules. Position/yaw match the striker spawn used at
C26MatchGameMode.cpp:153 (Athletes[11], FVector(-38,900,5), yaw -90) so it stands exactly where the
real batter does.

Safe to re-run: finds and reuses its own previous instance by label instead of duplicating.

Run:  UnrealEditor-Cmd CRICKETGAME.uproject -run=pythonscript -script=Tools/PlaceMetaHumanPreview.py
"""
import unreal as u

MAP_PATH = '/Game/Cricket26/Maps/L_SuperOver'
LABEL = 'MHPreview_C26_Player_001'
STRIKER_LOCATION = u.Vector(-38.0, 900.0, 5.0)
STRIKER_YAW = -90.0

def log(msg):
    u.log('C26_MH_PLACE %s' % msg)

editor_subsys = u.get_editor_subsystem(u.EditorActorSubsystem)
level_subsys = u.get_editor_subsystem(u.LevelEditorSubsystem)

if not level_subsys.load_level(MAP_PATH):
    log('FATAL could not load level %s' % MAP_PATH)
    quit()

existing = [a for a in editor_subsys.get_all_level_actors() if a.get_actor_label() == LABEL]
for a in existing:
    log('removing stale previous instance')
    editor_subsys.destroy_actor(a)

actor_class = u.load_class(None, '/Script/CRICKETGAME.C26MetaHumanPlayer')
if actor_class is None:
    log('FATAL C26MetaHumanPlayer class not found -- build the editor target first')
    quit()

rotation = u.Rotator(0.0, STRIKER_YAW, 0.0)
actor = editor_subsys.spawn_actor_from_class(actor_class, STRIKER_LOCATION, rotation)
if actor is None:
    log('FATAL spawn failed')
    quit()

actor.set_actor_label(LABEL)
log('placed %s at %s yaw %s' % (LABEL, STRIKER_LOCATION, STRIKER_YAW))

u.EditorLevelLibrary.save_current_level() if hasattr(u, 'EditorLevelLibrary') else level_subsys.save_current_level()
log('DONE')
