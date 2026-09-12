#pragma once

#include "CoreMinimal.h"
#include "C26Types.h"
#include "Core/C26Rules.h"
#include "C26PresentationTypes.generated.h"

class AC26Athlete;

/** High-level broadcast presentation event types across pre-match, gameplay pauses, milestones and post-match */
UENUM(BlueprintType)
enum class EC26PresentationEvent : uint8
{
    None = 0,
    PreMatch,
    TossStart,
    TossCeremonyWalkout,
    TossCoinFlip,
    TossResult,
    CaptainDecision,
    TossDecisionBat,
    TossDecisionBowl,
    TeamWalkout,
    TeamWalkoutAnthem,
    OpeningBatters,
    OpeningBattersWalkIn,
    OpeningBowler,
    OpeningBowlerRunupPrep,
    PreDeliveryDiscussion,
    BatterDiscussion,
    BatterMidPitchDiscussion,
    BatterBoundaryMeeting,
    BowlerCaptainDiscussion,
    BowlerKeeperDiscussion,
    BowlerKeeperFieldAdjust,
    BowlerFielderCongratulate,
    FieldAdjustment,
    BoundaryReaction,
    BowlerFrustrationBoundary,
    BowlerFrustrationDot,
    SixReaction,
    CloseCallReaction,
    BatterCloseCallReview,
    DotPressureReaction,
    BowlerAppealingDesperate,
    BatterDotDisappointment,
    BatterPlayAndMissReaction,
    BatterCloseRunoutSurvival,
    WicketCelebration,
    WicketCelebrationBowled,
    WicketCelebrationCaught,
    WicketCelebrationLBW,
    WicketCelebrationRunOut,
    WicketDisappointmentBatterWalk,
    CaughtCelebration,
    BowledCelebration,
    LBWCelebration,
    RunOutCelebration,
    StumpingCelebration,
    BatterDismissalReaction,
    NewBatterEntrance,
    NewBatterEntry,
    FiftyCelebration,
    CenturyCelebration,
    PartnershipMilestone,
    EndOfOver,
    EndOfOverSummaryCard,
    BatterEndOverDiscussion,
    BowlingSpellChange,
    InningsBreak,
    InningsBreakTransition,
    ChasePressure,
    FinalOver,
    FinalOverTensionSetup,
    FinalBall,
    FinalBallTensionSetup,
    WinningMoment,
    MatchWinCelebration,
    MatchWinningCelebration,
    MatchLossReaction,
    LosingTeamReaction,
    PostMatchHandshake,
    PostMatchHandshakes,
    PlayerOfMatch,
    PlayerOfTheMatchPresentation,
    TrophyCelebration,
    RainDelayWalkoff,
    DRSUmpireSignal
};

/** Presentation priority tiers for preempting or suppressing lower-priority micro-scenes */
UENUM(BlueprintType)
enum class EC26PresentationPriority : uint8
{
    Low,
    Medium,
    High,
    Critical
};

/** User presentation pacing preference */
UENUM(BlueprintType)
enum class EC26PresentationPacing : uint8
{
    Full,       // Complete broadcast presentation with routine micro-scenes
    Balanced,   // Strategic presentation: major moments + occasional interactions
    Quick       // High-tempo mobile: only critical dismissals, 50/100, innings break & win
};

/** Reusable cinematic camera lenses and tracking styles */
UENUM(BlueprintType)
enum class EC26CinematicCamera : uint8
{
    CloseUpFace,
    MediumPlayer,
    TwoShot,
    TeamWide,
    LowAngleHero,
    LowAngleDramatic,
    OverShoulder,
    OverShoulderBatter,
    OverShoulderBowler,
    PitchWide,
    StadiumWide,
    CrowdCutaway,
    TrackingWalk,
    PitchTrackWalking,
    OrbitCelebration,
    HighAngleToss,
    UmpirePOV,
    DugoutReaction
};

/** Authoritative context payload passed from gameplay to Presentation Director */
USTRUCT(BlueprintType)
struct CRICKETGAME_API FC26PresentationRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent Event = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent EventType = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationPriority Priority = EC26PresentationPriority::Medium;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> Participant1 = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> Participant2 = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> Striker = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> NonStriker = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> Bowler = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> WicketKeeper = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> Captain = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> RelevantFielder = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> DismissedBatter = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> NewBatter = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Score = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Wickets = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Over = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Ball = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RunsRequired = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BallsRemaining = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Target = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BatterRuns = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BatterBalls = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BowlerWickets = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BowlerRuns = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Partnership = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 MatchResult = 0; // C26::Result cast

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 DismissalType = 0; // C26::Dismissal cast

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 BoundaryType = 0; // C26::Boundary cast

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PressureLevel = 0.f; // 0..1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MatchImportance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFinalOver = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFinalBall = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsMilestone = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsWinningMoment = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SecondaryPlayerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CustomSubtitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TeamName;
};

/** Data definition for an individual presentation scene variant */
USTRUCT(BlueprintType)
struct CRICKETGAME_API FC26PresentationSceneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName VariantId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent Event = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent EventType = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationPriority Priority = EC26PresentationPriority::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26CinematicCamera CameraAngle = EC26CinematicCamera::MediumPlayer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26CinematicCamera CameraStyle = EC26CinematicCamera::MediumPlayer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCanBeSkipped = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSkippable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OverlayType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AudioSting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CrowdRoarType;

    FC26PresentationSceneDefinition() = default;
    FC26PresentationSceneDefinition(const FC26PresentationSceneDefinition&) = default;
    FC26PresentationSceneDefinition& operator=(const FC26PresentationSceneDefinition&) = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26Action Participant1Action = EC26Action::Ready;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26Action Participant2Action = EC26Action::Ready;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CooldownSeconds = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinPressure = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxPressure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CooldownDeliveries = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecentUsePenalty = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DefaultTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DefaultSubtitle;
};

/** Anti-repetition audit entry recording recent scene execution */
USTRUCT(BlueprintType)
struct CRICKETGAME_API FC26RecentSceneRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent Event = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EC26PresentationEvent EventType = EC26PresentationEvent::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName VariantId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DeliveryId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.f;
};

/** Active or queued item in the Presentation Director queue */
USTRUCT(BlueprintType)
struct CRICKETGAME_API FC26PresentationQueueItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FC26PresentationSceneDefinition SceneDef;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FC26PresentationSceneDefinition ResolvedScene;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnqueuedTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FC26PresentationRequest Request;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElapsedTime = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalDuration = 3.0f;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> PrimaryActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AC26Athlete> SecondaryActor = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AC26Athlete>> GatherActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector FocusLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector SecondaryFocusLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector CoinLocation = FVector::ZeroVector;
};
