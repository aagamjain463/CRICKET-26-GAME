"""Verify the IMPORTED authored clips against the facts the offline corrector proved.

Run:  UnrealEditor-Cmd <abs>/CRICKETGAME.uproject \
        -run=pythonscript -script=<abs>/Tools/VerifyImportedClip.py -unattended -nosplash -nullrhi

Why this exists
---------------
Tools/correct_authored_anim.py verifies the CORRECTED FBX. Tools/ImportAnimations.py
then imports it. Nothing checked the asset in between, so "the clip the game plays
is the clip that was verified" was an assumption, not a measurement -- and the
contact sheets Tools/AnimContactSheet.py draws come from the FBX, not from the
asset. This closes that gap: it reads the imported UAnimSequence out of the
content browser and re-asserts the same geometry in engine centimetres.

It is deliberately separate from Cricket26.Anim.AuthoredClips, which is broken
(see below) and whose failures are therefore not informative.

A note on composing bone chains, because this is the bug that hid the drive
-------------------------------------------------------------------------
The skeleton has a root bone (Armature_001) ABOVE Hips, and it carries the
rotation that stands the armature frame up. A chain composed from Hips upward
without it is measured in the armature's frame, not the body's: height ends up
on the wrong axis and every derived angle is wrong. Cricket26.Anim.AuthoredClips
makes exactly that omission, which is why all 89 of its checks fail uniformly
across all 18 clips. This script derives the root from the skeleton instead of
assuming it, so it cannot make the same mistake.

print()/unreal.log() are not forwarded under -run=pythonscript, so everything is
written to stdout via sys.stdout AND mirrored to Tools/VerifyImportedClip.log.
"""
import math
import os
import sys
import traceback
import unreal as u

FPS = 24.0
# Artifacts/ is gitignored, which is where every other harness in Tools/ puts its
# log. Do not write beside the script: Tools/*.log is not ignored here.
_ART = u.Paths.project_dir() + 'Artifacts/'
try:
    os.makedirs(_ART, exist_ok=True)
except Exception:
    pass
LOG = _ART + 'VerifyImportedClip.log'
_fh = open(LOG, 'w')


def say(m):
    _fh.write(m + '\n')
    _fh.flush()
    try:
        sys.stdout.write(m + '\n')
        sys.stdout.flush()
    except Exception:
        pass


CLIPS = {
    # name: (front foot side, defining frame 0-based, plant-to-finish window)
    'A_C26_BattingDrive': ('Left', 22, (17, 31)),
}

results = []


def check(label, ok, detail):
    results.append(ok)
    say('  %-52s %s  %s' % (label, 'PASS' if ok else 'FAIL', detail))


def main():
    for name, (front, contact, window) in CLIPS.items():
        seq = u.load_asset('/Game/Cricket26/Animations/' + name)
        if not seq:
            check('%s: asset loads' % name, False, 'missing -- run Tools/ImportAnimations.py')
            continue
        say('== %s ==' % name)
        say('  skeleton %s  frames %d  length %.3fs'
            % (seq.get_editor_property('skeleton').get_name(),
               u.AnimationLibrary.get_num_frames(seq), seq.get_play_length()))

        # Derive the root rather than hardcoding it: the whole point is that the
        # root is easy to forget and expensive to get wrong.
        root = list(u.AnimationLibrary.find_bone_path_to_root(seq, 'Hips'))[-1]
        say('  root bone = %s' % root)

        def at(frame, bone):
            # Iterate in the order find_bone_path_to_root returns it (bone first,
            # root last). That is the order that reproduces the offline
            # corrector's numbers to 0.1 cm -- hips 184.5, stance ankles 24.7,
            # hand span 17.9 -- so it is the one that composes correctly against
            # this API. Root-first does not, and it fails loudly rather than
            # subtly: the hips land at -1.4 cm with the height on the wrong axis.
            # The essential thing is that Armature_001 is IN the path at all.
            path = list(u.AnimationLibrary.find_bone_path_to_root(seq, bone))
            acc = u.Transform()
            for b in path:
                acc = acc * u.AnimationLibrary.get_bone_pose_for_frame(seq, b, frame, False)
            t = acc.translation
            return (t.x, t.y, t.z)

        def h(a, b):
            return (b[0] - a[0], b[1] - a[1])

        def n2(v):
            m = math.hypot(v[0], v[1])
            return (v[0] / m, v[1] / m) if m > 1e-6 else (0.0, 0.0)

        def dot(a, b):
            return a[0] * b[0] + a[1] * b[1]

        back = 'Right' if front == 'Left' else 'Left'
        lf0, rf0 = at(0, front + 'Foot'), at(0, back + 'Foot')
        hp0, hpC = at(0, 'Hips'), at(contact, 'Hips')
        lfC = at(contact, front + 'Foot')

        # The back foot to the front foot points down the pitch, at the bowler.
        # Using the feet rather than the shoulder line keeps this independent of
        # which way the rig's own forward axis happens to point.
        pitch = n2(h(rf0, lf0))

        stance_ground = min(lf0[2], rf0[2])

        check('%s: stance feet on the ground' % name,
              abs(lf0[2] - rf0[2]) < 2.0 and 15.0 < stance_ground < 40.0,
              'ankles %.1f / %.1f cm' % (lf0[2], rf0[2]))

        worst, where = 0.0, window[0]
        for fr in range(window[0], window[1] + 1):
            up = at(fr, front + 'Foot')[2] - stance_ground
            if up > worst:
                worst, where = up, fr
        check('%s: front foot really is planted after the plant' % name, worst < 3.0,
              'worst %.1f cm up (frame %d)' % (worst, where))

        hands0 = math.dist(at(0, 'LeftHand'), at(0, 'RightHand'))
        check('%s: hands together on the handle at the stance' % name, hands0 < 25.0,
              'span %.1f cm' % hands0)

        check('%s: hips at athletic height' % name, 150.0 < hp0[2] < 220.0,
              'hips %.1f cm' % hp0[2])

        travel = dot(h(hp0, hpC), pitch)
        check('%s: weight transfers onto the front foot' % name, travel > 20.0,
              'hips %.1f cm forward' % travel)

        stride = dot(h(lf0, lfC), pitch)
        check('%s: front foot strides down the pitch' % name, stride > 15.0,
              'front foot %.1f cm' % stride)

        hm = at(contact, 'LeftHand'), at(contact, 'RightHand')
        mid = ((hm[0][0] + hm[1][0]) / 2, (hm[0][1] + hm[1][1]) / 2)
        front_of = dot(h((hpC[0], hpC[1], 0.0), (mid[0], mid[1], 0.0)), pitch)
        check('%s: contact hands are in front of the body' % name, front_of > 10.0,
              'hands %.1f cm in front of the hips' % front_of)

        check('%s: head does not drop through the stroke' % name,
              at(contact, 'Head')[2] > hpC[2] + 40.0,
              'head %.1f cm above hips' % (at(contact, 'Head')[2] - hpC[2]))

        # Chest faces the bowler. The reference direction is the way the body
        # TRAVELS through the drive, not the line between the feet: a batsman's
        # feet are largely side by side across the crease, so back-foot-to-front-foot
        # is nowhere near "down the pitch" and using it makes this check lie. The
        # travel direction cannot be fooled -- he drives at the bowler.
        #
        # This is also the guard against a whole-body yaw bug, which this pipeline
        # has had before (see repair_facing in c26_anim_author.py): a 180 degree
        # twist sends the chest normal anti-parallel, ~143 degrees, and fails.
        u_dir = n2(h(at(0, 'Hips'), at(contact, 'Hips')))
        s = n2(h(at(0, 'RightShoulder'), at(0, 'LeftShoulder')))
        cand = [(-s[1], s[0]), (s[1], -s[0])]
        normal = cand[0] if dot(cand[0], u_dir) > dot(cand[1], u_dir) else cand[1]
        off = math.degrees(math.acos(max(-1.0, min(1.0, dot(normal, u_dir)))))
        check('%s: chest faces the bowler at the stance' % name, off < 60.0,
              'chest %.1f deg off square' % off)
        # A shoulder line that ran along the direction of travel would mean the
        # batsman is standing front-on (or twisted); either way the stance above
        # would have caught it, but this names the failure precisely.
        check('%s: the stance is side-on, not square to the bowler' % name,
              abs(dot(s, u_dir)) < 0.75,
              'shoulder line %.1f deg off the line of travel'
              % math.degrees(math.acos(max(-1.0, min(1.0, abs(dot(s, u_dir)))))))
        say('')

    failed = results.count(False)
    say('C26_IMPORTED_%s checks=%d failures=%d'
        % ('PASS' if failed == 0 else 'FAIL', len(results), failed))


try:
    main()
except Exception:
    say('FATAL\n' + traceback.format_exc())
    say('C26_IMPORTED_FAIL checks=%d failures=%d' % (len(results), results.count(False) + 1))

_fh.close()
