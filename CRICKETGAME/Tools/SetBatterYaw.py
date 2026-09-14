"""Set the batter review profile's mesh-to-gameplay yaw (no rebuild needed).

The candidate mesh's authored facing differs from the legacy skeleton the -90
default was chosen for. Evidence: warp log shows the authored stride mapping to
+Y world (toward the keeper) while bowler and ball sit at -Y. Trying +90 for the
batter profile only; the outfield profile keeps its verified -90.
"""
import sys
import unreal as u

import re
want = None
for arg in sys.argv[1:]:
    m = re.match(r'^--yaw=(-?\d+(?:\.\d+)?)$', arg)
    if m:
        want = float(m.group(1))
if want is None:
    want = -90.0
path = '/Game/Cricket26/Characters/Data/DA_C26_BatterReview'
profile = u.load_asset(path)
assert profile
# Named-field edit: avoids all Rotator constructor order ambiguity (the last
# run pitched the batter prone instead of yawing him).
rot = profile.get_editor_property('mesh_to_gameplay_rotation')
u.log('C26_BATTER_YAW was pitch=%s yaw=%s roll=%s' % (rot.pitch, rot.yaw, rot.roll))
rot.pitch = 0.0
rot.yaw = want
rot.roll = 0.0
profile.set_editor_property('mesh_to_gameplay_rotation', rot)
check = profile.get_editor_property('mesh_to_gameplay_rotation')
u.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
u.log('C26_BATTER_YAW now pitch=%s yaw=%s roll=%s' % (check.pitch, check.yaw, check.roll))
