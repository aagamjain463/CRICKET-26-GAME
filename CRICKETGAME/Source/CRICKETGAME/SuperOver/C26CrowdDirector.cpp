#include "C26CrowdDirector.h"
#include "C26Audio.h"
#include "C26Types.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

UC26CrowdDirector::UC26CrowdDirector()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UAudioComponent* UC26CrowdDirector::CreateDedicatedVoice(bool bIsUI)
{
    UAudioComponent* Comp = NewObject<UAudioComponent>(GetOwner());
    if (Comp)
    {
        Comp->bAutoActivate = false;
        Comp->bIsUISound = bIsUI;
        Comp->RegisterComponent();
    }
    return Comp;
}

void UC26CrowdDirector::Initialize(UC26Audio* InAudioDirector)
{
    AudioDirector = InAudioDirector;

    if (!BaseAmbienceComp)
    {
        BaseAmbienceComp = CreateDedicatedVoice(true);
    }
    if (!TensionDroneComp)
    {
        TensionDroneComp = CreateDedicatedVoice(true);
    }
    if (!PrimaryReactionComp)
    {
        PrimaryReactionComp = CreateDedicatedVoice(true);
    }
    if (!SecondaryReactionComp)
    {
        SecondaryReactionComp = CreateDedicatedVoice(true);
    }

    // Load sound assets
    if (!SoundAmbience)
    {
        SoundAmbience = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Cricket26/Audio/crowd_ambience.crowd_ambience")));
    }
    if (!SoundTension)
    {
        SoundTension = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Cricket26/Audio/crowd_anticipation.crowd_anticipation")));
    }
    if (!SoundFour)
    {
        SoundFour = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Cricket26/Audio/crowd_four.crowd_four")));
    }
    if (!SoundSix)
    {
        SoundSix = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Cricket26/Audio/crowd_six.crowd_six")));
    }
    if (!SoundWicket)
    {
        SoundWicket = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Cricket26/Audio/wicket_roar.wicket_roar")));
    }

    if (BaseAmbienceComp && SoundAmbience)
    {
        BaseAmbienceComp->SetSound(SoundAmbience);
        BaseAmbienceComp->Play();
    }
    if (TensionDroneComp && SoundTension)
    {
        TensionDroneComp->SetSound(SoundTension);
        TensionDroneComp->SetVolumeMultiplier(0.f);
        TensionDroneComp->Play();
    }

    bInitialized = true;
    UE_LOG(LogC26, Display, TEXT("C26_CROWD_DIRECTOR: Initialized broadcast crowd beds."));
}

void UC26CrowdDirector::Reset()
{
    CurrentPressure = 0.20f;
    TargetPressure = 0.20f;
    CurrentTension = 0.15f;
    ExcitementMomentum = 0.0f;
    bCommentaryActive = false;
    DuckFactor = 1.0f;
    TargetDuckFactor = 1.0f;
    TransientMicroDuckTimer = 0.0f;
}

void UC26CrowdDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (DeltaTime <= 0.f || !bInitialized)
    {
        return;
    }

    // Decay transient excitement momentum over 15-20 seconds (rate ~ 0.06f)
    ExcitementMomentum = FMath::FInterpTo(ExcitementMomentum, 0.0f, DeltaTime, 0.06f);

    // Smooth pressure and tension
    CurrentPressure = FMath::FInterpTo(CurrentPressure, TargetPressure, DeltaTime, 2.0f);
    CurrentTension = FMath::FInterpTo(CurrentTension, CurrentPressure, DeltaTime, 3.0f);

    // Transient micro-duck timer for hero bat crack
    if (TransientMicroDuckTimer > 0.f)
    {
        TransientMicroDuckTimer -= DeltaTime;
    }

    // Calculate Target Duck Factor:
    // Commentary speaking: duck crowd by ~4.7 dB (multiplier 0.58f)
    // Sweet-spot contact: micro-duck crowd by ~2.2 dB (multiplier 0.77f) for 90ms
    TargetDuckFactor = 1.0f;
    if (bCommentaryActive)
    {
        TargetDuckFactor *= 0.58f;
    }
    if (TransientMicroDuckTimer > 0.f)
    {
        TargetDuckFactor *= 0.77f;
    }

    // Smooth sidechain duck filter
    const float InterpSpeed = (TargetDuckFactor < DuckFactor) ? 18.0f : 3.5f;
    DuckFactor = FMath::FInterpTo(DuckFactor, TargetDuckFactor, DeltaTime, InterpSpeed);

    // Mix bus calculation
    float MasterVol = 1.0f;
    float CrowdVol = CrowdMasterVolume;
    if (AudioDirector)
    {
        MasterVol = AudioDirector->Master;
        CrowdVol = AudioDirector->CrowdVol;
    }

    // Apply continuous ambient bed volume
    if (BaseAmbienceComp && BaseAmbienceComp->IsPlaying())
    {
        const float BaseIntensity = FMath::Clamp(0.18f + ExcitementMomentum * 0.45f, 0.12f, 0.95f);
        const float EffectiveBase = BaseIntensity * MasterVol * CrowdVol * DuckFactor;
        BaseAmbienceComp->SetVolumeMultiplier(EffectiveBase);
    }

    // Apply situational tension drone volume
    if (TensionDroneComp && TensionDroneComp->IsPlaying())
    {
        const float EffectiveTension = FMath::Clamp(CurrentTension, 0.0f, 1.0f) * MasterVol * CrowdVol * DuckFactor;
        TensionDroneComp->SetVolumeMultiplier(EffectiveTension);
    }
}

void UC26CrowdDirector::SetMatchPressure(float InPressure)
{
    TargetPressure = FMath::Clamp(InPressure, 0.f, 1.f);
}

void UC26CrowdDirector::SetCommentarySpeaking(bool bSpeaking)
{
    bCommentaryActive = bSpeaking;
}

void UC26CrowdDirector::TriggerBatContactDucking(bool bSweetSpot)
{
    if (bSweetSpot)
    {
        TransientMicroDuckTimer = 0.090f; // 90ms micro-duck
    }
}

void UC26CrowdDirector::PlayReactionSound(USoundBase* Sound, float VolumeMultiplier, float PitchMultiplier)
{
    if (!Sound)
    {
        return;
    }

    UAudioComponent* Comp = PrimaryReactionComp;
    if (Comp && Comp->IsPlaying())
    {
        Comp = SecondaryReactionComp;
    }

    if (Comp)
    {
        Comp->SetSound(Sound);
        Comp->SetVolumeMultiplier(VolumeMultiplier * DuckFactor);
        Comp->SetPitchMultiplier(PitchMultiplier);
        Comp->Play();
    }
}

void UC26CrowdDirector::OnMatchStart()
{
    Reset();
    ExcitementMomentum = 0.40f;
    TargetPressure = 0.15f;
}

void UC26CrowdDirector::OnPreBall(float Pressure, bool bIsFinalBall)
{
    TargetPressure = Pressure;
    if (bIsFinalBall)
    {
        CurrentTension = 0.85f;
        ExcitementMomentum = FMath::Max(ExcitementMomentum, 0.50f);
    }
}

void UC26CrowdDirector::OnBatContact(bool bSweetSpot)
{
    TriggerBatContactDucking(bSweetSpot);
}

void UC26CrowdDirector::OnBoundary(bool bIsSix, bool bHomeTeamBatting)
{
    // Partisan crowd allegiances: home boundary = full cheer, away boundary = subdued cheer
    const float PartisanMultiplier = bHomeTeamBatting ? 1.0f : 0.45f;
    const float ReactionVol = (bIsSix ? 1.0f : 0.80f) * PartisanMultiplier;

    ExcitementMomentum = FMath::Clamp(ExcitementMomentum + (bIsSix ? 0.65f : 0.45f), 0.f, 1.f);
    TargetPressure = FMath::Max(0.05f, TargetPressure - 0.25f);

    USoundBase* Sound = bIsSix ? SoundSix : SoundFour;
    PlayReactionSound(Sound, ReactionVol, FMath::FRandRange(0.98f, 1.02f));
}

void UC26CrowdDirector::OnWicket(bool bHomeTeamBatting)
{
    // Home wicket = stunned murmur / away fans cheer; Away wicket = stadium-shattering roar
    const float PartisanMultiplier = bHomeTeamBatting ? 0.35f : 1.0f;
    ExcitementMomentum = 0.85f * PartisanMultiplier;
    TargetPressure = 0.30f;

    PlayReactionSound(SoundWicket, 1.0f * PartisanMultiplier, FMath::FRandRange(0.97f, 1.03f));
}

void UC26CrowdDirector::OnDotBall(float Pressure, int32 ConsecutiveDots)
{
    // Ratchet tension as dots accumulate
    TargetPressure = FMath::Clamp(Pressure + (ConsecutiveDots * 0.08f), 0.10f, 0.90f);
}

void UC26CrowdDirector::OnNearMiss()
{
    // Near miss gasp / surge
    ExcitementMomentum = FMath::Clamp(ExcitementMomentum + 0.35f, 0.f, 0.85f);
}

void UC26CrowdDirector::OnInningsBreak()
{
    ExcitementMomentum = 0.20f;
    TargetPressure = 0.10f;
}

void UC26CrowdDirector::OnMatchEnd(bool bHomeTeamWon, bool bTie)
{
    if (bTie)
    {
        ExcitementMomentum = 1.0f;
        PlayReactionSound(SoundSix, 1.0f, 1.0f);
    }
    else if (bHomeTeamWon)
    {
        ExcitementMomentum = 1.0f;
        PlayReactionSound(SoundSix, 1.0f, 1.0f);
    }
    else
    {
        ExcitementMomentum = 0.35f;
    }
}
