# Round 7 — player reactions, celebrations and personality

A presentation layer on top of Rounds 1–6. It never changes gameplay: cues are staged **after** the
outcome is committed to the rules ledger, and every gameplay action (stroke, delivery, gather, catch,
throw, dive) is rendered exactly as before.

## Flow

```
Resolve() commits the outcome
  -> StageReactions(Official, Focus)        C26MatchGameMode.cpp
       AC26Athlete::React(BaseAction, Cue, Delay[, PresentationOnly])
  -> UC26CharacterPresentationComponent::SelectState
       ReactionState: C26Character::ReactionKey(Cue, Role, Temperament, Handedness) -> clip
       waits the athlete's beat, plays at temperament rate
  -> clip ends on the pose it idles in -> bRecovered -> ready loop / walk (0.32 s blend)
```

`BaseAction` is what the legacy bodies always showed (Celebrate / Disappointed), so the old visual path
is unchanged. `PresentationOnly` cues (near miss, appeal, support, dropped, good stop, dot confidence)
only reach active premium bodies. `SetAction` clears any cue, so a new gameplay action always wins.

## Events -> cues -> clips

| Event (from the committed outcome) | Striker | Bowler | Keeper / fielders |
| --- | --- | --- | --- |
| Four | `Boundary` → BatterAcknowledge | `BoundaryConceded` → Frustrated (Energetic: HandsOnHead) | non-striker `Support` → BatterAcknowledge |
| Six | `Six` → BatterCelebrate (Calm: Acknowledge) | `BoundaryConceded` | non-striker `Support` |
| Fifty / hundred crossed | `Milestone` → BatterCelebrate | | |
| Wicket | `Dismissed` → BatterDismissed (holds final pose) | `Wicket` → CelebrateRestrained (Calm) / CelebrateEnergetic | `Wicket` → Restrained / team Celebrate (Energetic) / Energetic; catcher `Catch` |
| Play-and-miss (shot, `Timing::Miss`) | `PlayAndMiss` → BatterBeaten | `NearMiss` → HandsOnHead (Aggressive: Appeal) | keeper `Appeal` if the ball passed within 24 cm of the stumps, else `Support` → Clap |
| Beaten, no shot, close to stumps | `Beaten` → BatterBeaten | `NearMiss` | keeper `Appeal` |
| Edge, no runs | `Edge` → BatterEdge | `NearMiss` | keeper `Support` |
| Dot ball | `Dot` → BatterReset | `DotConfidence` → Clap (Calm: none, walks back) | |
| Dropped chance (`DROPPED CATCH!` / `PUT DOWN!`) | | `Dropped` → HandsOnHead | the fielder: `Dropped` → HandsOnHead |
| Diving stop, ≤1 run | | `GoodStop` → Clap | |

Beats: near players react first — `0.08 s + distance/7000` (capped 0.40 s); bowler 0.05–0.2 s, batter 0.12–0.3 s.

Reaction phase length is now 2.4 s for important balls (was 1.5 s) and 1.6 s otherwise (was 1.1 s) so a
reaction completes before the replay / next ball.

## Clips (authored in `c26_actions.py`, gated by `action_lab.py`)

CelebrateRestrained, CelebrateEnergetic, Appeal, HandsOnHead, Frustrated, Clap, and right-handed
BatterAcknowledge / BatterBeaten / BatterEdge / BatterReset / BatterDismissed mirrored to `_L`.
Existing Celebrate, BatterCelebrate_R/L and Disappointed are reused. Feet stay planted in every
reaction; every clip except BatterDismissed ends on the pose it started from (lab: ≤ 0.7 cm), and
hands move less than 26 cm per 30 fps frame.

## Personality

`FC26PlayerAppearance::AnimationStyle` (existing field). The default `Professional` resolves per squad slot
through `C26Character::Temperament` to Calm / Aggressive / Energetic (every XI has all three). It affects
only presentation: clip choice, reaction beat (+0.10 / +0.03 / 0 s), playback rate (0.94 / 1.04 / 1.08 ±2%),
idle amplitude, breathing rate and head-turn speed.

## Idle body language and secondary motion

`FC26LifeNode` in `C26CricketerAnimInstance.cpp` runs after the clip blend and rotates only
spine_01–05, neck_01 and head about their own pivots:

- breathing (chest opens, neck counter-rotates to keep the helmet level), faster after running;
- slow weight sway (S-curve through the spine, head counter-sway);
- head/eye aim at the match's `LookAt` (±50° yaw, −14…12° pitch; striker in stance excluded);
- follow-through inertia: an under-damped spring on measured acceleration and turn rate leans the trunk.

Weights are exactly **zero** for Batting, Bowling, Pickup, Throw, Catch, Dive and Slide, and hard-zero on
the Dt==0 event frames, so contact, release, gathers, bat/hand and foot locking are untouched. Legs and
pelvis are never modified. Replays evaluate with the layer off. Cost: one pass over the compact pose up to the head bone, skipped entirely when all weights are zero.

## Verification

```bash
B=/Applications/Blender.app/Contents/MacOS/Blender
$B --background --python ArtSource/Blender/Premium/action_lab.py [-- --only Clap,... --render]
$B --background --python ArtSource/Blender/Premium/author_cricket_actions.py -- --only <clips>
UnrealEditor CRICKETGAME.uproject -ExecutePythonScript="$PWD/Tools/ImportRound7Reactions.py"
bash Tools/ReactLab.sh react_lab        # in-match reaction lab
```

`Tools/ReactLab.sh` (`-C26ReactLab`) plays real AI deliveries (checks the release and contact frames carry no
procedural layer and that the bowler settles out of the delivery clip), then commits four, six, bowled,
caught, play-and-miss, edge, dropped and dot through the real `Resolve()` and checks the right athlete
shows the right clip, no bone jumps > 30 cm per frame (clip switches included), and each reaction has
recovered or is in its authored recovery when the phase ends. Captures: `Artifacts/Captures/<label>/`.

## Limitations

- Motion is kinematically authored, not captured; reactions are in place (no running celebrations,
  no players converging for a team huddle, no high-fives between specific players).
- No cloth, jersey or equipment physics: garments are skinned and equipment is rigidly socketed, so
  "secondary motion" is the trunk layer (breathing deforms the jersey; head stabilisation steadies the
  helmet). No eye bones or facial animation on the current body.
- Appeals only follow near misses; the rules have no LBW, so there is no appeal → decision sequence.
- Fielder caps sit rotated on the head: `BuildDefaultPlayerProfile.py` reuses the batting helmet offset for
  the cap (existing before Round 7).
- A reaction cut short by Skip or the next delivery blends out through the normal teleport reset.
