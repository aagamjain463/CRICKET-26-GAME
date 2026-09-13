# CRICKET 26: Contextual Match Presentation & Cinematic Architecture

## 1. Executive Summary

CRICKET 26's **Match Presentation System** bridges the gap between raw gameplay simulation and televised broadcast quality. Rather than allowing deliveries to transition abruptly or relying on static, disconnected cutscenes, the system introduces an authoritative, contextual presentation pipeline:

$$\text{MATCH EVENT} \longrightarrow \text{CONTEXT ANALYSIS} \longrightarrow \text{SCENE SELECTION} \longrightarrow \text{PARTICIPANT BINDING} \longrightarrow \text{CAMERA RIGGING} \longrightarrow \text{PROCEDURAL ACTING} \longrightarrow \text{CLEAN RETURN}$$

Gameplay remains the hero at all times. Presentation exists to heighten emotion, celebrate heroic milestones, and capture high-tension death overs without causing soft-locks or disrupting the core flow.

---

## 2. Core Architecture & Modules

### 2.1 Central Director (`UC26PresentationDirector`)
Attached directly to `AC26MatchGameMode`, `UC26PresentationDirector` acts as the central orchestrator for all cinematic sequences:
- **Scene Library**: Houses dynamic definitions and multi-shot variants for all 38 contextual cricket events.
- **Dynamic Prioritization Queue**: Sorts pending presentation requests by urgency (`Low`, `Medium`, `High`, `Critical`). High-priority scenes (e.g. Century or Wicket) preempt low-priority scenes cleanly.
- **Recency Ring Buffer & Anti-Repetition**: Tracks recent events and variant timestamps, applying heavy cooldown penalties to prevent repetitive broadcast scenes.
- **Match Pressure Calculator**: Real-time evaluation of tension based on required run rate (RRR), death deliveries remaining ($\le 12$ or $\le 6$ balls), and wickets in hand.
- **Instant Skip Safety**: Player tap, mouse click, or spacebar immediately interrupts any playing cinematic, cleans the queue, restores athlete transforms and poses, and smoothly returns to authoritative gameplay within a single frame.

### 2.2 Cinematic Camera Director (`AC26CameraDirector`)
The presentation camera pipeline features 11 dedicated broadcast cinematic lenses:
1. **`TwoShot`**: Mid-distance framing of bowler/captain, batsman/partner, or fielder conferences.
2. **`CloseUpFace`**: Hero portrait framing with dramatic shallow depth of field.
3. **`LowAngleDramatic`**: Low-turf upward angle capturing heroic bat salutes and roaring bowler reactions.
4. **`OverShoulderBatter`**: Behind batter's shoulder viewing bowler setup and field shifts.
5. **`OverShoulderBowler`**: Behind bowler's shoulder looking down the pitch towards the batter.
6. **`PitchTrackWalking`**: Dolly track running parallel to the pitch strip following batsmen walking between overs.
7. **`StadiumWide`**: Grand gantry establishing shot for team walkouts and toss ceremonies.
8. **`OrbitCelebration`**: Dynamic rotating arc around celebrating athletes.
9. **`HighAngleToss`**: Overhead view tracking the parabolic toss coin flip.
10. **`UmpirePOV`**: Behind bowler's end stumps framing LBW appeals and boundary calls.
11. **`DugoutReaction`**: Long-telephoto framing of team benches and coaching staff.

### 2.3 Procedural Athlete Animation Engine (`AC26Athlete`)
Athletes do not rely on fixed level sequences. All scenes leverage procedural spine kinematics, arm inverse kinematics, and expressive pose blending:
- `EC26Action::BatRaise`: Bat held aloft to the crowd/dressing room with head tilt.
- `EC26Action::GloveTap`: Batsmen meet at mid-pitch and clatter gloves with affirmative nod.
- `EC26Action::Handshake`: Formal post-match and toss congratulatory handshakes.
- `EC26Action::FistPump`: Sprinting bowler clenched fist celebration.
- `EC26Action::Discuss`: Gesturing arms and interactive head movement during tactical conferences.
- `EC26Action::TossFlip`: Parabolic arm flick with rotating physics coin.

---

## 3. Library of Contextual Presentation Events (38 Events)

| Category | Event Enum | Default Lens | Priority | Overlay Card |
| :--- | :--- | :--- | :--- | :--- |
| **Toss Ceremony** | `TossCeremonyWalkout` | `StadiumWide` | High | `TossCard` |
| | `TossCoinFlip` | `HighAngleToss` | High | `TossCard` |
| | `TossDecisionBat` | `CloseUpFace` | High | `TossCard` |
| | `TossDecisionBowl` | `CloseUpFace` | High | `TossCard` |
| **Walkout & Openings** | `TeamWalkoutAnthem` | `StadiumWide` | High | `TeamCard` |
| | `OpeningBattersWalkIn` | `PitchTrackWalking` | High | `BatterCard` |
| | `OpeningBowlerRunupPrep`| `OverShoulderBowler` | Medium | `BowlerCard` |
| **Batter Discussions** | `BatterMidPitchDiscussion` | `TwoShot` | Low | None |
| | `BatterBoundaryMeeting` | `TwoShot` | Medium | None |
| | `BatterCloseCallReview` | `TwoShot` | Medium | None |
| | `BatterEndOverDiscussion` | `PitchTrackWalking` | Low | `OverSummary` |
| **Tactical Conferences** | `BowlerCaptainDiscussion`| `TwoShot` | Medium | None |
| | `BowlerKeeperFieldAdjust`| `OverShoulderBowler`| Low | None |
| | `BowlerFielderCongratulate`| `TwoShot` | Low | None |
| **Bowler Reactions** | `BowlerFrustrationDot` | `CloseUpFace` | Low | None |
| | `BowlerFrustrationBoundary`| `LowAngleDramatic` | Medium | None |
| | `BowlerAppealingDesperate`| `OverShoulderBowler`| High | None |
| **Batter Reactions** | `BatterDotDisappointment` | `CloseUpFace` | Low | None |
| | `BatterPlayAndMissReaction`| `CloseUpFace` | Medium | None |
| | `BatterCloseRunoutSurvival`| `LowAngleDramatic` | High | None |
| **Wicket Celebrations** | `WicketCelebrationBowled`| `OrbitCelebration` | High | `WicketCard` |
| | `WicketCelebrationCaught`| `TwoShot` | High | `WicketCard` |
| | `WicketCelebrationLBW` | `UmpirePOV` | High | `WicketCard` |
| | `WicketCelebrationRunOut`| `TwoShot` | High | `WicketCard` |
| **Dismissal Follow-up** | `WicketDisappointmentBatterWalk`| `PitchTrackWalking` | High | `WicketCard` |
| | `NewBatterEntry` | `PitchTrackWalking` | High | `NewBatterCard` |
| **Milestones** | `FiftyCelebration` | `OrbitCelebration` | Critical | `MilestoneFifty` |
| | `CenturyCelebration` | `OrbitCelebration` | Critical | `MilestoneCentury` |
| **Over & Innings Breaks**| `EndOfOverSummaryCard` | `TwoShot` | Medium | `OverSummary` |
| | `InningsBreakTransition`| `StadiumWide` | High | `InningsBreak` |
| **Tension & Death Overs** | `FinalOverTensionSetup` | `OverShoulderBowler`| High | `FinalOverCard` |
| | `FinalBallTensionSetup` | `CloseUpFace` | Critical | `FinalBallCard` |
| **Match Conclusion** | `MatchWinningCelebration` | `OrbitCelebration` | Critical | `MatchResult` |
| | `LosingTeamReaction` | `CloseUpFace` | High | `MatchResult` |
| | `PostMatchHandshakes` | `PitchTrackWalking` | High | `MatchResult` |
| | `PlayerOfTheMatchPresentation` | `TwoShot` | Critical | `PlayerOfMatch` |
| **Weather & DRS** | `RainDelayWalkoff` | `StadiumWide` | High | `RainDelay` |
| | `DRSUmpireSignal` | `UmpirePOV` | High | `DRSReview` |

---

## 4. Broadcast Pacing Modes

The system supports three user-selectable pacing tiers:

1. **`Balanced` (Default)**:
   - Full coverage of High and Critical moments (wickets, milestones, final overs, match conclusion).
   - Dynamically thins out 50% of routine Low-priority discussions to keep game tempo crisp.
   - Scene durations streamlined by ~12%.
2. **`Full` (Televised Experience)**:
   - Plays all contextual interactions including mid-pitch banter, bowler-captain strategy adjustments, and play-and-miss reactions.
   - Ideal for career mode and championship finals.
3. **`Quick` (Action-Focused)**:
   - Only High and Critical priority scenes are permitted.
   - Scene durations capped strictly at 2.5 seconds.
   - Perfect for mobile sessions and speedrun play.

---

## 5. Exact Milestone Fire-Once Guarantee

To prevent immersion-breaking bugs where a batter crossing 50 or 100 triggers duplicate celebrations on subsequent runs (e.g. reaching 51, 52, or jumping from 46 to 52 with a six):
- **Authoritative Arrays**: `bFiftyCelebrated[3]` and `bCenturyCelebrated[3]` track celebration state per batter index.
- **Single Trigger Execution**: A milestone event is dispatched **once and only once** when a batter's score first reaches or exceeds the threshold.
- **Multi-Run Jumps Handled**: Jumps like $48 \to 54$ or $98 \to 104$ trigger their respective milestone cleanly.
- **Match Resets**: All flags reset cleanly on match restart or innings rollover.

---

## 6. Developer & Debug Controls

### 6.1 Keyboard Hotkeys
| Key | Action / Presentation Scene |
| :--- | :--- |
| **`F8`** | Toggle Presentation Director Debug Overlay |
| **`T`** | Trigger Official Toss Ceremony & Coin Flip |
| **`W`** | Trigger Wicket Bowled Celebration |
| **`K`** | Trigger Wicket Caught Celebration |
| **`5`** | Trigger 50-Run Milestone Celebration |
| **`0`** | Trigger 100-Run Century Milestone Celebration |
| **`B`** | Trigger Bowler & Captain Tactical Discussion |
| **`N`** | Trigger Incoming Batter Entry |
| **`O`** | Trigger End of Over Summary Card |
| **`M`** | Trigger Match Winning Celebration |
| **`H`** | Trigger Post-Match Handshake Line |
| **`Space / Tap`** | Skip active presentation cleanly |

### 6.2 Console Exec Commands
- `C26PresentationDebug`: Toggle HUD debug panel.
- `C26TriggerScene <SceneName>`: Trigger `Toss`, `Wicket`, `Caught`, `Fifty`, `Century`, `BowlerCaptain`, `NewBatter`, `EndOfOver`, `Win`, `Handshakes`.
- `C26Pacing <Mode>`: Set pacing tier to `Balanced`, `Full`, or `Quick`.
