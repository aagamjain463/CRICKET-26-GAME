#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26CommentaryTypes.h"
#include "C26CommentaryDirector.generated.h"

class UC26Audio;
class USoundBase;
class USoundWave;
class IHttpRequest;
class IHttpResponse;

/**
 * High-level broadcast commentary director for CRICKET 26.
 * Decides whether to commentate, which commentator speaks, emotional intensity,
 * line selection, ElevenLabs dynamic synthesis, disk caching, and queue priority.
 */
UCLASS(ClassGroup=(Cricket), meta=(BlueprintSpawnableComponent))
class CRICKETGAME_API UC26CommentaryDirector : public UActorComponent
{
    GENERATED_BODY()

public:
    UC26CommentaryDirector();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ---- Configuration ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    ECommentaryMode CommentaryMode = ECommentaryMode::Hybrid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    FString LeadVoiceId = TEXT("pNInz6obpgDQGcFmaJgB"); // Adam / Energetic Lead

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    FString AnalystVoiceId = TEXT("VR6AewLTigWG4xSOukaG"); // Arnold / Calm Technical Analyst

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    FString ElevenLabsModel = TEXT("eleven_multilingual_v2");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    int32 MaxDynamicRequestsPerMatch = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commentary Config")
    bool bDebugOverlay = true;

    // ---- Gameplay Event Hooks (called from GameMode) ----
    void OnMatchStart();
    void OnPreBall(const FC26CommentaryEvent& Event);
    void OnDeliveryStruck(const FC26CommentaryEvent& Event);
    void OnBallCompleted(const FC26CommentaryEvent& Event);
    void OnWicket(const FC26CommentaryEvent& Event);
    void OnBoundary(const FC26CommentaryEvent& Event);
    void OnMatchResult(const FC26CommentaryEvent& Event);
    void OnInningsBreak(const FC26CommentaryEvent& Event);
    void OnChaseStart(const FC26CommentaryEvent& Event);

    // ---- Diagnostic Test Tooling ----
    UFUNCTION(BlueprintCallable, Category = "Commentary Debug")
    void TestScenario(ECommentaryScenario Scenario);

    // ---- Match Pressure & Emotion API ----
    float CalculatePressure(const FC26CommentaryEvent& Event) const;
    ECommentaryEmotion SelectEmotion(const FC26CommentaryEvent& Event, float Pressure, float& OutIntensity) const;

    // ---- State Inspection ----
    int32 GetDynamicRequestsUsed() const { return DynamicRequestsUsed; }
    FString GetLastPlayedLineId() const { return LastPlayedLineId; }
    ECommentaryEmotion GetLastEmotion() const { return LastEmotion; }
    float GetLastPressure() const { return LastPressure; }

private:
    struct FCommentaryQueueItem
    {
        const FC26CommentaryLineDef* LineDef = nullptr;
        FString ResolvedText;
        FString ElevenLabsPrompt;
        ECommentatorRole Role = ECommentatorRole::Lead;
        ECommentaryEmotion Emotion = ECommentaryEmotion::Neutral;
        float Intensity = 0.5f;
        float Pressure = 0.5f;
        uint8 Priority = 50;
        float PlayAtTime = 0.f;
        float ExpiryTime = 0.f;
        uint32 DeliveryId = 0;
        bool bIsFollowUp = false;
        TObjectPtr<USoundBase> PreloadedSound = nullptr;
    };

    UPROPERTY()
    TObjectPtr<UC26Audio> AudioDirector = nullptr;

    TArray<FCommentaryQueueItem> Queue;
    TArray<FName> RecentLineHistory;
    int32 MaxHistorySize = 12;

    int32 DynamicRequestsUsed = 0;
    float LastSpeechEndTime = -99.f;
    float SpeechCooldownUntil = 0.f;
    uint8 CurrentSpeechPriority = 0;
    bool bIsSpeaking = false;

    // Last commentary diagnostic state
    FString LastPlayedLineId;
    ECommentaryEmotion LastEmotion = ECommentaryEmotion::Neutral;
    float LastPressure = 0.f;
    float LastIntensity = 0.f;

    // Pending async dynamic voice requests
    struct FPendingDynamicRequest
    {
        FString Hash;
        FString LineId;
        float RequestTime = 0.f;
        FCommentaryQueueItem QueueItem;
    };
    TMap<FString, FPendingDynamicRequest> ActiveRequests;

    // Helper methods
    void EvaluateAndQueueEvent(const FC26CommentaryEvent& Event, FName Category, bool bForcePlay = false);
    bool ShouldCommentate(const FC26CommentaryEvent& Event, float Pressure) const;
    const FC26CommentaryLineDef* PickLine(
        FName Category,
        ECommentatorRole Role,
        ECommentaryEmotion Emotion,
        float Pressure,
        float Intensity,
        bool bIsFinalBall,
        bool bIsMatchWinning,
        bool bFollowUp
    );

    void DispatchLine(const FCommentaryQueueItem& Item);
    void PlaySoundOrSynthesize(const FCommentaryQueueItem& Item);
    void ScheduleAnalystFollowUp(const FC26CommentaryEvent& Event, const FC26CommentaryLineDef* LeadLine);

    // Audio / ElevenLabs internals
    FString GetSecureApiKey() const;
    FString GetCacheDirectory() const;
    FString ComputeCacheHash(const FString& VoiceId, const FString& Text, ECommentaryEmotion Emotion, float Intensity) const;
    USoundWave* LoadCachedWav(const FString& FilePath);
    void RequestElevenLabsSynthesis(const FCommentaryQueueItem& Item, const FString& VoiceId, const FString& CachePath);
    USoundWave* CreateSoundWaveFromPcm(const TArray<uint8>& RawWavData);

    void PumpQueue(float CurrentTime);
    float Now() const;
};
