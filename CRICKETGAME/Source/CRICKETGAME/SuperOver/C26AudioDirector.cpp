#include "C26AudioDirector.h"
#include "C26CommentaryDirector.h"
#include "C26CrowdDirector.h"
#include "C26StadiumAmbienceComponent.h"
#include "C26Types.h"
#include "GameFramework/Actor.h"

UC26AudioDirector::UC26AudioDirector()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UC26AudioDirector::AttachSubDirectors(UC26CommentaryDirector* InCommentaryDirector, UC26CrowdDirector* InCrowdDirector, UC26StadiumAmbienceComponent* InStadiumAmbience)
{
    if (InCommentaryDirector) CommentaryDirector = InCommentaryDirector;
    if (InCrowdDirector) CrowdDirector = InCrowdDirector;
    if (InStadiumAmbience) StadiumAmbience = InStadiumAmbience;

    if (CrowdDirector)
    {
        CrowdDirector->Initialize(this);
    }
    if (StadiumAmbience)
    {
        StadiumAmbience->Initialize();
    }
    bBroadcastDirectorReady = true;
    UE_LOG(LogC26, Display, TEXT("C26_AUDIO_DIRECTOR: Attached sub-directors explicitly."));
}

void UC26AudioDirector::Initialize()
{
    Super::Initialize();

    if (!CrowdDirector)
    {
        CrowdDirector = NewObject<UC26CrowdDirector>(this, TEXT("CrowdDirector"));
        if (CrowdDirector)
        {
            CrowdDirector->RegisterComponent();
        }
    }
    if (!StadiumAmbience)
    {
        StadiumAmbience = NewObject<UC26StadiumAmbienceComponent>(this, TEXT("StadiumAmbience"));
        if (StadiumAmbience)
        {
            StadiumAmbience->RegisterComponent();
        }
    }

    if (CrowdDirector)
    {
        CrowdDirector->Initialize(this);
    }
    if (StadiumAmbience)
    {
        StadiumAmbience->Initialize();
    }

    if (!CommentaryDirector && GetOwner())
    {
        CommentaryDirector = GetOwner()->FindComponentByClass<UC26CommentaryDirector>();
    }

    bBroadcastDirectorReady = true;
    UE_LOG(LogC26, Display, TEXT("C26_AUDIO_DIRECTOR: Master broadcast director initialized cleanly."));
}

void UC26AudioDirector::Reset()
{
    Super::Reset();
    if (CrowdDirector)
    {
        CrowdDirector->Reset();
    }
}

void UC26AudioDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CrowdDirector)
    {
        const bool bIsSpeaking = IsCommentaryPlaying();
        CrowdDirector->SetCommentarySpeaking(bIsSpeaking);
    }
}

void UC26AudioDirector::Cue(FName Name, float Volume)
{
    // Micro-duck crowd transient when hero bat contact fires
    if (Name == TEXT("bat_sweet_spot") || Name == TEXT("bat_sweet") || Name == TEXT("bat_edge") || Name == TEXT("wicket_stump_smash"))
    {
        if (CrowdDirector)
        {
            CrowdDirector->TriggerBatContactDucking(true);
        }
    }

    Super::Cue(Name, Volume);
}

void UC26AudioDirector::CueAt(FName Name, const FVector& At, float Volume)
{
    if (Name == TEXT("bat_sweet_spot") || Name == TEXT("bat_sweet") || Name == TEXT("bat_edge") || Name == TEXT("wicket_stump_smash"))
    {
        if (CrowdDirector)
        {
            CrowdDirector->TriggerBatContactDucking(true);
        }
    }

    Super::CueAt(Name, At, Volume);
}

void UC26AudioDirector::PlayCommentarySound(USoundBase* Sound, const FString& SubtitleText, float Duration)
{
    Super::PlayCommentarySound(Sound, SubtitleText, Duration);

    if (CrowdDirector)
    {
        CrowdDirector->SetCommentarySpeaking(true);
    }
}

void UC26AudioDirector::NotifyMatchStart()
{
    if (CrowdDirector)
    {
        CrowdDirector->OnMatchStart();
    }
}

void UC26AudioDirector::NotifyPreBall(const FC26CommentaryContext& Ctx)
{
    if (CrowdDirector)
    {
        CrowdDirector->OnPreBall(Ctx.bPressure ? 0.75f : 0.20f, Ctx.bFinalBall);
    }
}

void UC26AudioDirector::NotifyDelivery(const FC26CommentaryContext& Ctx)
{
    // Legacy VO queueing bypassed; commentary managed by UC26CommentaryDirector
}

void UC26AudioDirector::NotifyResult(const FC26CommentaryContext& Ctx)
{
    if (CrowdDirector)
    {
        if (Ctx.bSix || Ctx.bFour)
        {
            CrowdDirector->OnBoundary(Ctx.bSix, true);
        }
        else if (Ctx.RunsScored == 0)
        {
            CrowdDirector->OnDotBall(Ctx.bPressure ? 0.70f : 0.30f, ConsecutiveDots);
        }
    }
}

void UC26AudioDirector::NotifyWicket(const FC26CommentaryContext& Ctx)
{
    if (CrowdDirector)
    {
        CrowdDirector->OnWicket(true);
    }
}

void UC26AudioDirector::NotifyInningsBreak()
{
    if (CrowdDirector)
    {
        CrowdDirector->OnInningsBreak();
    }
}

void UC26AudioDirector::NotifyChaseStart()
{
    if (CrowdDirector)
    {
        CrowdDirector->OnMatchStart();
    }
}

void UC26AudioDirector::NotifyMatchResult(bool bPlayerWon, bool bTie)
{
    if (CrowdDirector)
    {
        CrowdDirector->OnMatchEnd(bPlayerWon, bTie);
    }
}

void UC26AudioDirector::BroadcastMatchStart()
{
    NotifyMatchStart();
    if (CommentaryDirector)
    {
        CommentaryDirector->OnMatchStart();
    }
}

void UC26AudioDirector::BroadcastPreBall(float Pressure, bool bIsFinalBall)
{
    if (CrowdDirector)
    {
        CrowdDirector->OnPreBall(Pressure, bIsFinalBall);
    }
}

void UC26AudioDirector::BroadcastBallCompleted(int32 RunsScored, bool bIsFour, bool bIsSix, bool bIsWicket, uint8 WicketType, float Pressure, bool bHomeTeamBatting)
{
    if (CrowdDirector)
    {
        if (bIsWicket)
        {
            CrowdDirector->OnWicket(bHomeTeamBatting);
        }
        else if (bIsSix || bIsFour)
        {
            CrowdDirector->OnBoundary(bIsSix, bHomeTeamBatting);
        }
        else if (RunsScored == 0)
        {
            CrowdDirector->OnDotBall(Pressure, 1);
        }
    }
}

void UC26AudioDirector::BroadcastInningsBreak()
{
    NotifyInningsBreak();
}

void UC26AudioDirector::BroadcastChaseStart()
{
    NotifyChaseStart();
}

void UC26AudioDirector::BroadcastMatchEnd(bool bHomeTeamWon, bool bTie)
{
    NotifyMatchResult(bHomeTeamWon, bTie);
}
