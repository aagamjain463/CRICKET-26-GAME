#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26CrowdDirector.generated.h"

class UAudioComponent;
class USoundBase;
class UC26Audio;

/**
 * Broadcast-quality multi-layer crowd director for CRICKET 26.
 * Manages continuous stadium room-tone, situational tension beds,
 * dynamic partisan crowd reactions (4s, 6s, wickets, near misses),
 * smooth exponential decay curves, and sidechain ducking.
 */
UCLASS(ClassGroup=(Cricket))
class CRICKETGAME_API UC26CrowdDirector : public UActorComponent
{
    GENERATED_BODY()

public:
    UC26CrowdDirector();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void Initialize(UC26Audio* InAudioDirector);
    void Reset();

    // ---- Match Event Ingestion ----
    void OnMatchStart();
    void OnPreBall(float Pressure, bool bIsFinalBall);
    void OnBatContact(bool bSweetSpot);
    void OnBoundary(bool bIsSix, bool bHomeTeamBatting);
    void OnWicket(bool bHomeTeamBatting);
    void OnDotBall(float Pressure, int32 ConsecutiveDots);
    void OnNearMiss();
    void OnInningsBreak();
    void OnMatchEnd(bool bHomeTeamWon, bool bTie);

    // ---- Dynamic Parameters & Sidechain Ducking ----
    void SetMatchPressure(float InPressure);
    void SetCommentarySpeaking(bool bSpeaking);
    void TriggerBatContactDucking(bool bSweetSpot);
    void SetMasterCrowdVolume(float InVol) { CrowdMasterVolume = FMath::Clamp(InVol, 0.f, 1.f); }

    // ---- Diagnostic State ----
    float GetCurrentExcitement() const { return ExcitementMomentum; }
    float GetTensionLevel() const { return CurrentTension; }

private:
    UPROPERTY() TObjectPtr<UC26Audio> AudioDirector = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> BaseAmbienceComp = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> TensionDroneComp = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> PrimaryReactionComp = nullptr;
    UPROPERTY() TObjectPtr<UAudioComponent> SecondaryReactionComp = nullptr;

    UPROPERTY() TObjectPtr<USoundBase> SoundAmbience = nullptr;
    UPROPERTY() TObjectPtr<USoundBase> SoundTension = nullptr;
    UPROPERTY() TObjectPtr<USoundBase> SoundFour = nullptr;
    UPROPERTY() TObjectPtr<USoundBase> SoundSix = nullptr;
    UPROPERTY() TObjectPtr<USoundBase> SoundWicket = nullptr;

    bool bInitialized = false;
    float CrowdMasterVolume = 0.85f;

    // Continuous dynamic state
    float CurrentPressure = 0.20f;
    float TargetPressure = 0.20f;
    float CurrentTension = 0.15f;
    float ExcitementMomentum = 0.0f; // Decays over 15-20 seconds

    // Sidechain ducking parameters
    bool bCommentaryActive = false;
    float DuckFactor = 1.0f;
    float TargetDuckFactor = 1.0f;
    float TransientMicroDuckTimer = 0.0f;

    void PlayReactionSound(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier = 1.0f);
    UAudioComponent* CreateDedicatedVoice(bool bIsUI);
};
