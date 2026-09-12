# CRICKET 26: Pro Front/Back Batting & Bowling Cameras & Replay Overhaul

## 1. Overview
This update completely overhauls the **Batting**, **Bowling**, and **Replay** camera and presentation systems in **CRICKET 26**, inspired by professional broadcast angles (e.g. Cricket 24 / Ashes Cricket gameplay).

Previously, gameplay cameras sat high up in a distant crane position (over 12 meters behind the pitch and 5 meters in the air), detaching the player from the action. Replays were short, skipped time mid-stride, and terminated abruptly when the last frame was hit.

With this overhaul:
1. **Batting Camera**: Placed directly behind the batsman over the off shoulder, looking straight down the 22 yards of the wicket at the bowler. As the bowler approaches, a dynamic tension push tightens the framing onto the ball release.
2. **Bowling Camera**: The player **IS** the bowler. Camera starts behind the bowler at the top of the mark, actively dollys with the bowler's strides down the pitch (complete with subtle athletic vertical stride bobbing), and smoothly glides right over the bowling shoulder into delivery release.
3. **Extended Replay & Smooth Outro**:
   - Captures earlier in the delivery stride (extended lead-in) so the gather, plant, release, bounce, and stroke are all recorded.
   - Eliminates artificial intermediate time skipping (`ReplayClock` jump cut).
   - Adds a dedicated 0.85-second broadcast outro deceleration and smooth fade, ensuring the replay only completes after the full play has naturally ended.
   - Sleek slow-motion HUD badge (`● REPLAY | 0.3x SLOW MOTION`) with broadcast viewfinder brackets.

---

## 2. Technical Implementation

### A. Dynamic Front/Back Geometry & Tracking
- **Batting Rig**:
  - `Eye = FVector(StrikerPos.X + 46.f, StrikerPos.Y + 320.f - PushDistance, 212.f)`
  - `Aim = FVector(-18.f, -850.f, 120.f)` (straight down the 22 yards of the pitch at bowler)
  - `FOV`: 47.0° easing to 44.5° dynamically as the bowler approaches the crease.
  - Follow-through: During `InPlay` (first 0.38s), the camera holds tightly behind the batsman's contact point before panning into outfield ball tracking.

- **Bowling Rig & Trailing Dolly**:
  - `Ready`: Behind the bowler at the mark (`BowlerPos.X, BowlerPos.Y - 260.f, 222.f`), aiming at the striker's stumps (`-38, 900, 95`).
  - `RunUp`: Trailing dolly running with the bowler down the pitch:
    ```cpp
    const float StrideBob = FMath::Sin(Time * 14.f) * 3.2f;
    const FVector Eye = FVector(BowlerPos.X + 8.f, BowlerPos.Y - 240.f, 214.f + StrideBob);
    Look(EC26CameraMode::BowlerRunup, Eye, StrikerTarget, 46.f, ...);
    ```
  - `Delivery`: Tight over the bowling shoulder (`BowlerPos.X + 18.f, BowlerPos.Y - 180.f, 208.f`), watching the delivery arm whip through and fire down the wicket.

### B. Extended Replay Architecture & Outro Transition
- **Lead-In Expansion**:
  - `ReplayClock = FMath::Max(Frames[0].Time, ReleaseStamp >= 0 ? ReleaseStamp - 0.45f : (ContactStamp >= 0 ? ContactStamp - 0.95f : ReplayEnd - 2.8f));`
- **Broadcast Multi-Angle Cuts without Time Skips**:
  - Preserved multi-camera cuts while eliminating the skip jump (`ReplayClock = FMath::Max(ReplayClock, ReplayEnd - 1.5f)`), allowing continuous slow-motion playback.
- **Dedicated Broadcast Outro**:
  - `IsReplayOutro` holds final actor poses and decelerates camera tracking for 0.85 seconds.
  - Smooth dissolve veil (`Fade * 0.40f`) fades out the slow-motion badge and transitions back to the next match phase cleanly.

---

## 3. Verification & Metrics
- **Compilation**: UnrealBuildTool Development Mac Build completed in **5.35 seconds** (`libUnrealEditor-CRICKETGAME.dylib`).
- **Unit & Automation Tests**: All **6 / 6** suites passed:
  - `Cricket26.Commentary.Director`: Passed (Success)
  - `Cricket26.Production.DeliveryPlan`: Passed (Success)
  - `Cricket26.Production.FrameRateAndShots`: Passed (Success)
  - `Cricket26.Rules.SuperOver`: Passed (Success)
  - `Cricket26.Simulation.GoldenDelivery`: Passed (Success)
  - `Cricket26.Simulation.Trajectories`: Passed (Success)
- **Visual Acceptance Suite**: All **29 / 29** visual beats captured with 0 skipped beats (`Tools/Capture.sh Camera_Overhaul`).
