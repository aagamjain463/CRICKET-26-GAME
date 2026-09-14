#include "../C26MatchGameMode.h"

#if !UE_BUILD_SHIPPING
#include "../C26Athlete.h"
#include "../C26HUD.h"
#include "../C26PlayerController.h"
#include "../C26Settings.h"
#include "../C26Controls.h"
#include "../C26Delivery.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "UnrealClient.h"

// ---------------------------------------------------------------------------
// C26BowlLab - scripted bowling playtest.
//
// The lab IS the bowler. It presses the real delivery carousel, drags the real
// pitch target, turns the real movement dial, moves the real pace slider and
// releases on the real bar - every one of those through
// AC26PlayerController::BeginGesture / MoveGesture / EndGesture in viewport
// pixels, so the HUD hit-testing, the design-space conversion, the pointer
// ownership rules and the one-release guard are all exercised as a thumb would.
//
// Nothing here writes BowlingPlan directly. The plan is read back afterwards,
// and the ball is re-integrated from the delivered FC26DeliveryPlan - once as
// bowled, once with all movement stripped out - so the movement a delivery
// actually produces is MEASURED off the trajectory rather than asserted from
// the parameters that were asked for.
//
//   Tools/BowlLab.sh  ->  -C26BowlLab [-C26BowlLabDir=...] [-C26BowlLabShots]
//                                  [-C26BowlLabFPS=60] [-C26ReverseAlways]
// ---------------------------------------------------------------------------
namespace C26BowlLab
{
    struct FCase
    {
        const TCHAR* Name;
        EC26Delivery Type;
        float Line;             // intended pitch line, cm (+ = off side of a RH batter)
        float Length;           // intended pitch length, cm from the bowler's end
        float Magnitude;        // movement amount requested on the dial, 0..1
        float Pace;             // effort requested on the slider, 0..1
        float ReleaseAt;        // meter value the ball is let go at, 0..1
        bool  bLeftArm;         // which of the two named bowlers (V. SEN is left-arm)
        bool  bLeftBatter;      // force the striker to a left-hander
        bool  bAround;          // around the wicket
        const TCHAR* Note;
        /** How the dial is pushed, in dial radii: 0 = straight up, which sets a
            magnitude with no direction; +1 = toward the off side; -1 = toward
            the leg side. Only meaningful for a delivery whose direction is the
            player's to choose. */
        float DialSide = 0.f;
    };

    // Release points chosen to sit safely inside each band, so a one-frame
    // overshoot can never move a case into a neighbouring band. The meter only
    // ever overshoots UPWARD (the lab releases as soon as it reaches the value),
    // so each point is kept clear of the band's UPPER edge.
    //   ActiveBar (Normal): Early 0.42 | Good 0.62 | Perfect 0.90 | NoBall 0.957
    static constexpr float RelTooEarly = 0.20f;
    static constexpr float RelEarly    = 0.50f;
    static constexpr float RelGood     = 0.72f;
    static constexpr float RelPerfect  = 0.93f;
    static constexpr float RelEdge     = 0.950f;   // just inside the no-ball line
    static constexpr float RelNoBall   = 0.965f;   // just past it

    static const FCase Cases[] =
    {
        // ============ RIGHT-ARM (N. ARCHER) : the delivery library ============
        {TEXT("01_stock_straight"),  EC26Delivery::Pace,         0.f,  520.f, 0.30f, 0.62f, RelPerfect, false, false, false, TEXT("stock ball, no named movement")},
        {TEXT("02_outswing"),        EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("away from a right-hander")},
        {TEXT("03_inswing"),         EC26Delivery::Inswing,    -14.f,  470.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("into a right-hander")},
        {TEXT("04_leg_cutter"),      EC26Delivery::LegCutter,   20.f,  500.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("off the pitch, away")},
        {TEXT("05_off_cutter"),      EC26Delivery::OffCutter,  -18.f,  500.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("off the pitch, in")},
        {TEXT("06_slower_ball"),     EC26Delivery::Slower,      10.f,  560.f, 0.40f, 0.62f, RelPerfect, false, false, false, TEXT("deceived pace, no big movement")},
        {TEXT("07_slower_cutter"),   EC26Delivery::SlowerCutter,20.f,  560.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("slower ball that also grips")},
        {TEXT("08_reverse_out"),     EC26Delivery::ReverseOut,  26.f,  480.f, 0.70f, 0.80f, RelPerfect, false, false, false, TEXT("late air movement, away")},
        {TEXT("09_reverse_in"),      EC26Delivery::ReverseIn,  -14.f,  480.f, 0.70f, 0.80f, RelPerfect, false, false, false, TEXT("late air movement, in")},

        // ================== handedness mirror (left-hand batter) ===============
        {TEXT("10_outswing_lefty"),  EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelPerfect, false, true,  false, TEXT("outswing must mirror for a LH batter")},
        {TEXT("11_inswing_lefty"),   EC26Delivery::Inswing,    -14.f,  470.f, 0.70f, 0.62f, RelPerfect, false, true,  false, TEXT("inswing must mirror for a LH batter")},
        {TEXT("12_legcut_lefty"),    EC26Delivery::LegCutter,   20.f,  500.f, 0.70f, 0.62f, RelPerfect, false, true,  false, TEXT("cutter must mirror for a LH batter")},

        // ===================== pace ladder (identical plan) ====================
        {TEXT("13_pace_low"),        EC26Delivery::Pace,         4.f,  520.f, 0.30f, 0.10f, RelPerfect, false, false, false, TEXT("lowest effort")},
        {TEXT("14_pace_mid"),        EC26Delivery::Pace,         4.f,  520.f, 0.30f, 0.45f, RelPerfect, false, false, false, TEXT("medium effort")},
        {TEXT("15_pace_high"),       EC26Delivery::Pace,         4.f,  520.f, 0.30f, 0.80f, RelPerfect, false, false, false, TEXT("high effort")},
        {TEXT("16_pace_max"),        EC26Delivery::Pace,         4.f,  520.f, 0.30f, 1.00f, RelPerfect, false, false, false, TEXT("maximum effort")},

        // ================== movement magnitude ladder (outswing) ===============
        {TEXT("17_mag_20"),          EC26Delivery::Outswing,    26.f,  470.f, 0.20f, 0.62f, RelPerfect, false, false, false, TEXT("a fifth of the movement")},
        {TEXT("18_mag_50"),          EC26Delivery::Outswing,    26.f,  470.f, 0.50f, 0.62f, RelPerfect, false, false, false, TEXT("half the movement")},
        {TEXT("19_mag_100"),         EC26Delivery::Outswing,    26.f,  470.f, 1.00f, 0.62f, RelPerfect, false, false, false, TEXT("everything the bowler has")},

        // ===================== release band ladder (outswing) ==================
        {TEXT("20_rel_tooearly"),    EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelTooEarly, false, false, false, TEXT("let go far too soon")},
        {TEXT("21_rel_early"),       EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelEarly,    false, false, false, TEXT("let go early")},
        {TEXT("22_rel_good"),        EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelGood,     false, false, false, TEXT("a good release")},
        {TEXT("23_rel_perfect"),     EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("the perfect release")},
        {TEXT("24_rel_edge"),        EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelEdge,     false, false, false, TEXT("as late as possible without overstepping")},
        {TEXT("25_rel_noball"),      EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelNoBall,  false, false, false, TEXT("pushed past the line: a real no-ball")},
        {TEXT("26_rel_after_noball"),EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelPerfect, false, false, false, TEXT("the ball after a no-ball must be normal")},

        // ========================= exact pitch target ==========================
        {TEXT("27_target_yorker"),   EC26Delivery::Pace,         4.f,  795.f, 0.30f, 0.86f, RelPerfect, false, false, false, TEXT("middle-stump yorker")},
        {TEXT("28_target_fourth"),   EC26Delivery::Outswing,    26.f,  470.f, 0.72f, 0.66f, RelPerfect, false, false, false, TEXT("fourth-stump good length")},
        {TEXT("29_target_bouncer"),  EC26Delivery::Pace,        -6.f,   95.f, 0.20f, 0.95f, RelPerfect, false, false, false, TEXT("short ball at the body")},
        {TEXT("30_target_wideyork"), EC26Delivery::Pace,        62.f,  800.f, 0.25f, 0.80f, RelPerfect, false, false, false, TEXT("wide yorker")},

        // ====================== crease position + combinations =================
        {TEXT("31_crease_over"),     EC26Delivery::Pace,        20.f,  520.f, 0.30f, 0.70f, RelPerfect, false, false, false, TEXT("over the wicket: the reference origin")},
        {TEXT("32_crease_around"),   EC26Delivery::Pace,        20.f,  520.f, 0.30f, 0.70f, RelPerfect, false, false, true,  TEXT("around the wicket: same target, new angle"), 0.f},
        {TEXT("33_combo_fast_cut"),  EC26Delivery::LegCutter,   30.f,  520.f, 0.90f, 0.95f, RelPerfect, false, false, false, TEXT("fast leg cutter, max effort")},
        {TEXT("34_combo_slow_off"),  EC26Delivery::OffCutter,  -25.f,  560.f, 0.90f, 0.25f, RelPerfect, false, false, false, TEXT("slow off cutter")},

        // ============ hand-built delivery: a plain ball steered by the dial ====
        // Nothing named is selected here. The player picks a stock ball, pushes
        // the dial off-centre and gets a shape they asked for, which is the
        // whole point of "set swing direction" as a step of its own.
        {TEXT("35_dial_away"),       EC26Delivery::Pace,        20.f,  520.f, 0.75f, 0.62f, RelPerfect, false, false, false, TEXT("plain ball steered away from the batter"), 1.f},
        {TEXT("36_dial_in"),         EC26Delivery::Pace,        20.f,  520.f, 0.75f, 0.62f, RelPerfect, false, false, false, TEXT("plain ball steered into the batter"), -1.f},

        // ============================ LEFT-ARM bowler ==========================
        {TEXT("37_leftarm_out"),     EC26Delivery::Outswing,    26.f,  470.f, 0.70f, 0.62f, RelPerfect, true,  false, false, TEXT("left-arm outswing")},
        {TEXT("38_leftarm_in"),      EC26Delivery::Inswing,    -14.f,  470.f, 0.70f, 0.62f, RelPerfect, true,  false, false, TEXT("left-arm inswing")},
        {TEXT("39_leftarm_slower"),  EC26Delivery::Slower,      10.f,  560.f, 0.40f, 0.62f, RelPerfect, true,  false, false, TEXT("left-arm slower ball")},
    };
    static constexpr int NumCases = UE_ARRAY_COUNT(Cases);

    // Planning sub-states. Anything at or past Done means the ball is away and
    // the lab is only waiting for the innings to hand control back.
    enum : int { StFresh = 0, StType, StTarget, StMovement, StPace, StCrease, StPreview, StStart, StDone };

    // HUD control geometry, in 1600x900 design units. These are the same numbers
    // AC26HUD::Controls draws and AC26MatchGameMode hit-tests, restated here so
    // the lab presses where a thumb would.
    static constexpr float CarouselY = 730.f;
    static constexpr float CarouselPrevX = 83.f;
    static constexpr float CarouselNextX = 339.f;
    // The around-the-wicket toggle sits directly above the delivery-type
    // carousel. Centre of (56, 642, 170x34).
    static constexpr float AroundX = 141.f, AroundY = 659.f;
    static constexpr float StartX = 1389.f, StartY = 730.f;
    static constexpr float DialX = AC26MatchGameMode::DialCentreX, DialY = AC26MatchGameMode::DialCentreY, DialR = AC26MatchGameMode::DialRadius;
    static constexpr float PaceX = AC26MatchGameMode::PaceTrackX, PaceW = AC26MatchGameMode::PaceTrackW, PaceY = AC26MatchGameMode::PaceTrackY;
    /** Neutral press point: on the pitch, clear of every planning control. */
    static const FVector2D ReleaseTap(800.f, 300.f);
}

void AC26MatchGameMode::UpdateBowlLab(float Dt)
{
    (void)Dt;
    using namespace C26BowlLab;
    auto* PC = Cast<AC26PlayerController>(GetWorld()->GetFirstPlayerController());
    auto* HUD = PC ? Cast<AC26HUD>(PC->GetHUD()) : nullptr;
    if (!PC || !HUD) return;

    auto Check = [&](bool Passed, const TCHAR* Message)
    {
        if (!Passed) ++BowlLabFailures;
        UE_LOG(LogC26, Display, TEXT("C26_BOWL_CHECK case=%s %s %s"),
            BowlLabCase < NumCases ? Cases[BowlLabCase].Name : TEXT("-"),
            Passed ? TEXT("PASS") : TEXT("FAIL"), Message);
    };
    // Per-frame assertion that only ever logs its first failure, so a
    // frame-rate-dependent check cannot bury the report.
    auto Check2 = [&](bool Passed, const TCHAR* Message)
    {
        if (Passed || BowlLabQuietFail) return;
        BowlLabQuietFail = true; ++BowlLabFailures;
        UE_LOG(LogC26, Display, TEXT("C26_BOWL_CHECK case=%s FAIL %s"),
            BowlLabCase < NumCases ? Cases[BowlLabCase].Name : TEXT("-"), Message);
    };
    auto Px = [&](FVector2D Design) { return HUD->FromDesign(Design); };
    auto Shot = [&](const TCHAR* Beat)
    {
        if (!BowlLabShots || BowlLabCase >= NumCases) return;
        const FString Key = FString::Printf(TEXT("%s_%s"), Cases[BowlLabCase].Name, Beat);
        if (BowlLabShotKeys.Contains(Key)) return;
        BowlLabShotKeys.Add(Key);
        IFileManager::Get().MakeDirectory(*BowlLabDirectory, true);
        FScreenshotRequest::RequestScreenshot(BowlLabDirectory / (Key + TEXT(".png")), true, false);
        UE_LOG(LogC26, Display, TEXT("C26_BOWL_FRAME %s"), *Key);
    };
    // A fresh over with the player in the field. The two named bowlers are
    // opposite arms, so changing arm means starting a new innings.
    auto BowlInnings = [&](int Team)
    {
        PlayerTeam = Team; PlayerBatsFirst = false; UseToss = false;
        BowlLabTeam = Team;
        StartMatch(); Skip();
    };

    if (FPlatformTime::Seconds() - BowlLabStarted > 900.0)
    {
        UE_LOG(LogC26, Error, TEXT("C26_BOWL_TIMEOUT case=%d substate=%d matchphase=%d"),
            BowlLabCase, BowlLabPhase, int(Phase));
        BowlLab = false; FPlatformMisc::RequestExitWithStatus(false, 1); return;
    }
    if (BowlLabCase >= NumCases)
    {
        UE_LOG(LogC26, Display, TEXT("C26_BOWL_%s deliveries=%d failures=%d"),
            BowlLabFailures ? TEXT("FAIL") : TEXT("PASS"), BowlLabPlayed, BowlLabFailures);
        BowlLab = false; FPlatformMisc::RequestExitWithStatus(false, BowlLabFailures ? 1 : 0); return;
    }

    const FCase& C = Cases[BowlLabCase];

    // ---- keep a live, player-bowling Ready state in front of the lab --------
    if (Phase == EC26Phase::Menu || Phase == EC26Phase::Result) { BowlInnings(BowlLabTeam < 0 ? 0 : BowlLabTeam); return; }
    if (Phase == EC26Phase::Intro) { Skip(); return; }
    if (Phase == EC26Phase::Interval) { BowlInnings(BowlLabTeam < 0 ? 0 : BowlLabTeam); return; }
    if (Phase == EC26Phase::Reaction || Phase == EC26Phase::Replay) { Skip(); return; }
    if (PlayerBatting()) { BowlInnings(BowlLabTeam < 0 ? 0 : BowlLabTeam); return; }
    if (Phase != EC26Phase::Ready && Phase != EC26Phase::RunUp && Phase != EC26Phase::Delivery) return;

    // ---- report the delivery that just finished, then step on ---------------
    if (Phase == EC26Phase::Ready && BowlLabPhase >= StDone)
    {
        const C26::Innings& In = Rules.Now();
        const int Ledger = int(In.Ledger.size());
        if (Rules.Epoch == BowlLabEpoch)
        {
            // Same innings: both the scorecard and the ball count are meaningful.
            Check(Ledger == BowlLabLedgerBefore + 1,
                TEXT("exactly one delivery was committed to the scorecard"));
            const int LegalDelta = In.LegalBalls - BowlLabLegalBefore;
            const int ExtrasDelta = In.Extras - BowlLabExtrasBefore;
            if (BowlLabNoBall)
            {
                Check(ExtrasDelta >= Rules.Config.NoBallPenalty,
                    TEXT("the no-ball actually cost the extra run"));
                Check(LegalDelta == 0, TEXT("a no-ball does not count as a legal ball"));
                if (Rules.Config.FreeHitAfterNoBall)
                    Check(In.FreeHit, TEXT("a no-ball arms the free hit"));
            }
            else
            {
                Check(LegalDelta == 1, TEXT("a legal delivery counts as one ball of the over"));
            }
        }
        UE_LOG(LogC26, Display,
            TEXT("C26_BOWL_RESULT case=%s type=%s band=%s meter=%.3f q=%.2f noball=%d "
                 "wantPace=%.0f actualPace=%.1f wantMag=%.2f swing=%.1f dev=%.1f onset=%.2f "
                 "intent=(%.0f,%.0f) pitch=(%.0f,%.0f) err=%.1fcm "
                 "runs=%d extras=%d legal=%d freehit=%d ledger=%d"),
            C.Name, C26Delivery::Name(BowlLabType), C26Delivery::BandName(BowlLabBand), BowlLabMeter,
            BowlLabQuality, BowlLabNoBall ? 1 : 0, LastPlannedKph, BowlLabKph, BowlLabWantMag,
            BowlLabSwing, BowlLabDeviation, BowlLabOnset,
            BowlLabIntent.X, BowlLabIntent.Y, BowlLabPitch.X, BowlLabPitch.Y,
            FVector::Dist2D(BowlLabIntent, BowlLabPitch),
            In.Runs, In.Extras, In.LegalBalls, In.FreeHit ? 1 : 0, Ledger);

        BowlLabPhase = StFresh; BowlLabQuietFail = false;
        ++BowlLabPlayed; ++BowlLabCase;
        return;
    }

    // ---- planning ----------------------------------------------------------
    if (Phase == EC26Phase::Ready)
    {
        // The arm this case needs.
        const int WantTeam = C.bLeftArm ? 1 : 0;
        if (BowlLabTeam != WantTeam) { BowlInnings(WantTeam); return; }
        // The two sides are opposite arms, so the profile follows the team. If it
        // ever does not, that is a real failure worth reporting - not a reason to
        // restart the innings in a loop until the watchdog fires.
        Check(BowlerProfile.bLeftArm == C.bLeftArm,
            TEXT("the bowler on this side bowls with the arm this case expects"));
        // The striker's hand decides the mirror, and the AI striker is whoever
        // the innings happens to have at the crease, so the lab pins it.
        Rules.Scores[Rules.Current].Striker = C.bLeftBatter ? 1 : 0;

        if (PhaseTime < 0.35f) return;
        if (BowlLabPhase == StFresh)
        {
            // Let one frame render so AC26HUD has registered its button zones,
            // and clear every scrap of per-delivery lab state. Missing one of
            // these is how a harness silently stops testing after ball one.
            BowlLabPhase = StType;
            BowlLabPressed = BowlLabReleased = BowlLabSecondFinger = BowlLabResultLogged = false;
            BowlLabQuietFail = false;
            BowlLabType = EC26Delivery::Pace;
            BowlLabMeter = BowlLabQuality = BowlLabKph = 0.f;
            BowlLabSwing = BowlLabDeviation = BowlLabOnset = 0.f;
            BowlLabPitch = BowlLabIntent = FVector::ZeroVector;
            BowlLabBand = EC26ReleaseBand::TooEarly;
            BowlLabNoBall = false;
            return;
        }

        const bool bGesturePro = !Preferences || Preferences->ControlScheme == 0;
        Check(bGesturePro, TEXT("GesturePro control scheme is active"));

        if (BowlLabPhase == StType)
        {
            BowlLabWantType = C.Type; BowlLabWantLine = C.Line; BowlLabWantLength = C.Length;
            BowlLabWantPace = C.Pace; BowlLabWantMag = C.Magnitude;
            const int32 Before = DeliveryLibrary.IndexOfByKey(BowlingPlan.Type);
            int32 Guard = 0;
            while (BowlingPlan.Type != C.Type && Guard++ <= DeliveryLibrary.Num())
            {
                PC->DebugPointer(0, Px(FVector2D(CarouselNextX, CarouselY)), 0);
                PC->DebugPointer(0, Px(FVector2D(CarouselNextX, CarouselY)), 2);
            }
            Check(BowlingPlan.Type == C.Type,
                *FString::Printf(TEXT("the carousel reached %s"), C26Delivery::Name(C.Type)));
            Check(DeliveryLibrary.Contains(C.Type), TEXT("the delivery is in this bowler's locker"));
            Check(FMath::IsNearlyEqual(BowlingIntendedPitch.Y, BowlingPlan.TargetLength, 1.f),
                TEXT("a carousel press is a button, not a pitch-target drag"));
            if (!C26Delivery::DirectionIsFree(C.Type))
                Check(FMath::IsNearlyEqual(BowlingPlan.MovementDirection,
                    C26Delivery::NaturalDirection(C.Type), 0.001f),
                    TEXT("a named delivery locks its own direction"));
            UE_LOG(LogC26, Display, TEXT("C26_BOWL_PLAN case=%s type=%s libIndex %d->%d of %d"),
                C.Name, C26Delivery::Name(BowlingPlan.Type), Before,
                DeliveryLibrary.IndexOfByKey(BowlingPlan.Type), DeliveryLibrary.Num());
            BowlLabPhase = StTarget;
            return;
        }

        if (BowlLabPhase == StTarget)
        {
            FVector2D Screen;
            const FVector World(C.Line, C.Length, 8.f);
            const bool bProjected = PC->ProjectWorldLocationToScreen(World, Screen);
            Check(bProjected, TEXT("the intended pitch point is on screen and projectable"));
            if (bProjected)
            {
                const FVector2D D = HUD->ToDesign(Screen);
                // The press must not land on a planning widget, or it is not a
                // target drag at all.
                Check(!IsOnPaceSlider(D) && !IsOnMovementDial(D),
                    TEXT("the pitch target press is clear of the dial and the slider"));
                PC->DebugPointer(0, Screen, 0);
                PC->DebugPointer(0, Screen, 1);
                PC->DebugPointer(0, Screen, 2);
                const float LineErr = FMath::Abs(BowlingPlan.TargetLine - C.Line);
                const float LenErr = FMath::Abs(BowlingPlan.TargetLength - C.Length);
                // A drag target is picked in screen pixels, and one pixel is a
                // few centimetres of pitch at the planning camera's distance, so
                // the tolerance is about 1% of the pitch rather than exact.
                Check(LineErr <= 6.f && LenErr <= 25.f,
                    *FString::Printf(TEXT("the dragged target landed on the requested point (%.1f, %.1f cm off)"),
                        LineErr, LenErr));
                UE_LOG(LogC26, Display, TEXT("C26_BOWL_TARGET case=%s want=(%.0f,%.0f) got=(%.0f,%.0f)"),
                    C.Name, C.Line, C.Length, BowlingPlan.TargetLine, BowlingPlan.TargetLength);
            }
            BowlLabPhase = StMovement;
            return;
        }

        if (BowlLabPhase == StMovement)
        {
            const float M = FMath::Clamp(FMath::Abs(C.Magnitude), 0.f, 1.f);
            // The dial is pushed from its own centre. Straight up sets an amount
            // with no direction; off to one side sets the direction as well, and
            // stays clear of the pace slider's hit band either way.
            const FVector2D Dial = C.DialSide == 0.f
                ? FVector2D(DialX, DialY - M * DialR)
                : FVector2D(DialX + C.DialSide * M * DialR, DialY);
            Check(IsOnMovementDial(Dial), TEXT("the dial press is inside the dial's hit area"));
            PC->DebugPointer(0, Px(Dial), 0);
            Check(bMovementDragging && MovementPointerId == 0,
                TEXT("the dial drag is owned by the pointer that started it"));
            PC->DebugPointer(0, Px(Dial), 1);

            // Multi-touch: a second finger on the pace slider must take its own
            // control without stealing the dial, and must not move the dial.
            {
                const float MagBefore = BowlingPlan.MovementMagnitude;
                const FVector2D Other(PaceX + PaceW * 0.5f, PaceY);
                PC->DebugPointer(1, Px(Other), 0);
                PC->DebugPointer(1, Px(Other), 1);
                Check(PacePointerId == 1 && MovementPointerId == 0,
                    TEXT("two fingers own two controls without stealing each other"));
                Check(FMath::IsNearlyEqual(BowlingPlan.MovementMagnitude, MagBefore, 0.001f),
                    TEXT("a finger on the pace slider cannot move the movement dial"));
                PC->DebugPointer(1, Px(Other), 2);
                Check(MovementPointerId == 0,
                    TEXT("lifting the pace finger does not release the movement dial"));
            }

            PC->DebugPointer(0, Px(Dial), 1);
            const float GotMag = BowlingPlan.MovementMagnitude;
            PC->DebugPointer(0, Px(Dial), 2);
            Check(FMath::Abs(GotMag - M) <= 0.03f,
                *FString::Printf(TEXT("the dial set the requested movement amount (%.2f vs %.2f)"), GotMag, M));
            if (C.DialSide != 0.f)
                Check(FMath::Sign(BowlingPlan.MovementDirection) == FMath::Sign(C.DialSide)
                    && FMath::Abs(BowlingPlan.MovementDirection) > 0.5f,
                    *FString::Printf(TEXT("pushing the dial off-centre set the movement direction (%.2f)"),
                        BowlingPlan.MovementDirection));
            Check(!bMovementDragging, TEXT("lifting on the dial releases ownership"));
            UE_LOG(LogC26, Display, TEXT("C26_BOWL_MOVEMENT case=%s want=%.2f got=%.2f dir=%.2f"),
                C.Name, M, GotMag, BowlingPlan.MovementDirection);
            BowlLabPhase = StPace;
            return;
        }

        if (BowlLabPhase == StPace)
        {
            const FVector2D Track(PaceX + PaceW * FMath::Clamp(C.Pace, 0.f, 1.f), PaceY);
            Check(IsOnPaceSlider(Track), TEXT("the pace press is inside the slider's hit area"));
            PC->DebugPointer(0, Px(Track), 0);
            Check(PacePointerId == 0, TEXT("the pace drag is owned by the pointer that started it"));
            PC->DebugPointer(0, Px(Track), 1);
            const float GotPace = BowlingPlan.PaceNormalized;
            PC->DebugPointer(0, Px(Track), 2);
            Check(FMath::Abs(GotPace - C.Pace) <= 0.02f,
                *FString::Printf(TEXT("the slider set the requested effort (%.2f vs %.2f)"), GotPace, C.Pace));
            float Lo, Hi; PaceRangeKph(Lo, Hi);
            const float WantKph = PlannedKph();
            Check(WantKph >= Lo - 0.5f && WantKph <= Hi + 0.5f,
                *FString::Printf(TEXT("the requested pace sits inside the bowler's range (%.0f in %.0f-%.0f)"),
                    WantKph, Lo, Hi));
            UE_LOG(LogC26, Display, TEXT("C26_BOWL_PACE case=%s effort=%.2f want=%.0fkph range=%.0f-%.0f"),
                C.Name, GotPace, WantKph, Lo, Hi);
            BowlLabPhase = StCrease;
            return;
        }

        if (BowlLabPhase == StCrease)
        {
            if (BowlingPlan.bAroundWicket != C.bAround)
            {
                PC->DebugPointer(0, Px(FVector2D(AroundX, AroundY)), 0);
                PC->DebugPointer(0, Px(FVector2D(AroundX, AroundY)), 2);
            }
            Check(BowlingPlan.bAroundWicket == C.bAround,
                C.bAround ? TEXT("around the wicket is selected") : TEXT("over the wicket is selected"));
            BowlLabPhase = StPreview;
            return;
        }

        if (BowlLabPhase == StPreview)
        {
            // The projected path is deliberately suppressed for a clean bowling view,
            // so the HUD renders no spline. Assert it stays empty: that is the contract
            // now, and it catches an accidental re-enable.
            Check(TrajectoryPreview.Num() == 0, TEXT("the projected path is suppressed for a clean bowling view"));
            const FC26DeliveryPlan Preview = PreviewDelivery();
            Check(Preview.Type == BowlingPlan.Type && Preview.Movement == C26Delivery::MovementOf(BowlingPlan.Type),
                TEXT("the previewed delivery is the selected delivery"));
            Check(FMath::IsNearlyEqual(Preview.Line, BowlingPlan.TargetLine, 0.5f)
                && FMath::IsNearlyEqual(Preview.Length, BowlingPlan.TargetLength, 0.5f),
                TEXT("the previewed path is aimed at the chosen pitch point"));
            Shot(TEXT("1_plan"));
            BowlLabPhase = StStart;
            return;
        }

        if (BowlLabPhase == StStart)
        {
            BowlLabLedgerBefore = int(Rules.Now().Ledger.size());
            BowlLabExtrasBefore = Rules.Now().Extras;
            BowlLabLegalBefore = Rules.Now().LegalBalls;
            BowlLabEpoch = Rules.Epoch;
            BowlLabReleaseAt = C.ReleaseAt;
            PC->DebugPointer(0, Px(FVector2D(StartX, StartY)), 0);
            PC->DebugPointer(0, Px(FVector2D(StartX, StartY)), 2);
            Check(Phase == EC26Phase::RunUp, TEXT("START RUN-UP leaves the planning state"));
            Check(BowlingState == EC26BowlingState::RunUp, TEXT("the bowling state machine is in RunUp"));
            Check(LockedBowling.Type == C.Type, TEXT("the locked plan is the delivery that was selected"));
            UE_LOG(LogC26, Display, TEXT("C26_BOWL_BEGIN case=%s type=%s pace=%.0fkph mag=%.2f around=%d hand=%s note=%s"),
                C.Name, C26Delivery::Name(LockedBowling.Type), LastPlannedKph,
                BowlingPlan.MovementMagnitude, BowlingPlan.bAroundWicket ? 1 : 0,
                BowlerProfile.bLeftArm ? TEXT("LEFT-ARM") : TEXT("RIGHT-ARM"), C.Note);
            Shot(TEXT("2_runup"));
            BowlLabPhase = StDone;
            return;
        }
    }

    // ---- the run-up: wait for the meter, then let go -------------------------
    if (Phase == EC26Phase::RunUp && BowlLabPhase >= StDone)
    {
        // Released is the state the release itself writes; it is legitimate for
        // the rest of the run-up while the arm comes over.
        Check2(BowlingState == EC26BowlingState::RunUp || BowlingState == EC26BowlingState::ReleaseWindow
            || BowlingState == EC26BowlingState::Released,
            TEXT("the run-up holds a bowling state"));
        Check2(!ReleaseLocked || BowlLabReleased, TEXT("the ball is not released before the meter reaches the point"));
        if (!BowlLabReleased && BowlingMeter() >= BowlLabReleaseAt)
        {
            BowlLabReleased = true;
            BowlLabOrigin = ReleaseOriginCm();
            const FVector2D Tap = Px(ReleaseTap);
            // The release is one press anywhere on the pitch.
            PC->DebugPointer(0, Tap, 0);
            PC->DebugPointer(0, Tap, 2);
            Check(ReleaseLocked, TEXT("the release committed exactly once"));
            Check(!BallReleased, TEXT("the ball has not left the hand yet at the moment of release"));
            // A duplicate press from another finger must change nothing.
            PC->DebugPointer(1, Tap, 0);
            PC->DebugPointer(1, Tap, 2);
            Check(ReleaseLocked, TEXT("a second finger cannot re-release the ball"));

            BowlLabMeter = ReleaseMeterValue;
            BowlLabBand = ReleaseBand;
            BowlLabQuality = ReleaseBandQuality;
            BowlLabNoBall = bBowlingNoBall;
            BowlLabKph = LastActualKph;
            BowlLabIntent = BowlingIntendedPitch;
            UE_LOG(LogC26, Display,
                TEXT("C26_BOWL_RELEASE case=%s meter=%.3f band=%s quality=%.2f noball=%d bar=[%.3f %.3f %.3f %.3f]"),
                C.Name, BowlLabMeter, C26Delivery::BandName(BowlLabBand), BowlLabQuality,
                BowlLabNoBall ? 1 : 0, ActiveBar.EarlyStart, ActiveBar.GoodStart,
                ActiveBar.PerfectStart, ActiveBar.NoBallStart);
            Shot(TEXT("3_release"));
        }
    }

    // ---- the flight: capture what the delivery actually did -------------------
    if ((Phase == EC26Phase::RunUp || Phase == EC26Phase::Delivery) && BowlLabReleased && !BowlLabResultLogged
        && BallReleased)
    {
        BowlLabResultLogged = true;
        BowlLabType = Bowling.Type;
        BowlLabSwing = Bowling.Swing;
        BowlLabDeviation = Bowling.Deviation;
        BowlLabOnset = Bowling.SwingOnset;
        BowlLabKph = C26Delivery::UnitsToKph(Bowling.Speed);
        BowlLabPitch = BowlingActualPitch;
        // The real release point, recorded by the release itself. Re-integrating
        // from anywhere else would be comparing a ball to a different ball.
        if (bHasBowlingOrigin) BowlLabOrigin = LastBowlingOrigin;

        // Re-integrate the DELIVERED plan, then the same plan with every movement
        // term stripped out. The difference is the movement this ball really
        // produced, measured off the trajectory rather than read back off the
        // parameter that asked for it.
        //
        // FC26Simulation::Release trims the launch so that the movement still
        // touches down on the aim point. The stripped plan would therefore leave
        // the hand on a different line, and comparing where each one pitches
        // would measure the AIM, not the movement. Handing the stripped ball the
        // identical launch leaves the movement as the only difference between
        // the two flights.
        FC26Simulation With, Without;
        With.Tuning = Tuning; Without.Tuning = Tuning;
        With.Release(Bowling, BowlLabOrigin);
        FC26DeliveryPlan Bare = Bowling;
        Bare.Swing = 0.f; Bare.Deviation = 0.f; Bare.Seam = 0.f; Bare.SwingOnset = 0.f;
        Without.Release(Bare, BowlLabOrigin);
        Without.Ball.Velocity = With.Ball.Velocity;

        const float BounceT = With.BounceTime > 0.f ? With.BounceTime : With.ContactTime;
        const float ContactT = FMath::Max(BounceT, With.ContactTime);
        // Lateral separation of the two flights at three points along the ball's
        // journey: half way there, at the pitch, and by the time it reaches the bat.
        const float MidMove = With.Predict(BounceT * 0.5f).Position.X
                            - Without.Predict(BounceT * 0.5f).Position.X;
        const float AirMove = With.Predict(BounceT).Position.X - Without.Predict(BounceT).Position.X;
        const float TotalMove = With.Predict(ContactT).Position.X - Without.Predict(ContactT).Position.X;
        const float PostMove = TotalMove - AirMove;   // gained after the ball pitched
        const float TargetErr = FVector::Dist2D(BowlingIntendedPitch, BowlingActualPitch);

        UE_LOG(LogC26, Display,
            TEXT("C26_BOWL_FLIGHT case=%s midMove=%+.1fcm airMove=%+.1fcm postMove=%+.1fcm totalMove=%+.1fcm targetErr=%.1fcm"),
            C.Name, MidMove, AirMove, PostMove, TotalMove, TargetErr);

        // --- the type actually bowled is the type the player chose -----------
        Check(Bowling.Type == C.Type, TEXT("the bowled delivery is the selected type"));
        Check(Bowling.Movement == C26Delivery::MovementOf(C.Type),
            TEXT("the movement phase matches the delivery type"));

        const EC26Movement Mov = C26Delivery::MovementOf(C.Type);
        const float BatterSign = C.bLeftBatter ? -1.f : 1.f;
        const float WantDir = C26Delivery::DirectionIsFree(C.Type)
            ? FMath::Clamp(BowlingPlan.MovementDirection, -1.f, 1.f)
            : C26Delivery::NaturalDirection(C.Type);
        const float ExpectWorldDir = WantDir * BatterSign;

        // --- swing and cutter are different phases of flight, not one renamed --
        if (Mov == EC26Movement::Swing || Mov == EC26Movement::ReverseSwing)
        {
            // A swing delivery's lateral movement is generated IN THE AIR, and
            // the trajectory has to show it - not just the parameter that asked
            // for it. Stripping the movement must visibly straighten the ball.
            Check(FMath::Abs(AirMove) > 1.5f, TEXT("a swing delivery moves in the air before the pitch"));
            // ...and it is the swing term doing the work, not a seam break in disguise.
            Check(FMath::IsNearlyZero(Bowling.Deviation), TEXT("a swing delivery takes no break off the pitch"));
            Check(ExpectWorldDir == 0.f || FMath::Sign(Bowling.Swing) == FMath::Sign(ExpectWorldDir),
                TEXT("the swing direction is batter-relative and correctly mirrored"));
            Check(FMath::Sign(AirMove) == FMath::Sign(Bowling.Swing),
                TEXT("the ball swings the way the delivery says it swings"));
        }
        else if (Mov == EC26Movement::Seam || Mov == EC26Movement::Spin)
        {
            Check(FMath::Abs(PostMove) > 1.5f, TEXT("a cutter moves off the pitch after it bounces"));
            Check(FMath::Abs(AirMove) < 1.5f, TEXT("a cutter does NOT swing in the air"));
            Check(FMath::Abs(Bowling.Deviation) > 1.f, TEXT("a cutter carries post-bounce deviation"));
            Check(ExpectWorldDir == 0.f || FMath::Sign(Bowling.Deviation) == FMath::Sign(ExpectWorldDir),
                TEXT("the cutter's break is batter-relative and correctly mirrored"));
            Check(FMath::Sign(PostMove) == FMath::Sign(Bowling.Deviation),
                TEXT("the ball breaks off the pitch the way the delivery says it breaks"));
        }
        else
        {
            Check(FMath::Abs(TotalMove) < 12.f, TEXT("the stock ball is not a big mover"));
            // A steered stock ball is a delivery the player assembled by hand out
            // of a plain ball and the dial. The direction they pushed has to
            // reach the ball itself, not merely the arrow on the screen.
            if (FMath::Abs(WantDir) > 0.15f)
            {
                Check(FMath::Abs(AirMove) > 1.5f,
                    TEXT("a steered plain ball swings in the air, like the dial says it will"));
                Check(FMath::Sign(AirMove) == FMath::Sign(ExpectWorldDir),
                    TEXT("a steered plain ball moves the way the dial was pushed"));
            }
        }

        // --- reverse swing is late, and only reverse swing is late -----------
        if (Mov == EC26Movement::ReverseSwing)
        {
            Check(Bowling.SwingOnset > 0.f, TEXT("reverse swing starts late in the flight"));
            // The proof of lateness is on the trajectory, not in the parameter:
            // half way to the pitch the ball is still travelling straight, and
            // the bend only shows up after that.
            Check(FMath::Abs(MidMove) < FMath::Abs(AirMove) * 0.20f,
                TEXT("reverse swing is still going straight at half flight and bends late"));
            Check(DeliveryLibrary.Contains(C.Type), TEXT("reverse swing is available under these conditions"));
        }
        else if (Mov != EC26Movement::None)
        {
            Check(FMath::IsNearlyZero(Bowling.SwingOnset), TEXT("only reverse swing has a late onset"));
            // Conventional swing, by contrast, bends from the moment it is let go.
            if (Mov == EC26Movement::Swing)
                Check(FMath::Abs(MidMove) > FMath::Abs(AirMove) * 0.12f,
                    TEXT("conventional swing bends from the moment it is released"));
        }

        // --- pace -----------------------------------------------------------
        // A clean release delivers the pace that was asked for; a ragged one
        // loses some of it, and that is the model working rather than failing.
        // What must never happen is the ball coming out FASTER than requested,
        // or losing more pace than the tuning allows.
        const float PaceFloor = LastPlannedKph * (1.f - BowlingTuning.MaxPaceLoss) - 1.5f;
        Check(BowlLabKph <= LastPlannedKph + 1.5f && BowlLabKph >= PaceFloor,
            *FString::Printf(TEXT("the ball came out at the requested pace (%.1f vs %.0f km/h, floor %.1f)"),
                BowlLabKph, LastPlannedKph, PaceFloor));
        if (BowlLabBand == EC26ReleaseBand::Perfect)
            Check(FMath::Abs(BowlLabKph - LastPlannedKph) < 2.f,
                *FString::Printf(TEXT("a perfect release loses no pace at all (%.1f vs %.0f km/h)"),
                    BowlLabKph, LastPlannedKph));
        if (C.Type == EC26Delivery::Slower || C.Type == EC26Delivery::SlowerCutter)
            Check(BowlLabKph < BowlerProfile.MaxSpeedKph - 6.f,
                TEXT("a slower ball is materially slower than the bowler's stock"));

        // --- release quality -------------------------------------------------
        const EC26ReleaseBand WantBand = C26Delivery::BandAt(C.ReleaseAt, ActiveBar);
        Check(BowlLabBand == WantBand,
            *FString::Printf(TEXT("the release landed in the intended band (%s)"),
                C26Delivery::BandName(WantBand)));
        Check(FMath::IsNearlyEqual(BowlLabMeter, ReleaseMeterValue, 0.0001f),
            TEXT("the recorded meter value is the value the release used"));
        if (BowlLabBand == EC26ReleaseBand::Perfect)
            Check(BowlLabQuality > 0.80f, TEXT("a perfect release scores high execution"));
        else if (BowlLabBand == EC26ReleaseBand::Good)
            Check(BowlLabQuality > 0.40f && BowlLabQuality <= 0.96f, TEXT("a good release scores mid execution"));
        else if (BowlLabBand == EC26ReleaseBand::Early || BowlLabBand == EC26ReleaseBand::TooEarly)
            Check(BowlLabQuality < 0.80f, TEXT("an early release scores lower execution"));

        // --- a perfect release must be more accurate than a poor one ---------
        if (BowlLabBand == EC26ReleaseBand::Perfect)
            Check(TargetErr < 26.f,
                *FString::Printf(TEXT("a perfect release lands near the intended point (%.1f cm)"), TargetErr));

        // --- no-ball is a real no-ball, not a badge --------------------------
        Check(BowlLabNoBall == (BowlLabBand == EC26ReleaseBand::NoBall),
            TEXT("the no-ball flag follows the meter, never a roll"));

        // --- crease position changes the release point, not the target -------
        if (C.bAround && BowlLabHasPrev)
        {
            const float Shift = FMath::Abs(BowlLabOrigin.X - BowlLabPrevOrigin.X);
            Check(FMath::Abs(Shift - BowlingTuning.AroundWicketOffsetCm) < 4.f,
                *FString::Printf(TEXT("around the wicket shifts the release point (%.1f cm)"), Shift));
            // The whole point of the crease is a new angle to the SAME target.
            Check(FVector::Dist2D(BowlLabIntent, BowlLabPrevPitch) < 40.f,
                TEXT("around the wicket changes the angle, not where the ball is aimed"));
        }
        BowlLabPrevOrigin = BowlLabOrigin;
        BowlLabPrevPitch = BowlLabIntent;
        BowlLabHasPrev = true;

        if (C.Type != EC26Delivery::Slower && C.Type != EC26Delivery::SlowerCutter
            && WantBand == EC26ReleaseBand::Perfect)
            Shot(TEXT("4_flight"));
    }
}
#endif
