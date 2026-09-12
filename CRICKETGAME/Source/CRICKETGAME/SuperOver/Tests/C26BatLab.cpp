#include "../C26MatchGameMode.h"

#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26HUD.h"
#include "../C26PlayerController.h"
#include "../C26Settings.h"
#include "../C26Controls.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "UnrealClient.h"

// ---------------------------------------------------------------------------
// C26BatLab - scripted batting playtest.
//
// Runs a matrix of real deliveries and plays each one through the ACTUAL input
// path: synthetic pointer events go into AC26PlayerController::BeginGesture /
// MoveGesture / EndGesture in viewport pixels, so the HUD hit-testing, the
// design-space conversion, the zone test, the pointer ownership rules and the
// release commit are all exercised exactly as a finger or a mouse would.
//
// Nothing here reaches into the gesture state to "help" it: the lab presses,
// drags and lifts, then reads back what the batting loop decided.
//
//   Tools/BatLab.sh  ->  -C26BatLab [-C26BatLabDir=...] [-C26BatLabShots]
// ---------------------------------------------------------------------------
namespace C26BatLab
{
    struct FCase
    {
        const TCHAR* Name;
        EC26Delivery Type;
        float Line;             // cm, - = off side
        float Length;           // cm from the bowler's end
        FVector2D DragTo;       // design-space offset from the touch origin at release
        FVector2D MidDrag;      // waypoint held before the final vector (change of mind)
        float ReleaseOffsetMs;  // 0 = ideal; negative = early
        bool bLeftHanded;
        bool bPressDuringRunUp; // press while the bowler is running in, not after release
        const TCHAR* Expect;    // shot family expected from the gesture + delivery
    };

    // Origin of every synthetic press: inside the batting zone, clear of the HUD.
    static const FVector2D Origin(1180.f, 560.f);

    static const FCase Cases[] =
    {
        // --- TEST 1: cover drive, full outside off, medium pull, ideal release
        {TEXT("01_cover_drive"),        EC26Delivery::Pace,   42.f, 620.f, { 96.f, -96.f}, FVector2D::ZeroVector,    0.f, false, true,  TEXT("COVER DRIVE")},
        // --- TEST 2: leg-side attack, full leg stump, large pull
        {TEXT("02_legside_attack"),     EC26Delivery::Pace,    -34.f, 620.f, {-150.f, -95.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("FLICK")},
        // --- TEST 3: short ball, leg-side square drag
        {TEXT("03_short_pull"),         EC26Delivery::Bouncer,  -8.f, 120.f, {-165.f, -30.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("PULL")},
        // --- TEST 4: change of mind mid-hold (cover -> straight -> leg)
        {TEXT("04_change_of_mind"),     EC26Delivery::Pace,     -6.f, 600.f, {-120.f, -80.f}, {130.f, -80.f},          0.f, false, true,  TEXT("FLICK")},
        // --- TEST 5: magnitude ladder on identical deliveries
        {TEXT("05a_magnitude_20"),      EC26Delivery::Pace,   10.f, 600.f, {  0.f,  -56.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("STRAIGHT DRIVE")},
        {TEXT("05b_magnitude_50"),      EC26Delivery::Pace,   10.f, 600.f, {  0.f, -120.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("STRAIGHT DRIVE")},
        {TEXT("05c_magnitude_100"),     EC26Delivery::Pace,   10.f, 600.f, {  0.f, -260.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("STRAIGHT DRIVE")},
        // --- TEST 6: early release
        {TEXT("06a_early"),             EC26Delivery::Pace,   10.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector, -130.f, false, true, TEXT("COVER DRIVE")},
        {TEXT("06b_very_early"),        EC26Delivery::Pace,   10.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector, -165.f, false, true, TEXT("COVER DRIVE")},
        // --- TEST 7: late release
        {TEXT("07a_late"),              EC26Delivery::Pace,   10.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,  130.f, false, true, TEXT("COVER DRIVE")},
        {TEXT("07b_very_late"),         EC26Delivery::Pace,   10.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,  165.f, false, true, TEXT("COVER DRIVE")},
        // --- TEST 8: left-handed batter, same intents, mirrored screen drags
        {TEXT("08a_lefty_cover"),       EC26Delivery::Pace,    -42.f, 620.f, {-96.f,  -96.f}, FVector2D::ZeroVector,   0.f, true,  true,  TEXT("COVER DRIVE")},
        {TEXT("08b_lefty_pull"),        EC26Delivery::Bouncer, 8.f, 120.f, {165.f,  -30.f}, FVector2D::ZeroVector,   0.f, true,  true,  TEXT("PULL")},
        // --- straight drive, on drive, cut, upper cut, yorker, glance
        {TEXT("09_straight_drive"),     EC26Delivery::Pace,     -0.f, 640.f, {  0.f, -140.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("STRAIGHT DRIVE")},
        {TEXT("10_square_cut"),         EC26Delivery::Pace,   55.f, 140.f, {150.f,  -40.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("SQUARE CUT")},
        {TEXT("11_bouncer_cut"),        EC26Delivery::Bouncer,40.f, 40.f, {150.f,  -40.f}, FVector2D::ZeroVector,   0.f, false, true,  nullptr},
        {TEXT("12_yorker_dig"),         EC26Delivery::Yorker,   -0.f, 790.f, {  0.f, -110.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("DUG-OUT DRIVE")},
        {TEXT("13_leg_glance"),         EC26Delivery::Pace,    -30.f, 560.f, {-190.f,  20.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("LEG GLANCE")},
        // --- controlled defence from a pull inside the commit threshold
        {TEXT("14_defensive_push"),     EC26Delivery::Pace,   10.f, 480.f, {  6.f,  -16.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("DEFENSIVE PUSH")},
        // --- press only after release (late engagement) still works
        {TEXT("15_press_after_release"),EC26Delivery::Pace,   30.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,   0.f, false, false, TEXT("COVER DRIVE")},
        // --- multi-touch: a second finger during the drag must be ignored
        {TEXT("16_multitouch"),         EC26Delivery::Pace,   30.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("COVER DRIVE")},
        // --- cancelled touch: gesture abandoned, next ball must still work
        {TEXT("17_cancelled_touch"),    EC26Delivery::Pace,   20.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,   0.f, false, true,  nullptr},
        {TEXT("18_after_cancel"),       EC26Delivery::Pace,   30.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("COVER DRIVE")},
        // --- release during the run-up: very early, must not stick or double-fire
        {TEXT("19_runup_release"),      EC26Delivery::Pace,   10.f, 600.f, { 90.f,  -90.f}, FVector2D::ZeroVector,   0.f, false, true,  nullptr},
        // --- swing away, off-side drive off an outswinger
        {TEXT("20_outswing_drive"),     EC26Delivery::Outswing,35.f, 600.f, { 96.f,  -96.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("COVER DRIVE")},
        {TEXT("21_inswing_flick"),      EC26Delivery::Inswing,  -20.f, 600.f, {-130.f, -80.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("FLICK")},
        {TEXT("22_slower_drive"),       EC26Delivery::Slower,  20.f, 580.f, { 60.f, -120.f}, FVector2D::ZeroVector,   0.f, false, true,  TEXT("COVER DRIVE")},
    };
    static constexpr int NumCases = UE_ARRAY_COUNT(Cases);
}

void AC26MatchGameMode::UpdateBatLab(float Dt)
{
    (void)Dt;
    using namespace C26BatLab;
    auto* PC = Cast<AC26PlayerController>(GetWorld()->GetFirstPlayerController());
    auto* HUD = PC ? Cast<AC26HUD>(PC->GetHUD()) : nullptr;
    if (!PC || !HUD) return;

    auto Check = [&](bool Passed, const TCHAR* Message)
    {
        if (!Passed) ++LabFailures;
        UE_LOG(LogC26, Display, TEXT("C26_LAB_CHECK case=%s %s %s"),
            LabCase < NumCases ? Cases[LabCase].Name : TEXT("-"), Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
    };
    // Design -> viewport pixels, so the lab pushes real screen coordinates.
    auto Px = [&](FVector2D Design) { return HUD->FromDesign(Design); };
    // Visual acceptance frames: the arrow and the marker have to be *seen*, not
    // just measured, so a handful of cases write real screenshots.
    auto Shot = [&](const TCHAR* Beat)
    {
        if (!LabShots || LabCase >= NumCases) return;
        const FString Key = FString::Printf(TEXT("%s_%s"), Cases[LabCase].Name, Beat);
        if (LabShotKeys.Contains(Key)) return;
        LabShotKeys.Add(Key);
        IFileManager::Get().MakeDirectory(*LabDirectory, true);
        FScreenshotRequest::RequestScreenshot(LabDirectory / (Key + TEXT(".png")), true, false);
        UE_LOG(LogC26, Display, TEXT("C26_LAB_FRAME %s"), *Key);
    };
    const bool bShotCase = LabCase == 0 || LabCase == 2 || LabCase == 6;

    if (FPlatformTime::Seconds() - LabStarted > 420.0)
    {
        UE_LOG(LogC26, Error, TEXT("C26_LAB_TIMEOUT case=%d phase=%d"), LabCase, int(Phase));
        BatLab = false; FPlatformMisc::RequestExitWithStatus(false, 1); return;
    }
    if (LabCase >= NumCases)
    {
        UE_LOG(LogC26, Display, TEXT("C26_LAB_%s deliveries=%d failures=%d"),
            LabFailures ? TEXT("FAIL") : TEXT("PASS"), LabPlayed, LabFailures);
        BatLab = false; FPlatformMisc::RequestExitWithStatus(false, LabFailures ? 1 : 0); return;
    }

    const FCase& C = Cases[LabCase];

    // ---- keep a live, player-batting Ready state in front of the lab --------
    if (Phase == EC26Phase::Menu || Phase == EC26Phase::Result) { PlayerBatsFirst = true; StartMatch(); Skip(); return; }
    if (Phase == EC26Phase::Intro || Phase == EC26Phase::Interval || Phase == EC26Phase::Reaction || Phase == EC26Phase::Replay) { Skip(); return; }
    if (!PlayerBatting()) { PlayerBatsFirst = true; StartMatch(); Skip(); return; }

    if (Phase == EC26Phase::Ready)
    {
        if (LabPhase != 0)
        {
            // Previous case finished: report it and step on.
            LabPhase = 0; LabPressed = false; LabReleased = false; LabSecondFinger = false;
            LabQuietFail = false; LabSawBounce = false; LabMidAim = 0.f; LabResultLogged = false;
            LabCandidate.Reset(); GestureCommitCount = 0;
            ++LabPlayed; ++LabCase;
            return;
        }
        if (PhaseTime < 0.35f) return;
        // Force the delivery this case needs, then start the run-up.
        Bowling = FC26DeliveryPlan();
        Bowling.Type = C.Type; Bowling.Line = C.Line; Bowling.Length = C.Length;
        Bowling.Speed = C.Type == EC26Delivery::Slower ? 2450.f : 3350.f;
        Bowling.Swing = C.Type == EC26Delivery::Outswing ? 180.f : C.Type == EC26Delivery::Inswing ? -180.f : 0.f;
        Bowling.Bounce = C.Type == EC26Delivery::Bouncer ? .72f : .55f;
        if (Preferences) { Preferences->LeftHandedBatter = C.bLeftHanded; }
        bLeftHandedBatter = C.bLeftHanded;
        Intent = FC26ShotIntent();
        Footwork = 0.f; bFootworkManual = false;
        LabGestureLog.Reset();
        StartDelivery();
        bLeftHandedBatter = C.bLeftHanded; // PrepareDelivery is not re-run here, but keep it explicit
        LabPhase = 1;
        UE_LOG(LogC26, Display, TEXT("C26_LAB_BEGIN case=%s type=%d line=%.0f length=%.0f hand=%s"),
            C.Name, int(C.Type), C.Line, C.Length, C.bLeftHanded ? TEXT("LEFT") : TEXT("RIGHT"));
        // BUG 1 regression: the marker must already exist on the first run-up frame.
        Check(IsBounceIndicatorVisible(), TEXT("marker visible on the first run-up frame"));
        Check(BouncePrediction.bFromIntent, TEXT("run-up marker is the bowler's intended pitch"));
        if (bShotCase) Shot(TEXT("1_runup_marker"));
        return;
    }

    if (Phase != EC26Phase::RunUp && Phase != EC26Phase::Delivery)
    {
        if (LabPhase == 2 && !LabResultLogged && Phase == EC26Phase::InPlay)
        {
            LabResultLogged = true;
            UE_LOG(LogC26, Display, TEXT("C26_LAB_RESULT case=%s played=%s timing=%d quality=%.2f simDelta=%+.0fms gestureDelta=%+.0fms"),
                Cases[LabCase].Name, *LastContact.Shot, int(LastContact.Timing), LastContact.Quality,
                LastContact.TimingDeltaMs, LabDelta);
            if (GestureCommitCount > 0 && LastContact.Timing != EC26Timing::Miss)
            {
                Check(FMath::IsNearlyEqual(LastContact.TimingDeltaMs, LabDelta, 0.5f),
                    TEXT("the bat used the exact release delta the meter showed"));
                // An edge is a mishit, not a different stroke: only compare the
                // family when the bat actually middled its intended shot.
                if (LastContact.Timing != EC26Timing::Edge)
                    Check(LastContact.Shot == LabCandidate,
                        TEXT("the stroke played is the stroke the gesture previewed"));
            }
        }
        // Ball is done. Wait for Ready; the report happens on the next Ready frame.
        if (LabPhase == 1 && !LabReleased) { LabPhase = 2; }
        return;
    }

    // ---- press ------------------------------------------------------------
    const bool bPressWindow = C.bPressDuringRunUp
        ? (Phase == EC26Phase::RunUp && PhaseTime > 0.45f)
        : (Phase == EC26Phase::Delivery && Simulation.Ball.Age > 0.04f);
    if (!LabPressed && !LabReleased && bPressWindow)
    {
        LabPressed = true;
        PC->DebugPointer(0, Px(Origin), 0);
        Check(bBattingGestureActive, TEXT("press opens the gesture through the real input path"));
        Check(!ShotQueued, TEXT("press alone does not commit a shot"));
        Check(BattingState == EC26BattingState::Pulling, TEXT("press enters Pulling, inside the dead zone"));
        Check(BattingPullFrac <= 0.001f, TEXT("no magnitude before any drag"));
        LabPressAim = BattingGestureAngle;
    }

    // ---- drag -------------------------------------------------------------
    if (LabPressed && !LabReleased && bBattingGestureActive)
    {
        const bool bUseMid = !C.MidDrag.IsZero() && SecondsToIdealRelease() > 0.35f;
        const FVector2D Target = Origin + (bUseMid ? C.MidDrag : C.DragTo);
        // Step the finger toward the target so the arrow is genuinely animated.
        const FVector2D Now = FMath::Vector2DInterpTo(BattingGestureCurrent, Target, Dt, 26.f);
        PC->DebugPointer(0, Px(Now), 1);
        Check(!ShotQueued, TEXT("dragging does not commit a shot"));
        if (bShotCase)
        {
            if (BattingPullFrac > 0.02f && BattingPullFrac < 0.20f) Shot(TEXT("2_pull_small"));
            else if (BattingPullFrac > 0.40f && BattingPullFrac < 0.60f) Shot(TEXT("3_pull_mid"));
            else if (BattingPullFrac > 0.90f) Shot(TEXT("4_pull_full"));
        }
        if (bUseMid) LabMidAim = BattingGestureAngle;

        // Second finger during the drag must not disturb the owner (TEST: multi-touch).
        if (LabCase == 20 && !LabSecondFinger && BattingPullFrac > 0.4f)
        {
            LabSecondFinger = true;
            const float AimBefore = BattingGestureAngle, MagBefore = BattingPullFrac;
            PC->DebugPointer(1, Px(FVector2D(620.f, 300.f)), 0);
            PC->DebugPointer(1, Px(FVector2D(500.f, 700.f)), 1);
            Check(FMath::IsNearlyEqual(AimBefore, BattingGestureAngle, 0.01f)
                && FMath::IsNearlyEqual(MagBefore, BattingPullFrac, 0.001f), TEXT("second finger cannot steer the gesture"));
            PC->DebugPointer(1, Px(FVector2D(500.f, 700.f)), 2);
            Check(bBattingGestureActive && !ShotQueued, TEXT("second finger lifting cannot commit the shot"));
        }

        // Cancellation case: abandon the gesture and prove nothing is left stuck.
        if (LabCase == 21 && BattingPullFrac > 0.5f && !LabReleased)
        {
            LabReleased = true;
            CancelBattingGesture(TEXT("lab: simulated lost touch"));
            PC->DebugPointer(0, Px(Origin + C.DragTo), 2);
            Check(!bBattingGestureActive, TEXT("cancelled touch clears the gesture"));
            Check(!ShotQueued, TEXT("cancelled touch commits nothing"));
            Check(BattingState != EC26BattingState::Pulling && BattingState != EC26BattingState::Armed,
                TEXT("cancelled touch does not leave the machine in Pulling/Armed"));
            LabPhase = 2;
            return;
        }

        // Run-up release case: lift while the bowler is still running in.
        if (LabCase == 23 && Phase == EC26Phase::RunUp && PhaseTime > 1.2f)
        {
            LabReleased = true;
            PC->DebugPointer(0, Px(Origin + C.DragTo), 2);
            Check(ShotQueued, TEXT("run-up release still commits one attempt"));
            Check(BattingReleaseTiming == EC26ReleaseTiming::VeryEarly || BattingReleaseTiming == EC26ReleaseTiming::NoShot,
                TEXT("run-up release reads very early"));
            Check(!bBattingGestureActive, TEXT("run-up release clears the gesture"));
            UE_LOG(LogC26, Display, TEXT("C26_LAB_RUNUP_RELEASE delta=%+.0fms timing=%s"),
                BattingReleaseDeltaMs, *GetReleaseTimingName());
            LabPhase = 2;
            return;
        }
    }

    // ---- release ----------------------------------------------------------
    if (LabPressed && !LabReleased && bBattingGestureActive && Phase == EC26Phase::Delivery)
    {
        const float ToIdealMs = SecondsToIdealRelease() * 1000.f;
        if (ToIdealMs <= -C.ReleaseOffsetMs + FApp::GetDeltaTime() * 500.f)
        {
            LabReleased = true;
            const FVector2D Final = Origin + C.DragTo;
            const float AimAtRelease = BattingGestureAngle;
            const float MagAtRelease = BattingPullFrac;
            const float AggAtRelease = BattingAggression;
            const FString Candidate = BattingShotCandidate;
            const int32 Before = GestureCommitCount;

            PC->DebugPointer(0, Px(Final), 1);   // last drag frame
            PC->DebugPointer(0, Px(Final), 2);   // lift = commit

            Check(ShotQueued, TEXT("release commits the shot"));
            Check(GestureCommitCount == Before + 1, TEXT("release commits exactly one shot"));
            Check(!bBattingGestureActive, TEXT("release closes the gesture"));
            // A second lift from the same pointer must change nothing.
            PC->DebugPointer(0, Px(Final), 2);
            Check(GestureCommitCount == Before + 1, TEXT("a duplicate lift is ignored"));

            if (!C.MidDrag.IsZero())
                Check(FMath::Abs(LabMidAim - BattingGestureAngle) > 25.f,
                    TEXT("direction at release differs from the mid-hold direction"));
            Check(FMath::Abs(MagAtRelease - BattingPullFrac) < 0.35f, TEXT("magnitude carried into the release"));

            LabAim = BattingGestureAngle;
            LabMag = BattingPullFrac;
            LabAgg = BattingAggression;
            LabDelta = BattingReleaseDeltaMs;
            LabPower = BattingGesturePower;
            LabCandidate = BattingShotCandidate;
            LabTiming = GetReleaseTimingName();
            UE_LOG(LogC26, Display,
                TEXT("C26_LAB_RELEASE case=%s aim=%+.1f zone=%s mag=%.2f aggr=%.2f power=%.2f delta=%+.0fms timing=%s candidate=%s contactZ=%.0f (pre-release aim=%+.1f mag=%.2f aggr=%.2f cand=%s)"),
                C.Name, LabAim, C26Controls::DirectionZoneName(LabAim), LabMag, LabAgg, LabPower, LabDelta,
                *LabTiming, *LabCandidate, Simulation.ContactPosition.Z, AimAtRelease, MagAtRelease, AggAtRelease, *Candidate);

            // Expected release-timing band from the case's requested offset.
            if (FMath::Abs(C.ReleaseOffsetMs) < 40.f)
                Check(BattingReleaseTiming == EC26ReleaseTiming::Perfect || BattingReleaseTiming == EC26ReleaseTiming::Good,
                    TEXT("an on-time release reads PERFECT or GOOD"));
            else if (C.ReleaseOffsetMs <= -150.f)
                Check(BattingReleaseTiming == EC26ReleaseTiming::VeryEarly || BattingReleaseTiming == EC26ReleaseTiming::NoShot,
                    TEXT("a very early release reads VERY EARLY"));
            else if (C.ReleaseOffsetMs < 0.f)
                Check(BattingReleaseTiming == EC26ReleaseTiming::Early || BattingReleaseTiming == EC26ReleaseTiming::VeryEarly,
                    TEXT("an early release reads EARLY"));
            else if (C.ReleaseOffsetMs >= 150.f)
                Check(BattingReleaseTiming == EC26ReleaseTiming::VeryLate || BattingReleaseTiming == EC26ReleaseTiming::NoShot,
                    TEXT("a very late release reads VERY LATE"));
            else
                Check(BattingReleaseTiming == EC26ReleaseTiming::Late || BattingReleaseTiming == EC26ReleaseTiming::VeryLate,
                    TEXT("a late release reads LATE"));
            if (C.Expect)
                Check(LabCandidate == C.Expect, *FString::Printf(TEXT("gesture selected %s"), C.Expect));
            LabPhase = 2;
        }
    }

    // ---- marker lifetime, sampled every flight frame ----------------------
    if (Phase == EC26Phase::Delivery)
    {
        if (!Simulation.BounceEvent && !Simulation.CrossedContact)
        {
            Check2(IsBounceIndicatorVisible(), TEXT("marker stays visible until the ball bounces"));
            if (bShotCase && Simulation.Ball.Age > 0.12f) Shot(TEXT("5_marker_inflight"));
        }
        else if (BouncePrediction.bBounced && BouncePrediction.FadeClock > GestureTuning.MarkerFadeDuration + 0.06f)
        {
            Check2(!IsBounceIndicatorVisible(), TEXT("marker fades out once the ball has bounced"));
            if (bShotCase) Shot(TEXT("7_after_bounce"));
        }
        if (Simulation.BounceEvent && !LabSawBounce)
        {
            LabSawBounce = true;
            UE_LOG(LogC26, Display, TEXT("C26_LAB_BOUNCE case=%s shown=(%.1f,%.1f) actual=(%.1f,%.1f) err=%.2fcm alpha=%.2f"),
                Cases[LabCase].Name, BouncePrediction.DisplayLocation.X, BouncePrediction.DisplayLocation.Y,
                Simulation.BouncePosition.X, Simulation.BouncePosition.Y,
                FVector::Dist2D(BouncePrediction.DisplayLocation, Simulation.BouncePosition), BouncePrediction.Alpha);
            Check(FVector::Dist2D(BouncePrediction.DisplayLocation, Simulation.BouncePosition) < 6.f,
                TEXT("marker has converged onto the real bounce point by the bounce"));
            if (bShotCase) Shot(TEXT("6_at_bounce"));
        }
    }
}

// Collapsed per-frame assertion: only the first failure of a delivery is logged,
// so a frame-rate-dependent check cannot bury the report in thousands of lines.
void AC26MatchGameMode::Check2(bool Passed, const TCHAR* Message)
{
    if (Passed) return;
    if (LabQuietFail) return;
    LabQuietFail = true; ++LabFailures;
    UE_LOG(LogC26, Display, TEXT("C26_LAB_CHECK case=%s FAIL %s"),
        LabCase < C26BatLab::NumCases ? C26BatLab::Cases[LabCase].Name : TEXT("-"), Message);
}
#endif
