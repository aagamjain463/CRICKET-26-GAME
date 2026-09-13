#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26CommentaryTypes.h"
#include "C26Commentary.h"
#include "C26Audio.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
struct FC26CommentaryRowSrc;

USTRUCT()
struct FC26LoadedLine
{
    GENERATED_BODY()
    UPROPERTY() int32 Row = -1;
    UPROPERTY() TObjectPtr<USoundBase> Sound = nullptr;
};

// Match audio director. Conceptually three directors in one component:
//   - CommentaryManager (event-driven VO queue, priority, cooldowns)
//   - CrowdDirector     (persistent bed + tension layer + reaction overlays)
//   - CricketSFXDirector (hero transients, spatialized as broadcast field mics)
// Gameplay sends context/events; this system never writes match truth.
UCLASS(ClassGroup=(Cricket))
class CRICKETGAME_API UC26Audio : public UActorComponent
{
    GENERATED_BODY()
public:
    UC26Audio();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ---- legacy API (kept for existing call sites) ----
    virtual void Initialize();
    virtual void Cue(FName Name, float Volume = 1.f);
    void SetTension(float Amount, float Volume);
    virtual void Reset();
    float Master = .75f;

    // ---- mix buses (0..1, each multiplied with Master) ----
    float CommentaryVol = .95f;
    float CrowdVol = .85f;
    float SFXVol = .9f;
    float MusicVol = .5f;
    float UIVol = .8f;
    void SyncVolumesFromSettings();

    // ---- spatial hero transients (broadcast field-mic treatment) ----
    virtual void CueAt(FName Name, const FVector& At, float Volume = 1.f);

    // ---- commentary director bridge ----
    virtual void PlayCommentarySound(USoundBase* Sound, const FString& SubtitleText, float Duration);
    bool IsCommentaryPlaying() const;
    void StopCommentary();

    // ---- commentary event API (called exactly once per match event) ----
    virtual void NotifyMatchStart();
    virtual void NotifyPreBall(const FC26CommentaryContext& Ctx);
    virtual void NotifyFinalBallPre();
    virtual void NotifyDelivery(const FC26CommentaryContext& Ctx);
    virtual void NotifyResult(const FC26CommentaryContext& Ctx);
    virtual void NotifyWicket(const FC26CommentaryContext& Ctx);
    virtual void NotifyInningsBreak();
    virtual void NotifyChaseStart();
    virtual void NotifyMatchResult(bool bPlayerWon, bool bTie);
    void NoteBallCompleted(int32 RunsScored, bool bWicket, bool bBoundary);

    // ---- debug ----
    void TestCommentary(FName Category);
    void DumpState() const;
    int32 GetConsecutiveBoundaries() const { return ConsecutiveBoundaries; }
    int32 GetConsecutiveDots() const { return ConsecutiveDots; }
    bool GetPrevWasWicket() const { return bPrevWasWicket; }

    // ---- subtitles (read by the HUD) ----
    UPROPERTY() FString ActiveSubtitle;
    float SubtitleUntil = -1.f;

protected:
    struct FQueuedLine
    {
        int32 Row = -1;
        float StartAt = 0.f;
        uint8 Priority = 0;
        uint32 Ball = 0;
    };
    bool bInitialized = false;
    UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> Sounds;
    UPROPERTY() TArray<FC26LoadedLine> VoiceLines;
    UPROPERTY() TObjectPtr<UAudioComponent> Ambience = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> TensionLayer = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> CommentaryVoice = nullptr;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Channels;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> FieldPool;
    UPROPERTY() TObjectPtr<USoundAttenuation> FieldAttenuation = nullptr;
    int NextChannel = 0;
    int NextField = 0;

    // Commentary state
    TArray<FQueuedLine> Queue;
    int32 ActiveRow = -1;
    uint8 ActivePriority = 0;
    uint32 ActiveBall = 0;
    float GapUntil = 0.f;
    float LastPrimaryEnd = -1.f;
    FString LastPrimaryFamily;
    uint8 LastPrimaryPriority = 0;
    TMap<int32, uint32> LastUsedBall;
    uint32 CurrentBall = 1;
    int32 ConsecutiveBoundaries = 0;
    int32 ConsecutiveDots = 0;
    bool bPrevWasWicket = false;

    // Crowd state
    EC26CrowdState CrowdState = EC26CrowdState::Calm;
    float CrowdIntensity = .22f;
    float CrowdTension = .15f;
    float DuckFactor = 1.f;
    float VolumeSyncClock = 0.f;
    TMap<FName, double> LastCueTime;

    float Now() const;
    int32 PickLine(const TArray<const TCHAR*>& Categories, uint32 BallId, bool bFollowOnly, int32 VoiceFilter = -1);
    int32 PickLineSingle(const TCHAR* Category, uint32 BallId, bool bFollowOnly, int32 VoiceFilter = -1);
    void QueueLine(int32 RowIdx, uint32 BallId);
    void PlayLine(int32 RowIdx, uint32 BallId);
    void OnPrimaryFinished();
    float BusFor(FName Name) const;
    void SetCrowd(EC26CrowdState State, float Intensity);
    void PumpQueue(float CurrentTime);
};
