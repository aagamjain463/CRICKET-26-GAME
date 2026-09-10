
import unreal as u

# Load map
u.EditorLevelLibrary.load_level('/Game/Cricket26/Maps/L_SuperOver')
world = u.EditorLevelLibrary.get_editor_world()

# We can spawn AC26MatchGameMode or inspect what happens
# Or we can inspect the blueprint/classes directly
cls_athlete = u.EditorAssetLibrary.load_blueprint_class('/Script/CRICKETGAME.C26Athlete')
u.log(f"Athlete class: {cls_athlete}")
