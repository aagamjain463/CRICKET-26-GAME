#pragma once

#include "CoreMinimal.h"
#include "C26CommentaryTypes.generated.h"

/** High-level classification of cricket commentary events. */
UENUM(BlueprintType)
enum class ECommentaryEventType : uint8
{
    MatchStart,
    ChaseStart,
    InningsBreak,
    PreBall,
    DeliveryStruck,
    Dot,
    Single,
    Double,
    Triple,
    Four,
    Six,
    Wicket,
    Bowled,
    Caught,
    KeeperCatch,
    LBW,
    RunOut,
    Edge,
    Beaten,
    ShotMistimed,
    ShotDefended,
    ShotDriven,
    Appeal,
    DroppedCatch,
    Wide,
    NoBall,
    FreeHit,
    MilestoneFifty,
    MilestoneCentury,
    FinalOver,
    FinalBall,
    MatchWin,
    MatchLoss,
    MatchTie,
    AnalystFollowUp
};

/** Nuanced emotional delivery state for commentary. */
UENUM(BlueprintType)
enum class ECommentaryEmotion : uint8
{
    Calm,
    Neutral,
    Analytical,
    Appreciative,
    Amused,
    Surprised,
    Excited,
    VeryExcited,
    Tense,
    Dramatic,
    Shocked,
    Celebratory,
    Disappointed,
    Reflective
};

/** Commentator personality and broadcast function. */
UENUM(BlueprintType)
enum class ECommentatorRole : uint8
{
    Lead,    // Commentator A: play-by-play, high energy, reactive, boundaries, wickets, match finishes
    Analyst  // Commentator B: tactical, calm, observational, cricket wisdom, field placements, follow-ups
};

/** Operating mode for voice clip generation. */
UENUM(BlueprintType)
enum class ECommentaryMode : uint8
{
    OfflineOnly, // Pre-generated imported assets & local cache only. Zero network requests.
    Hybrid,      // Pre-generated for routine deliveries; dynamic ElevenLabs for high-pressure & match-winning moments.
    Dynamic      // Full dynamic ElevenLabs generation within match budget.
};

/** Diagnostic test scenarios for development & automated verification. */
UENUM(BlueprintType)
enum class ECommentaryScenario : uint8
{
    RoutineDot,
    RoutineSingle,
    NormalFour,
    NormalSix,
    HighPressureSix,
    WicketBowled,
    WicketCaught,
    FinalBallTense,
    MatchWinningSix
};

/**
 * Rich authoritative snapshot of match state and delivery outcome
 * emitted by gameplay systems to the Commentary Director.
 */
USTRUCT(BlueprintType)
struct FC26CommentaryEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) ECommentaryEventType EventType = ECommentaryEventType::PreBall;
    UPROPERTY(BlueprintReadWrite) FString BatterName = TEXT("BATTER");
    UPROPERTY(BlueprintReadWrite) FString BowlerName = TEXT("BOWLER");

    UPROPERTY(BlueprintReadWrite) int32 RunsScored = 0;
    UPROPERTY(BlueprintReadWrite) bool bIsBoundary = false;
    UPROPERTY(BlueprintReadWrite) bool bIsSix = false;
    UPROPERTY(BlueprintReadWrite) bool bIsWicket = false;
    UPROPERTY(BlueprintReadWrite) uint8 DismissalType = 0; // 0=None, 1=Bowled, 2=Caught, 3=RunOut, 4=Keeper, 5=LBW

    UPROPERTY(BlueprintReadWrite) FName ShotType = NAME_None;
    UPROPERTY(BlueprintReadWrite) uint8 DeliveryType = 0; // EC26Delivery
    UPROPERTY(BlueprintReadWrite) float DeliveryLine = 0.f;
    UPROPERTY(BlueprintReadWrite) float DeliveryLength = 460.f;
    UPROPERTY(BlueprintReadWrite) float TimingQuality = 0.f;
    UPROPERTY(BlueprintReadWrite) float ContactQuality = 0.f;

    UPROPERTY(BlueprintReadWrite) int32 CurrentScore = 0;
    UPROPERTY(BlueprintReadWrite) int32 Wickets = 0;
    UPROPERTY(BlueprintReadWrite) int32 InningsNumber = 0;
    UPROPERTY(BlueprintReadWrite) int32 CurrentOver = 0;
    UPROPERTY(BlueprintReadWrite) int32 BallNumber = 0;

    UPROPERTY(BlueprintReadWrite) int32 Target = 0;
    UPROPERTY(BlueprintReadWrite) int32 RunsRequired = 0;
    UPROPERTY(BlueprintReadWrite) int32 BallsRemaining = 6;
    UPROPERTY(BlueprintReadWrite) float RequiredRunRate = 0.f;
    UPROPERTY(BlueprintReadWrite) float CurrentRunRate = 0.f;

    UPROPERTY(BlueprintReadWrite) bool bIsFreeHit = false;
    UPROPERTY(BlueprintReadWrite) bool bIsSuperOver = true;
    UPROPERTY(BlueprintReadWrite) bool bIsFinalOver = false;
    UPROPERTY(BlueprintReadWrite) bool bIsFinalBall = false;
    UPROPERTY(BlueprintReadWrite) bool bIsMatchWinningEvent = false;

    UPROPERTY(BlueprintReadWrite) int32 BatterRuns = 0;
    UPROPERTY(BlueprintReadWrite) int32 BatterBalls = 0;
    UPROPERTY(BlueprintReadWrite) int32 BowlerWickets = 0;
    UPROPERTY(BlueprintReadWrite) int32 BowlerRuns = 0;

    UPROPERTY(BlueprintReadWrite) int32 ConsecutiveBoundaries = 0;
    UPROPERTY(BlueprintReadWrite) int32 ConsecutiveDots = 0;

    UPROPERTY(BlueprintReadWrite) float MatchPressure = 0.f; // 0.0 to 1.0
    UPROPERTY(BlueprintReadWrite) float CrowdIntensity = 0.22f;
    UPROPERTY(BlueprintReadWrite) int64 DeliveryId = 0;
};

/** Metadata definition for a data-driven commentary line. */
USTRUCT(BlueprintType)
struct FC26CommentaryLineDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FName LineId = NAME_None;
    UPROPERTY(BlueprintReadWrite) FName Category = NAME_None;
    UPROPERTY(BlueprintReadWrite) ECommentatorRole Role = ECommentatorRole::Lead;
    UPROPERTY(BlueprintReadWrite) ECommentaryEmotion Emotion = ECommentaryEmotion::Neutral;

    UPROPERTY(BlueprintReadWrite) float MinIntensity = 0.f;
    UPROPERTY(BlueprintReadWrite) float MaxIntensity = 1.f;
    UPROPERTY(BlueprintReadWrite) float MinPressure = 0.f;
    UPROPERTY(BlueprintReadWrite) float MaxPressure = 1.f;

    UPROPERTY(BlueprintReadWrite) uint8 Priority = 50;
    UPROPERTY(BlueprintReadWrite) float Delay = 0.4f;
    UPROPERTY(BlueprintReadWrite) uint8 Weight = 10;

    UPROPERTY(BlueprintReadWrite) bool bFollowUpOnly = false;
    UPROPERTY(BlueprintReadWrite) bool bMatchWinningOnly = false;
    UPROPERTY(BlueprintReadWrite) bool bFinalBallOnly = false;

    UPROPERTY(BlueprintReadWrite) FString Text;
    UPROPERTY(BlueprintReadWrite) FString ElevenLabsPrompt;
    UPROPERTY(BlueprintReadWrite) FName PreGenFile = NAME_None;
};
