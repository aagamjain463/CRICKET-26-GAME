#include "C26PresentationDirector.h"
#include "C26MatchGameMode.h"
#include "C26CameraDirector.h"
#include "C26Athlete.h"
#include "C26HUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UC26PresentationDirector::UC26PresentationDirector()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PacingMode = EC26PresentationPacing::Balanced;
    bPresentationActive = false;
    ActiveEvent = EC26PresentationEvent::None;
    SceneTime = 0.f;
    bTossCoinActive = false;
    TossCoinTimer = 0.f;
    bDebugOverlayVisible = false;
    InitializeSceneLibrary();
}

void UC26PresentationDirector::BeginPlay()
{
    Super::BeginPlay();
    if (SceneLibrary.Num() == 0)
    {
        InitializeSceneLibrary();
    }
}

AC26MatchGameMode* UC26PresentationDirector::GetGameMode() const
{
    return Cast<AC26MatchGameMode>(GetOwner());
}

void UC26PresentationDirector::InitializeSceneLibrary()
{
    SceneLibrary.Empty();

    auto AddDef = [this](EC26PresentationEvent Event, FName VariantId, float Duration, EC26CinematicCamera Lens,
                         EC26PresentationPriority Priority, const FString& Overlay, const FString& Sting,
                         const FString& Crowd, EC26Action ActionP1, EC26Action ActionP2, float Cooldown)
    {
        FC26PresentationSceneDefinition Def;
        Def.Event = Event;
        Def.VariantId = VariantId;
        Def.Duration = Duration;
        Def.CameraAngle = Lens;
        Def.Priority = Priority;
        Def.OverlayType = Overlay;
        Def.AudioSting = Sting;
        Def.CrowdRoarType = Crowd;
        Def.Participant1Action = ActionP1;
        Def.Participant2Action = ActionP2;
        Def.CooldownSeconds = Cooldown;
        Def.bCanBeSkipped = true;

        SceneLibrary.FindOrAdd(Event).Add(Def);
    };

    // 1. Toss Ceremony (4 events)
    AddDef(EC26PresentationEvent::TossCeremonyWalkout, TEXT("Toss_CaptainsWalkout"), 3.5f,
           EC26CinematicCamera::StadiumWide, EC26PresentationPriority::High, TEXT("TossCard"),
           TEXT("Sting_Toss"), TEXT("Crowd_Applause"), EC26Action::Ready, EC26Action::Ready, 60.f);

    AddDef(EC26PresentationEvent::TossCoinFlip, TEXT("Toss_CoinFlipStandard"), 3.2f,
           EC26CinematicCamera::HighAngleToss, EC26PresentationPriority::High, TEXT("TossCard"),
           TEXT("Sting_CoinFlip"), TEXT("Crowd_Murmur"), EC26Action::TossFlip, EC26Action::Ready, 60.f);

    AddDef(EC26PresentationEvent::TossDecisionBat, TEXT("Toss_ElectedToBat"), 2.8f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::High, TEXT("TossCard"),
           TEXT("Sting_Decision"), TEXT("Crowd_Cheer"), EC26Action::Discuss, EC26Action::Handshake, 60.f);

    AddDef(EC26PresentationEvent::TossDecisionBowl, TEXT("Toss_ElectedToBowl"), 2.8f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::High, TEXT("TossCard"),
           TEXT("Sting_Decision"), TEXT("Crowd_Cheer"), EC26Action::Discuss, EC26Action::Handshake, 60.f);

    // 2. Team Walkout & Openings (3 events)
    AddDef(EC26PresentationEvent::TeamWalkoutAnthem, TEXT("Walkout_FieldingSide"), 3.6f,
           EC26CinematicCamera::StadiumWide, EC26PresentationPriority::High, TEXT("TeamCard"),
           TEXT("Sting_Anthem"), TEXT("Crowd_Roar"), EC26Action::Ready, EC26Action::Ready, 90.f);

    AddDef(EC26PresentationEvent::OpeningBattersWalkIn, TEXT("WalkIn_OpeningBatters"), 3.2f,
           EC26CinematicCamera::PitchTrackWalking, EC26PresentationPriority::High, TEXT("BatterCard"),
           TEXT("Sting_Opening"), TEXT("Crowd_Applause"), EC26Action::Ready, EC26Action::Ready, 90.f);

    AddDef(EC26PresentationEvent::OpeningBowlerRunupPrep, TEXT("Prep_BowlerMarkCheck"), 2.6f,
           EC26CinematicCamera::OverShoulderBowler, EC26PresentationPriority::Medium, TEXT("BowlerCard"),
           TEXT("Sting_Tension"), TEXT("Crowd_Murmur"), EC26Action::Ready, EC26Action::Ready, 60.f);

    // 3. Batter Discussions & Reactions (4 events, multiple variants for variety)
    AddDef(EC26PresentationEvent::BatterMidPitchDiscussion, TEXT("Batter_DiscussPitchStrategy"), 2.4f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT("Crowd_Murmur"), EC26Action::Discuss, EC26Action::Discuss, 25.f);
    AddDef(EC26PresentationEvent::BatterMidPitchDiscussion, TEXT("Batter_DiscussNodAgree"), 2.2f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT("Crowd_Murmur"), EC26Action::GloveTap, EC26Action::Discuss, 25.f);

    AddDef(EC26PresentationEvent::BatterBoundaryMeeting, TEXT("Batter_GloveTapBoundary"), 2.4f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Medium, TEXT(""),
           TEXT("Sting_Boundary"), TEXT("Crowd_Cheer"), EC26Action::GloveTap, EC26Action::GloveTap, 20.f);

    AddDef(EC26PresentationEvent::BatterCloseCallReview, TEXT("Batter_CloseCallConfer"), 2.6f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Medium, TEXT(""),
           TEXT(""), TEXT("Crowd_Ooh"), EC26Action::Discuss, EC26Action::Discuss, 30.f);

    AddDef(EC26PresentationEvent::BatterEndOverDiscussion, TEXT("Batter_EndOverWalkTogether"), 2.8f,
           EC26CinematicCamera::PitchTrackWalking, EC26PresentationPriority::Low, TEXT("OverSummary"),
           TEXT(""), TEXT("Crowd_Applause"), EC26Action::Discuss, EC26Action::GloveTap, 25.f);

    // 4. Bowler / Captain / Fielder Tactical Discussions (3 events)
    AddDef(EC26PresentationEvent::BowlerCaptainDiscussion, TEXT("Bowler_CaptainFieldAdjust"), 2.6f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Medium, TEXT(""),
           TEXT(""), TEXT("Crowd_Murmur"), EC26Action::Discuss, EC26Action::Discuss, 30.f);
    AddDef(EC26PresentationEvent::BowlerCaptainDiscussion, TEXT("Bowler_CaptainEncourage"), 2.2f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Medium, TEXT(""),
           TEXT(""), TEXT("Crowd_Murmur"), EC26Action::Discuss, EC26Action::FistPump, 30.f);

    AddDef(EC26PresentationEvent::BowlerKeeperFieldAdjust, TEXT("Bowler_KeeperAngleConsult"), 2.4f,
           EC26CinematicCamera::OverShoulderBowler, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT("Crowd_Murmur"), EC26Action::Discuss, EC26Action::Discuss, 35.f);

    AddDef(EC26PresentationEvent::BowlerFielderCongratulate, TEXT("Bowler_FielderHighFive"), 2.0f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT("Crowd_Applause"), EC26Action::Handshake, EC26Action::GloveTap, 25.f);

    // 5. Bowler Frustrations & Desperation (3 events)
    AddDef(EC26PresentationEvent::BowlerFrustrationDot, TEXT("Bowler_TurnAwayDot"), 1.8f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT("Crowd_Ooh"), EC26Action::Disappointed, EC26Action::Ready, 20.f);

    AddDef(EC26PresentationEvent::BowlerFrustrationBoundary, TEXT("Bowler_HandsOnHipsBoundary"), 2.2f,
           EC26CinematicCamera::LowAngleDramatic, EC26PresentationPriority::Medium, TEXT(""),
           TEXT(""), TEXT("Crowd_Cheer"), EC26Action::Disappointed, EC26Action::Ready, 25.f);

    AddDef(EC26PresentationEvent::BowlerAppealingDesperate, TEXT("Bowler_TurnToUmpireAppeal"), 2.5f,
           EC26CinematicCamera::OverShoulderBowler, EC26PresentationPriority::High, TEXT(""),
           TEXT("Sting_Appeal"), TEXT("Crowd_Roar"), EC26Action::Celebrate, EC26Action::Ready, 30.f);

    // 6. Batter Disappointments & Survival (3 events)
    AddDef(EC26PresentationEvent::BatterDotDisappointment, TEXT("Batter_HeadShakeDot"), 1.8f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Low, TEXT(""),
           TEXT(""), TEXT(""), EC26Action::Disappointed, EC26Action::Ready, 20.f);

    AddDef(EC26PresentationEvent::BatterPlayAndMissReaction, TEXT("Batter_InspectBladeMiss"), 2.2f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Medium, TEXT(""),
           TEXT(""), TEXT("Crowd_Ooh"), EC26Action::Disappointed, EC26Action::Ready, 25.f);

    AddDef(EC26PresentationEvent::BatterCloseRunoutSurvival, TEXT("Batter_PhewSlideSafe"), 2.6f,
           EC26CinematicCamera::LowAngleDramatic, EC26PresentationPriority::High, TEXT(""),
           TEXT("Sting_Tension"), TEXT("Crowd_Applause"), EC26Action::GloveTap, EC26Action::Ready, 35.f);

    // 7. Wicket Celebrations & Variants (4 events)
    AddDef(EC26PresentationEvent::WicketCelebrationBowled, TEXT("Bowled_CleanKnockRoar"), 3.4f,
           EC26CinematicCamera::OrbitCelebration, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::Celebrate, EC26Action::Disappointed, 15.f);
    AddDef(EC26PresentationEvent::WicketCelebrationBowled, TEXT("Bowled_FistPumpSprint"), 3.0f,
           EC26CinematicCamera::LowAngleDramatic, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::FistPump, EC26Action::Disappointed, 15.f);

    AddDef(EC26PresentationEvent::WicketCelebrationCaught, TEXT("Caught_CatcherHugBowler"), 3.2f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::Celebrate, EC26Action::Celebrate, 15.f);
    AddDef(EC26PresentationEvent::WicketCelebrationCaught, TEXT("Caught_CrowdPointing"), 2.8f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::FistPump, EC26Action::Disappointed, 15.f);

    AddDef(EC26PresentationEvent::WicketCelebrationLBW, TEXT("LBW_UmpireFingerUp"), 3.2f,
           EC26CinematicCamera::UmpirePOV, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::Celebrate, EC26Action::Disappointed, 15.f);

    AddDef(EC26PresentationEvent::WicketCelebrationRunOut, TEXT("RunOut_StumpDirectHit"), 3.0f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT("Sting_Wicket"), TEXT("Crowd_Roar"), EC26Action::Celebrate, EC26Action::Disappointed, 15.f);

    // 8. Wicket Follow-up (2 events)
    AddDef(EC26PresentationEvent::WicketDisappointmentBatterWalk, TEXT("Wicket_BatterWalkOff"), 3.2f,
           EC26CinematicCamera::PitchTrackWalking, EC26PresentationPriority::High, TEXT("WicketCard"),
           TEXT(""), TEXT("Crowd_Applause"), EC26Action::Disappointed, EC26Action::Ready, 15.f);

    AddDef(EC26PresentationEvent::NewBatterEntry, TEXT("NewBatter_WalkToCrease"), 3.0f,
           EC26CinematicCamera::PitchTrackWalking, EC26PresentationPriority::High, TEXT("NewBatterCard"),
           TEXT("Sting_Entry"), TEXT("Crowd_Applause"), EC26Action::Ready, EC26Action::Ready, 20.f);

    // 9. Fifty & Century Milestones (2 events, exact fire-once)
    AddDef(EC26PresentationEvent::FiftyCelebration, TEXT("Fifty_BatSaluteDressingRoom"), 3.8f,
           EC26CinematicCamera::OrbitCelebration, EC26PresentationPriority::Critical, TEXT("MilestoneFifty"),
           TEXT("Sting_Milestone"), TEXT("Crowd_MassiveCheer"), EC26Action::BatRaise, EC26Action::Celebrate, 120.f);
    AddDef(EC26PresentationEvent::FiftyCelebration, TEXT("Fifty_PartnerGloveClash"), 3.4f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Critical, TEXT("MilestoneFifty"),
           TEXT("Sting_Milestone"), TEXT("Crowd_MassiveCheer"), EC26Action::BatRaise, EC26Action::GloveTap, 120.f);

    AddDef(EC26PresentationEvent::CenturyCelebration, TEXT("Century_HelmetOffBatSalute"), 4.8f,
           EC26CinematicCamera::OrbitCelebration, EC26PresentationPriority::Critical, TEXT("MilestoneCentury"),
           TEXT("Sting_Century"), TEXT("Crowd_Thunderous"), EC26Action::BatRaise, EC26Action::Celebrate, 300.f);
    AddDef(EC26PresentationEvent::CenturyCelebration, TEXT("Century_PartnerEmbrace"), 4.4f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Critical, TEXT("MilestoneCentury"),
           TEXT("Sting_Century"), TEXT("Crowd_Thunderous"), EC26Action::BatRaise, EC26Action::Handshake, 300.f);

    // 10. End of Over & Innings Breaks (2 events)
    AddDef(EC26PresentationEvent::EndOfOverSummaryCard, TEXT("EndOver_FullSummary"), 3.2f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Medium, TEXT("OverSummary"),
           TEXT("Sting_OverSummary"), TEXT("Crowd_Murmur"), EC26Action::Discuss, EC26Action::Ready, 15.f);

    AddDef(EC26PresentationEvent::InningsBreakTransition, TEXT("Innings_BreakOverview"), 4.0f,
           EC26CinematicCamera::StadiumWide, EC26PresentationPriority::High, TEXT("InningsBreak"),
           TEXT("Sting_InningsEnd"), TEXT("Crowd_Applause"), EC26Action::Ready, EC26Action::Ready, 180.f);

    // 11. Pressure & Tension Setups (2 events)
    AddDef(EC26PresentationEvent::FinalOverTensionSetup, TEXT("Tension_FinalOverFaceoff"), 3.4f,
           EC26CinematicCamera::OverShoulderBowler, EC26PresentationPriority::High, TEXT("FinalOverCard"),
           TEXT("Sting_FinalOver"), TEXT("Crowd_TenseMurmur"), EC26Action::Ready, EC26Action::Ready, 60.f);

    AddDef(EC26PresentationEvent::FinalBallTensionSetup, TEXT("Tension_FinalBallCloseups"), 3.2f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::Critical, TEXT("FinalBallCard"),
           TEXT("Sting_Heartbeat"), TEXT("Crowd_DeadSilence"), EC26Action::Ready, EC26Action::Ready, 60.f);

    // 12. Match Winning & Post-Match (4 events)
    AddDef(EC26PresentationEvent::MatchWinningCelebration, TEXT("Win_TeamSprintHuddle"), 5.0f,
           EC26CinematicCamera::OrbitCelebration, EC26PresentationPriority::Critical, TEXT("MatchResult"),
           TEXT("Sting_Victory"), TEXT("Crowd_Thunderous"), EC26Action::Celebrate, EC26Action::Celebrate, 300.f);

    AddDef(EC26PresentationEvent::LosingTeamReaction, TEXT("Lose_CaptainHandsOnHead"), 3.2f,
           EC26CinematicCamera::CloseUpFace, EC26PresentationPriority::High, TEXT("MatchResult"),
           TEXT(""), TEXT("Crowd_Applause"), EC26Action::Disappointed, EC26Action::Disappointed, 300.f);

    AddDef(EC26PresentationEvent::PostMatchHandshakes, TEXT("Post_OpponentsHandshakeLine"), 4.0f,
           EC26CinematicCamera::PitchTrackWalking, EC26PresentationPriority::High, TEXT("MatchResult"),
           TEXT("Sting_Presentation"), TEXT("Crowd_Applause"), EC26Action::Handshake, EC26Action::Handshake, 300.f);

    AddDef(EC26PresentationEvent::PlayerOfTheMatchPresentation, TEXT("POTM_TrophyHandover"), 4.2f,
           EC26CinematicCamera::TwoShot, EC26PresentationPriority::Critical, TEXT("PlayerOfMatch"),
           TEXT("Sting_Awards"), TEXT("Crowd_MassiveCheer"), EC26Action::BatRaise, EC26Action::Handshake, 300.f);

    // 13. Weather & DRS (2 events)
    AddDef(EC26PresentationEvent::RainDelayWalkoff, TEXT("Rain_CoversRushedOut"), 3.6f,
           EC26CinematicCamera::StadiumWide, EC26PresentationPriority::High, TEXT("RainDelay"),
           TEXT("Sting_Rain"), TEXT("Crowd_Disappointed"), EC26Action::Ready, EC26Action::Ready, 120.f);

    AddDef(EC26PresentationEvent::DRSUmpireSignal, TEXT("DRS_TVBoxUmpireSignal"), 3.0f,
           EC26CinematicCamera::UmpirePOV, EC26PresentationPriority::High, TEXT("DRSReview"),
           TEXT("Sting_DRS"), TEXT("Crowd_Murmur"), EC26Action::Ready, EC26Action::Ready, 45.f);
}

int32 UC26PresentationDirector::GetVariantCount(EC26PresentationEvent Event) const
{
    const auto* Found = SceneLibrary.Find(Event);
    return Found ? Found->Num() : 0;
}

float UC26PresentationDirector::CalculateMatchPressure(int32 TargetRuns, int32 CurrentRuns, int32 BallsRemaining, int32 WicketsDown) const
{
    if (TargetRuns <= 0)
    {
        // 1st innings: pressure based on remaining balls and wickets lost
        const float WicketPressure = FMath::Clamp(float(WicketsDown) / 10.f, 0.f, 1.f) * 0.4f;
        const float DeathOverPressure = (BallsRemaining <= 12 && BallsRemaining > 0) ? 0.35f : 0.1f;
        return FMath::Clamp(WicketPressure + DeathOverPressure, 0.f, 0.85f);
    }

    const int32 RunsNeeded = TargetRuns - CurrentRuns;
    if (RunsNeeded <= 0) return 1.0f; // Target achieved

    if (BallsRemaining <= 0) return 1.0f;

    const float RequiredRate = (float(RunsNeeded) / float(BallsRemaining)) * 6.f;
    float BasePressure = 0.2f;

    if (RequiredRate > 15.f) BasePressure = 0.90f;
    else if (RequiredRate > 12.f) BasePressure = 0.75f;
    else if (RequiredRate > 9.f) BasePressure = 0.55f;
    else if (RequiredRate > 6.f) BasePressure = 0.35f;

    // Death balls amplify tension
    if (BallsRemaining <= 6)
    {
        BasePressure = FMath::Max(BasePressure, 0.85f);
        if (RunsNeeded <= 6) BasePressure = 1.0f; // Match-deciding ball
    }
    else if (BallsRemaining <= 12)
    {
        BasePressure = FMath::Max(BasePressure, 0.70f);
    }

    // Wickets in hand factor
    if (WicketsDown >= 8) BasePressure = FMath::Min(1.0f, BasePressure + 0.15f);

    return FMath::Clamp(BasePressure, 0.f, 1.f);
}

bool UC26PresentationDirector::EvaluateEligibility(const FC26PresentationRequest& Request) const
{
    if (Request.Event == EC26PresentationEvent::None)
        return false;

    // Pacing filter
    if (PacingMode == EC26PresentationPacing::Quick)
    {
        // Quick mode only plays High and Critical priority events
        if (Request.Priority != EC26PresentationPriority::High &&
            Request.Priority != EC26PresentationPriority::Critical)
        {
            return false;
        }
    }
    else if (PacingMode == EC26PresentationPacing::Balanced)
    {
        // Balanced filters out 50% of Low priority events to keep match moving briskly
        if (Request.Priority == EC26PresentationPriority::Low)
        {
            if (FMath::FRand() > 0.50f)
            {
                return false;
            }
        }
    }

    return true;
}

FC26PresentationSceneDefinition UC26PresentationDirector::SelectSceneDefinition(const FC26PresentationRequest& Request)
{
    const auto* Variants = SceneLibrary.Find(Request.Event);
    if (!Variants || Variants->IsEmpty())
    {
        // Fallback default definition
        FC26PresentationSceneDefinition Fallback;
        Fallback.Event = Request.Event;
        Fallback.VariantId = FName(TEXT("Default_Fallback"));
        Fallback.Duration = 2.5f;
        Fallback.CameraAngle = EC26CinematicCamera::TwoShot;
        Fallback.Priority = Request.Priority;
        Fallback.bCanBeSkipped = true;
        return Fallback;
    }

    const UWorld* World = GetWorld();
    const float CurrentWorldTime = World ? World->GetTimeSeconds() : 0.f;

    float BestScore = -1.f;
    int32 BestIndex = 0;

    for (int32 I = 0; I < Variants->Num(); ++I)
    {
        const auto& Def = (*Variants)[I];
        float Score = 100.f;

        // Check recent history for cooldown and anti-repetition penalty
        for (const auto& Rec : RecentScenes)
        {
            if (Rec.VariantId == Def.VariantId)
            {
                const float Elapsed = CurrentWorldTime - Rec.Timestamp;
                if (Elapsed < Def.CooldownSeconds)
                {
                    Score *= 0.1f; // Heavy cooldown penalty
                }
                else if (Elapsed < Def.CooldownSeconds * 2.f)
                {
                    Score *= 0.6f; // Moderate recency penalty
                }
            }
        }

        // Add slight random jitter to prevent deterministic cycles among equal scores
        Score += FMath::FRandRange(0.f, 15.f);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = I;
        }
    }

    FC26PresentationSceneDefinition Selected = (*Variants)[BestIndex];

    // Apply pacing duration tuning
    if (PacingMode == EC26PresentationPacing::Quick)
    {
        Selected.Duration = FMath::Min(Selected.Duration, 2.5f);
    }
    else if (PacingMode == EC26PresentationPacing::Balanced)
    {
        Selected.Duration = Selected.Duration * 0.88f; // Brisk 12% trim
    }

    return Selected;
}

bool UC26PresentationDirector::RequestPresentation(const FC26PresentationRequest& Request)
{
    // Presentation scenes disabled per user request: replays only
    return false;
}

void UC26PresentationDirector::ApplySceneParticipants(const FC26PresentationSceneDefinition& Def, const FC26PresentationRequest& Req)
{
    RestoredParticipants.Empty();

    auto SaveAndApply = [this](AC26Athlete* Athlete, EC26Action Action)
    {
        if (!Athlete) return;
        FParticipantRestoreState State;
        State.Athlete = Athlete;
        State.OriginalTransform = Athlete->GetActorTransform();
        State.OriginalAction = Athlete->Action;
        RestoredParticipants.Add(State);

        Athlete->SetAction(Action, true);
    };

    SaveAndApply(Req.Participant1.Get(), Def.Participant1Action);
    SaveAndApply(Req.Participant2.Get(), Def.Participant2Action);

    // Special scene actor setups
    if (Req.Event == EC26PresentationEvent::TossCoinFlip)
    {
        bTossCoinActive = true;
        TossCoinTimer = 0.f;
        if (Req.Participant1 != nullptr)
        {
            TossCoinStart = Req.Participant1->GetActorLocation() + FVector(35.f, 12.f, 120.f);
            TossCoinApex = TossCoinStart + FVector(10.f, 0.f, 150.f);
            TossCoinLanding = TossCoinStart + FVector(25.f, 0.f, -115.f); // Land on pitch turf
        }
        else
        {
            TossCoinStart = FVector(0.f, 0.f, 120.f);
            TossCoinApex = FVector(0.f, 0.f, 270.f);
            TossCoinLanding = FVector(0.f, 0.f, 5.f);
        }
    }
    else
    {
        bTossCoinActive = false;
    }
}

void UC26PresentationDirector::RestoreParticipantPoses()
{
    for (const auto& State : RestoredParticipants)
    {
        if (State.Athlete.IsValid())
        {
            AC26Athlete* Athlete = State.Athlete.Get();
            Athlete->SetActorTransform(State.OriginalTransform);
            Athlete->SetAction(EC26Action::Ready, true);
        }
    }
    RestoredParticipants.Empty();
    bTossCoinActive = false;
}

void UC26PresentationDirector::RecordScenePlayed(EC26PresentationEvent Event, FName VariantId)
{
    FC26RecentSceneRecord Rec;
    Rec.Event = Event;
    Rec.VariantId = VariantId;
    Rec.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    RecentScenes.Add(Rec);

    // Limit history to 20 records
    while (RecentScenes.Num() > 20)
    {
        RecentScenes.RemoveAt(0);
    }
}

bool UC26PresentationDirector::GetTossCoinState(FVector& OutPosition, FRotator& OutRotation) const
{
    if (!bTossCoinActive) return false;

    const float TotalTime = 1.8f;
    const float T = FMath::Clamp(TossCoinTimer / TotalTime, 0.f, 1.f);

    // Parabolic arc
    const float Height = 4.f * 160.f * T * (1.f - T);
    OutPosition = FMath::Lerp(TossCoinStart, TossCoinLanding, T);
    OutPosition.Z += Height;

    // Spinning coin
    OutRotation = FRotator(TossCoinTimer * 1440.f, TossCoinTimer * 360.f, 0.f);
    return true;
}

void UC26PresentationDirector::DirectActiveCamera(float Dt)
{
    AC26MatchGameMode* GM = GetGameMode();
    if (!GM || !GM->Director) return;

    FVector Focus1 = FVector::ZeroVector;
    FVector Focus2 = FVector::ZeroVector;

    if (ActiveQueueItem.Request.Participant1 != nullptr)
    {
        Focus1 = ActiveQueueItem.Request.Participant1->GetActorLocation();
    }
    if (ActiveQueueItem.Request.Participant2 != nullptr)
    {
        Focus2 = ActiveQueueItem.Request.Participant2->GetActorLocation();
    }
    else
    {
        Focus2 = Focus1 + FVector(0, 250.f, 0);
    }

    const float Progress = GetSceneProgress();
    const bool bCut = (SceneTime <= 0.001f);

    GM->Director->DirectPresentation(ActiveSceneDef.CameraAngle, Focus1, Focus2, Progress, Dt, bCut);
}

void UC26PresentationDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bPresentationActive) return;

    SceneTime += DeltaTime;
    if (bTossCoinActive)
    {
        TossCoinTimer += DeltaTime;
    }

    // Drive presentation camera lens
    DirectActiveCamera(DeltaTime);

    // Check completion
    if (SceneTime >= ActiveSceneDef.Duration)
    {
        FinishCurrentScene();
    }
}

void UC26PresentationDirector::FinishCurrentScene()
{
    RestoreParticipantPoses();

    if (PresentationQueue.Num() > 0)
    {
        // Play next queued scene
        ActiveQueueItem = PresentationQueue[0];
        PresentationQueue.RemoveAt(0);

        ActiveSceneDef = ActiveQueueItem.ResolvedScene;
        ActiveEvent = ActiveQueueItem.Request.Event;
        SceneTime = 0.f;
        bPresentationActive = true;

        ApplySceneParticipants(ActiveSceneDef, ActiveQueueItem.Request);
        RecordScenePlayed(ActiveEvent, ActiveSceneDef.VariantId);
        DirectActiveCamera(0.f);
    }
    else
    {
        bPresentationActive = false;
        ActiveEvent = EC26PresentationEvent::None;

        // Clean return to gameplay authority
        AC26MatchGameMode* GM = GetGameMode();
        if (GM)
        {
            GM->OnPresentationCompleted();
        }
    }
}

void UC26PresentationDirector::SkipCurrentScene()
{
    if (!bPresentationActive) return;

    // Log skip for analytics
    UE_LOG(LogC26, Log, TEXT("C26_PRESENTATION skipped event=%d variant=%s time=%.2f"),
           int32(ActiveEvent), *ActiveSceneDef.VariantId.ToString(), SceneTime);

    // Safe immediate restore
    RestoreParticipantPoses();
    ClearQueue();
    bPresentationActive = false;
    ActiveEvent = EC26PresentationEvent::None;

    AC26MatchGameMode* GM = GetGameMode();
    if (GM)
    {
        GM->OnPresentationCompleted();
    }
}

void UC26PresentationDirector::ClearQueue()
{
    PresentationQueue.Empty();
}

void UC26PresentationDirector::TriggerDebugScene(EC26PresentationEvent Event, AC26Athlete* P1, AC26Athlete* P2)
{
    AC26MatchGameMode* GM = GetGameMode();
    if (!GM) return;

    // Use default actors if none supplied
    if (!P1 && GM->Athletes.Num() > 11)
    {
        P1 = GM->Athletes[11]; // Striker batter
    }
    if (!P2 && GM->Athletes.Num() > 0)
    {
        P2 = GM->Athletes[0]; // Bowler
    }

    FC26PresentationRequest Req;
    Req.Event = Event;
    Req.Participant1 = P1;
    Req.Participant2 = P2;
    Req.Priority = EC26PresentationPriority::Critical; // Forced debug trigger gets highest priority
    Req.Context = TEXT("ManualDebugTrigger");

    RequestPresentation(Req);
}
