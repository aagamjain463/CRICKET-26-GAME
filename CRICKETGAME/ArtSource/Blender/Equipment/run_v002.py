"""Round-2 equipment build: improved gloves + shoes ONLY.

Saves C26_Equipment_v002.blend (v001 stays untouched) and overwrites ONLY the
four glove/shoe FBX exports. Pads, helmet, bat, ball, wickets are not rebuilt.
Renders isolated turntables to Artifacts/CharacterAudit/equip_after/ for review.
"""
import importlib
import sys
from pathlib import Path

import bpy

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))
import c26_build
import build_glove_v002
import build_shoe_v002
for _m in (c26_build, build_glove_v002, build_shoe_v002):
    importlib.reload(_m)

ROOT = HERE.parents[2]
EXPORTS = ROOT / 'ArtSource/Exports/Equipment'
OUT = ROOT / 'Artifacts/CharacterAudit/equip_after'
OUT.mkdir(parents=True, exist_ok=True)


def run(export=True, save=True, preview=True):
    c26_build.clear_scene()
    made = []
    made.append(build_glove_v002.build_glove('SM_C26_Glove_L', 'l', +1))
    made.append(build_glove_v002.build_glove('SM_C26_Glove_R', 'r', -1))
    made.append(build_shoe_v002.build('SM_C26_Shoe_L', side=-1))
    made.append(build_shoe_v002.build('SM_C26_Shoe_R', side=1))
    c26_build.report(made)
    total = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in made)
    print('TOTAL TRIS %d across %d objects' % (total, len(made)))
    if preview:
        for o in made:
            for i, az in enumerate((38, 158, 278)):
                c26_build.studio([o], str(OUT / ('%s_iso_%d.png' % (o.name, i))),
                                 azimuth=az, elevation=16, res=(700, 700))
    if export:
        for o in made:
            c26_build.export_fbx([o], str(EXPORTS / (o.name + '.fbx')))
    if save:
        bpy.ops.wm.save_as_mainfile(
            filepath=str(HERE / 'C26_Equipment_v002.blend'))
    return made


run()
print('C26_EQUIP_V002_DONE')
