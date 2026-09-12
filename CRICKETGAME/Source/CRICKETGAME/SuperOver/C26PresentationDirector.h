#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26Types.h"
#include "C26PresentationTypes.h"
#include "C26PresentationDirector.generated.h"

class AC26Athlete;
class AC26MatchGameMode;

/**
 * UC26PresentationDirector
 * Central director for broadcast match presentation, cinematic scenes,
 * camera direction, procedural athlete acting, audio stings, and replay overlays.
 *
 * Designed to guarantee:
 * 1. Gameplay remains the hero - scenes enhance emotion rather than stall flow.
 * 2. Absolute match state safety - authoritative scores, no soft-locks.
 * 3. Robust anti-repetition - cooldowns and recency penalties prevent stale scenes.
 * 4. Exact fire-once milestone rules for 50s, 100s, wickets, and match outcomes.
 * 5. Immediate, safe skipping returning players cleanly to gameplay poses.
 */
UCLASS(ClassGroup=(Cricket26), meta=(BlueprintSpawnableComponent))
class CRICKETGAME_API UC26PresentationDirector : public UActorComponent
{
    GENERATED_BODY()

public:
    UC26PresentationDirector();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ---- Presentation API ----

    /** Request a cinematic presentation scene. Evaluates priority, eligibility, and pacing. */
    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    bool RequestPresentation(const FC26PresentationRequest& Request);

    /** Safe immediate skip by player tap / spacebar. Restores athlete poses and advances queue. */
    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    void SkipCurrentScene();

    /** Advance to next queued item or cleanly return to gameplay. */
    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    void FinishCurrentScene();

    /** Clear all pending queued presentation requests. */
    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    void ClearQueue();

    // ---- Pacing & Filtering ----

    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    void SetPacing(EC26PresentationPacing NewPacing) { PacingMode = NewPacing; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    EC26PresentationPacing GetPacing() const { return PacingMode; }

    // ---- State Inspection ----

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    bool IsPresentationActive() const { return bPresentationActive; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    EC26PresentationEvent GetCurrentEvent() const { return ActiveEvent; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    const FC26PresentationSceneDefinition& GetCurrentSceneDef() const { return ActiveSceneDef; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    const FC26PresentationQueueItem& GetCurrentQueueItem() const { return ActiveQueueItem; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    float GetSceneProgress() const { return ActiveSceneDef.Duration > 0.01f ? FMath::Clamp(SceneTime / ActiveSceneDef.Duration, 0.f, 1.f) : 1.f; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    float GetSceneTime() const { return SceneTime; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    float GetSceneDuration() const { return ActiveSceneDef.Duration; }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    int32 GetQueueCount() const { return PresentationQueue.Num(); }

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    int32 GetVariantCount(EC26PresentationEvent Event) const;

    const TArray<FC26RecentSceneRecord>& GetRecentScenes() const { return RecentScenes; }

    // ---- Dynamic Match Pressure ----

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    float CalculateMatchPressure(int32 TargetRuns, int32 CurrentRuns, int32 BallsRemaining, int32 WicketsDown) const;

    // ---- Toss Coin Physics Solver ----

    UFUNCTION(BlueprintPure, Category="Cricket26|Presentation")
    bool GetTossCoinState(FVector& OutPosition, FRotator& OutRotation) const;

    // ---- Development & Debug Triggers ----

    UFUNCTION(BlueprintCallable, Category="Cricket26|Presentation")
    void TriggerDebugScene(EC26PresentationEvent Event, AC26Athlete* P1 = nullptr, AC26Athlete* P2 = nullptr);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cricket26|Presentation")
    bool bDebugOverlayVisible = false;

protected:
    void InitializeSceneLibrary();
    bool EvaluateEligibility(const FC26PresentationRequest& Request) const;
    FC26PresentationSceneDefinition SelectSceneDefinition(const FC26PresentationRequest& Request);
    void ApplySceneParticipants(const FC26PresentationSceneDefinition& Def, const FC26PresentationRequest& Req);
    void RestoreParticipantPoses();
    void RecordScenePlayed(EC26PresentationEvent Event, FName VariantId);
    void DirectActiveCamera(float Dt);

private:
    UPROPERTY(EditAnywhere, Category="Presentation")
    EC26PresentationPacing PacingMode = EC26PresentationPacing::Balanced;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    bool bPresentationActive = false;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    EC26PresentationEvent ActiveEvent = EC26PresentationEvent::None;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    FC26PresentationSceneDefinition ActiveSceneDef;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    FC26PresentationQueueItem ActiveQueueItem;

    UPROPERTY(VisibleAnywhere, Category="Presentation")
    float SceneTime = 0.f;

    UPROPERTY()
    TArray<FC26PresentationQueueItem> PresentationQueue;

    UPROPERTY()
    TArray<FC26RecentSceneRecord> RecentScenes;

    TMap<EC26PresentationEvent, TArray<FC26PresentationSceneDefinition>> SceneLibrary;

    struct FParticipantRestoreState
    {
        TWeakObjectPtr<AC26Athlete> Athlete;
        FTransform OriginalTransform;
        EC26Action OriginalAction;
        bool bWasRunning = false;
    };
    TArray<FParticipantRestoreState> RestoredParticipants;

    // Toss coin flip physics
    bool bTossCoinActive = false;
    float TossCoinTimer = 0.f;
    FVector TossCoinStart = FVector::ZeroVector;
    FVector TossCoinApex = FVector::ZeroVector;
    FVector TossCoinLanding = FVector::ZeroVector;
    FRotator TossCoinRotation = FRotator::ZeroRotator;

    AC26MatchGameMode* GetGameMode() const;
};
