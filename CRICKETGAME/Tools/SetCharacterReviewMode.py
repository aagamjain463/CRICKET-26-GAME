import unreal as u
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level('/Game/Cricket26/Characters/Debug/L_C26_CharacterReview')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',u.C26CharacterReviewMode)
levels.save_current_level()
