
import unreal as u
p = '/Game/Cricket26/Characters/Players/SM_C26_Player_Batter.SM_C26_Player_Batter'
asset = u.load_object(None, p)
u.log(f"LOAD_OBJECT TEST: {p} -> {asset}")
