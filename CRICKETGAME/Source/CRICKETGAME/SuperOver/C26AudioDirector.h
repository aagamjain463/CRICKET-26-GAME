#pragma once

#include "CoreMinimal.h"
#include "C26Audio.h"
#include "C26AudioDirector.generated.h"

class UC26CommentaryDirector;
class UC26CrowdDirector;
class UC26StadiumAmbienceComponent;

/**
 * Master Broadcast Audio Director for CRICKET 26.
 * Single point of authority coordinating commentary scheduling,
 * multi-layered crowd simulation, spatial hero Foley, and broadcast mix ducking.
 */
UCLASS(ClassGroup=(Cricket))
class CRICKETGAME_API UC26AudioDirector : public UC26Audio
{
    GENERATED_BODY()

public:
    UC26AudioDirector();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void Initialize() override;
    virtual void Reset() override;

    void AttachSubDirectors(UC26CommentaryDirector* InCommentaryDirector, UC26CrowdDirector* InCrowdDirector, UC26StadiumAmbienceComponent* InStadiumAmbience);

    // Overridden audio cues for hero transients with micro-ducking
    virtual void Cue(FName Name, float Volume = 1.f) override;
    virtual void CueAt(FName Name, const FVector& At, float Volume = 1.f) override;

    // Commentary bridge with crowd sidechain ducking
    virtual void PlayCommentarySound(USoundBase* Sound, const FString& SubtitleText, float Duration) override;

    // Match event overrides routing to broadcast directors
    virtual void NotifyMatchStart() override;
    virtual void NotifyPreBall(const FC26CommentaryContext& Ctx) override;
    virtual void NotifyDelivery(const FC26CommentaryContext& Ctx) override;
    virtual void NotifyResult(const FC26CommentaryContext& Ctx) override;
    virtual void NotifyWicket(const FC26CommentaryContext& Ctx) override;
    virtual void NotifyInningsBreak() override;
    virtual void NotifyChaseStart() override;
    virtual void NotifyMatchResult(bool bPlayerWon, bool bTie) override;

    // High-level broadcast coordination methods
    void BroadcastMatchStart();
    void BroadcastPreBall(float Pressure, bool bIsFinalBall);
    void BroadcastBallCompleted(int32 RunsScored, bool bIsFour, bool bIsSix, bool bIsWicket, uint8 WicketType, float Pressure, bool bHomeTeamBatting);
    void BroadcastInningsBreak();
    void BroadcastChaseStart();
    void BroadcastMatchEnd(bool bPlayerWon, bool bTie);

    UPROPERTY() TObjectPtr<UC26CrowdDirector> CrowdDirector = nullptr;
    UPROPERTY() TObjectPtr<UC26StadiumAmbienceComponent> StadiumAmbience = nullptr;
    UPROPERTY() TObjectPtr<UC26CommentaryDirector> CommentaryDirector = nullptr;

private:
    bool bBroadcastDirectorReady = false;
};
