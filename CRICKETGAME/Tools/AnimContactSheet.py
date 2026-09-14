#!/usr/bin/env python3
"""CRICKET 26 -- look at an authored clip without opening Blender or the editor.

Why this exists
---------------
Every gate in Tools/correct_authored_anim.py is a number, and numbers cannot
answer "does this read as a cricketer". Round 3's batting work needed a way to
watch Idle -> Trigger -> Straight Drive -> Recovery -> Idle at a glance, from
more than one angle, and at more than one speed. This draws the skeleton itself
straight out of the corrected FBX -- the exact asset the game imports -- so what
is on the sheet is what the engine plays, not a re-simulation of it.

Usage
-----
  python3 Tools/AnimContactSheet.py                      # the drive, all views
  python3 Tools/AnimContactSheet.py A_C26_BattingDrive --view side --step 1
  python3 Tools/AnimContactSheet.py --out Artifacts/AnimSheets

Views are named for where the CAMERA is, relative to a right-handed batter:
  front   from the bowler's end, looking back down the pitch
  side    from square of the wicket (the classic technique camera)
  three   a 3/4 view between the two
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw

from correct_authored_anim import RigData

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORRECTED = os.path.join(ROOT, 'ArtSource', 'Exports', 'Animations', 'Corrected')

# Bone chains to stroke, drawn back-to-front so the near arm reads on top.
CHAINS = [
    ('spine', ['Hips', 'Spine', 'Spine1', 'Spine2', 'Neck', 'Head']),
    ('armL', ['LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand']),
    ('armR', ['RightShoulder', 'RightArm', 'RightForeArm', 'RightHand']),
    ('legL', ['Hips', 'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'LeftToeBase']),
    ('legR', ['Hips', 'RightUpLeg', 'RightLeg', 'RightFoot', 'RightToeBase']),
]
COLOUR = {'spine': (232, 232, 236), 'armL': (255, 176, 70), 'armR': (255, 120, 60),
          'legL': (120, 190, 255), 'legR': (80, 140, 230)}
BG = (16, 17, 20)
GRID = (40, 42, 48)

# The corrected clips are Y-up. These pick the two horizontal axes a camera sees.
VIEWS = {'front': 0.0, 'three': 45.0, 'side': 90.0}

# The drive's authored key frames, named, so the sheet reads as the SEQUENCE the brief asks to be
# able to watch rather than as a row of anonymous poses:
#   Idle stance -> Trigger -> Stride -> Contact -> Follow-through -> Recovery -> Idle stance.
# Frame numbers are 1-based and match c26_anim_author.py::batting_keys() exactly.
PHASES = {
    1:  'STANCE (anticipation)',
    5:  'TRIGGER (back+across, foot up)',
    11: 'TOP OF BACKLIFT',
    13: 'TOE-OFF',
    15: 'STRIDE (foot airborne)',
    17: 'REACHING',
    18: 'FRONT-FOOT PLANT',
    21: 'DOWNSWING (hips lead)',
    23: 'CONTACT',
    26: 'EXTENSION',
    29: 'FOLLOW-THROUGH',
    32: 'POISED FINISH',
    33: 'UNWEIGHT (foot lifts)',
    35: 'STEP BACK',
    36: 'RECOVERED (== f1)',
}
# The two frames that define the shot and the seam; drawn with a highlighted cell border.
HIGHLIGHT = {1: (120, 190, 255), 23: (255, 90, 70), 36: (120, 190, 255)}


def pose_at(rig, frame):
    W = rig.compose(rig.local_at(max(0, min(len(rig.times) - 1, frame - 1))))
    return {rig.name[b].replace('mixamorig:', ''): w.t for b, w in W.items()}


def wrap(text, width):
    words, lines, line = text.split(), [], ''
    for w in words:
        if len(line) + len(w) + (1 if line else 0) > width:
            lines.append(line)
            line = w
        else:
            line = (line + ' ' + w).strip()
    if line:
        lines.append(line)
    return lines


def draw_frame(draw, pose, box, yaw_deg, scale, ground_y, label, phase=None):
    x0, y0, w, h = box
    a = math.radians(yaw_deg)
    ca, sa = math.cos(a), math.sin(a)
    cx = x0 + w * 0.5

    def project(p):
        # Horizontal plane is (x, z); height is y.
        u = p[0] * ca + p[2] * sa
        return (cx + u * scale, y0 + h - (p[1] - ground_y) * scale - h * 0.06)

    # Ground line: a foot that leaves this line is off the turf, which is the
    # whole point of the stride and recovery frames.
    gy = y0 + h - h * 0.06
    draw.line([(x0 + 4, gy), (x0 + w - 4, gy)], fill=GRID, width=1)
    for name, chain in CHAINS:
        pts = [project(pose[b]) for b in chain if b in pose]
        if len(pts) > 1:
            draw.line(pts, fill=COLOUR[name], width=3, joint='curve')
        for p in pts:
            draw.ellipse([p[0] - 2.5, p[1] - 2.5, p[0] + 2.5, p[1] + 2.5], fill=COLOUR[name])
    if 'Head' in pose:
        hp = project(pose['Head'])
        draw.ellipse([hp[0] - 7, hp[1] - 12, hp[0] + 7, hp[1] + 2], outline=(232, 232, 236), width=2)
    draw.text((x0 + 6, y0 + 6), label, fill=(150, 152, 160))
    if phase:
        # The phase name goes under the figure, wrapped, so a long name cannot
        # run over the neighbouring cell.
        for i, line in enumerate(wrap(phase, 26)[:2]):
            draw.text((x0 + 6, y0 + h - 26 + i * 12), line, fill=(196, 200, 210))


def sheet(clip, view, step, out_dir, columns=9, cell=190):
    rig = RigData(os.path.join(CORRECTED, clip + '.fbx'))
    frames = list(range(1, len(rig.times) + 1, step))
    poses = [pose_at(rig, f) for f in frames]
    ground = min(min(p['LeftToeBase'][1], p['RightToeBase'][1]) for p in poses)
    span = max(max(p['Head'][1] for p in poses) - ground, 1.0)
    scale = (cell * 0.80) / span
    rows = (len(frames) + columns - 1) // columns
    img = Image.new('RGB', (columns * cell, rows * cell + 54), BG)
    d = ImageDraw.Draw(img)
    d.text((8, 8), '%s  --  %s view  --  every %d frame(s) at 24fps'
           % (clip, view, step), fill=(200, 202, 210))
    # The sequence, in order, so the sheet can be read without the source.
    d.text((8, 24), '  ->  '.join(PHASES[f] for f in sorted(PHASES) if f in frames),
           fill=(140, 150, 170))
    for i, (f, p) in enumerate(zip(frames, poses)):
        box = ((i % columns) * cell, 54 + (i // columns) * cell, cell, cell)
        if f in HIGHLIGHT:
            d.rectangle([box[0] + 1, box[1] + 1, box[0] + box[2] - 1, box[1] + box[3] - 1],
                        outline=HIGHLIGHT[f], width=2)
        draw_frame(d, p, box, VIEWS[view], scale, ground, 'f%d' % f, PHASES.get(f))
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, '%s_%s_step%d.png' % (clip, view, step))
    img.save(path)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('clip', nargs='?', default='A_C26_BattingDrive')
    ap.add_argument('--view', choices=list(VIEWS) + ['all'], default='all')
    ap.add_argument('--step', default='2',
                    help='2 = normal read, 1 = every frame (the slow-motion pass), all = both')
    ap.add_argument('--out', default=os.path.join(ROOT, 'Artifacts', 'AnimSheets'))
    args = ap.parse_args()
    views = list(VIEWS) if args.view == 'all' else [args.view]
    steps = [1, 2] if args.step == 'all' else [int(args.step)]
    # Slow motion first: the full sequence is what the brief asks to be able to watch, and the
    # normal-speed sheet is the read of it.
    for step in sorted(steps):
        for v in views:
            print('C26_SHEET %s' % sheet(args.clip, v, step, args.out))


if __name__ == '__main__':
    main()
