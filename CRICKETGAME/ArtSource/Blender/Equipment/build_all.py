"""Author the whole CRICKET 26 hero equipment set and export it for Unreal.

One FBX per piece into ArtSource/Exports/Equipment, imported by Tools/ImportEquipment.py into
/Game/Cricket26/Equipment. Left and right variants are authored separately rather than mirrored
with a negative component scale, so nothing in the game depends on negative-determinant
transforms and the material slot order is identical on both sides.
"""
import importlib
import c26_build, build_bat, build_headwear, build_guards, build_shoe, build_premium_pads
for _m in (c26_build, build_bat, build_headwear, build_guards, build_shoe, build_premium_pads):
    importlib.reload(_m)

PROJ = "/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/"
EXPORTS = PROJ + "ArtSource/Exports/Equipment/"


def run(export=True, save=True):
    c26_build.clear_scene()
    made = []
    made.append(build_bat.build())
    made.append(build_headwear.build_helmet())
    made.append(build_headwear.build_grille())
    made.append(build_headwear.build_cap())
    # Premium pads: same frame/slots/size contract as build_guards.build_pad,
    # fitted to the measured hero leg (open-back shell, knee roll on the knee).
    made.append(build_premium_pads.build('SM_C26_Pad_L', buckle_side=1))
    made.append(build_premium_pads.build('SM_C26_Pad_R', buckle_side=-1))
    made.append(build_guards.build_glove('SM_C26_Glove_L', hand=-1))
    made.append(build_guards.build_glove('SM_C26_Glove_R', hand=1))
    made.append(build_shoe.build('SM_C26_Shoe_L', side=-1))
    made.append(build_shoe.build('SM_C26_Shoe_R', side=1))
    c26_build.report(made)
    total = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in made)
    print('TOTAL TRIS %d across %d objects' % (total, len(made)))
    if export:
        for o in made:
            c26_build.export_fbx([o], EXPORTS + o.name + '.fbx')
    if save:
        import bpy
        bpy.ops.wm.save_as_mainfile(
            filepath=PROJ + 'ArtSource/Blender/Equipment/C26_Equipment_v001.blend')
    return made
