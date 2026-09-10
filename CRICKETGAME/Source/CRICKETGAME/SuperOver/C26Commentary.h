#pragma once
#include "CoreMinimal.h"
#include "C26Commentary.generated.h"

// Compact source row compiled in from C26CommentaryData.inc (generated from
// Tools/CommentaryScript.py, the single source of truth for VO content).
struct FC26CommentaryRowSrc
{
    const TCHAR* Id;
    const TCHAR* Category;
    const TCHAR* File;
    const TCHAR* Text;
    uint8 Commentator; // 0 = play-by-play (A), 1 = analyst (B)
    uint8 Priority;    // match result 100 > wicket 90 > final 85 > six 80 ...
    uint8 Cooldown;    // minimum balls before this exact line may repeat
    float Delay;       // seconds after the event before speech starts
    uint8 Weight;      // relative selection weight inside its category
    bool Follow;       // analyst follow-up: only queued after a primary call
};

// Everything the commentary selection is allowed to know. Built by the
// GameMode from authoritative match state at the moment an event fires.
// Commentary OBSERVES this; it never writes back into gameplay.
USTRUCT(BlueprintType)
struct FC26CommentaryContext
{
    GENERATED_BODY()
    UPROPERTY() int32 BallsRemaining = 6;
    UPROPERTY() int32 RunsRequired = 0;
    UPROPERTY() int32 Target = 0;
    UPROPERTY() int32 Score = 0;
    UPROPERTY() int32 Wickets = 0;
    UPROPERTY() int32 InningsNumber = 0;
    UPROPERTY() uint8 DeliveryType = 0;
    UPROPERTY() uint8 TimingResult = 5;
    UPROPERTY() float ContactQuality = 0.f;
    UPROPERTY() int32 RunsScored = 0;
    UPROPERTY() bool bFour = false;
    UPROPERTY() bool bSix = false;
    UPROPERTY() bool bWicket = false;
    UPROPERTY() uint8 WicketType = 0;
    UPROPERTY() bool bEdge = false;
    UPROPERTY() bool bMistimed = false;
    UPROPERTY() bool bBeaten = false;
    UPROPERTY() bool bFinalBall = false;
    UPROPERTY() bool bMatchWinning = false;
    UPROPERTY() bool bPressure = false;
    UPROPERTY() int32 ConsecutiveBoundaries = 0;
    UPROPERTY() int32 ConsecutiveDots = 0;
    UPROPERTY() uint32 DeliveryId = 0;
};

// Crowd director states. The bed never restarts between balls; only the
// target intensity and one-shot overlays change.
UENUM(BlueprintType)
enum class EC26CrowdState : uint8
{
    Calm, Anticipation, Excited, Boundary, Six, Wicket, Tense, Win, Loss
};
