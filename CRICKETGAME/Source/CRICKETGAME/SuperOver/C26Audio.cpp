#include "C26Audio.h"
#include "C26Settings.h"
#include "C26MatchGameMode.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/Actor.h"

// Generated VO table: single source of truth is Tools/CommentaryScript.py.
#include "C26CommentaryData.inc"

static const TCHAR* GC26Sfx[] = {
    TEXT("bat_sweet_spot"), TEXT("bat_edge"), TEXT("bat_defensive"), TEXT("bat_mistimed"),
    TEXT("ball_bounce"), TEXT("stump_hit"), TEXT("crowd_ambience"), TEXT("crowd_anticipation"),
    TEXT("crowd_four"), TEXT("crowd_six"), TEXT("wicket_roar"), TEXT("ui_button_click"),
    TEXT("keeper_catch"), TEXT("ui_result_sting"), TEXT("fielder_gather"), TEXT("foot_plant"),
    TEXT("runup_step"), TEXT("ball_release"), TEXT("final_ball_pulse")
};

UC26Audio::UC26Audio()
{
    PrimaryComponentTick.bCanEverTick = true;
}

float UC26Audio::Now() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UC26Audio::SyncVolumesFromSettings()
{
    // The component lives on the GameMode, which owns the preferences.
    if (AC26MatchGameMode* M = Cast<AC26MatchGameMode>(GetOwner()))
    {
        if (M->Preferences)
        {
            Master = M->Preferences->SoundVolume;
            CommentaryVol = M->Preferences->CommentaryVolume;
            CrowdVol = M->Preferences->CrowdVolume;
            SFXVol = M->Preferences->SFXVolume;
            MusicVol = M->Preferences->MusicVolume;
            UIVol = M->Preferences->UIVol;
        }
    }
}

static UAudioComponent* NewVoice(UObject* Outer, bool bUI)
{
    UAudioComponent* C = NewObject<UAudioComponent>(Outer);
    C->bAutoActivate = false;
    C->bIsUISound = bUI;
    C->RegisterComponent();
    return C;
}

void UC26Audio::Initialize()
{
    SyncVolumesFromSettings();
    for (const TCHAR* N : GC26Sfx)
    {
        if (Sounds.Contains(N))
            continue;
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Cricket26/Audio/%s.%s"), N, N)))
            Sounds.Add(N, S);
    }
    // Load every commentary line. Missing assets (partial import) are simply
    // skipped at selection time, so audio can never hard-fail a build.
    if (VoiceLines.Num() == 0)
    {
        const int32 Count = UE_ARRAY_COUNT(GVoiceRows);
        VoiceLines.Reserve(Count);
        int32 Missing = 0;
        for (int32 I = 0; I < Count; ++I)
        {
            FC26LoadedLine L;
            L.Row = I;
            L.Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Cricket26/Audio/Commentary/%s.%s"), GVoiceRows[I].File, GVoiceRows[I].File));
            if (!L.Sound)
                ++Missing;
            VoiceLines.Add(L);
        }
        UE_LOG(LogC26, Display, TEXT("C26_COMMENTARY_BANK lines=%d missing=%d"), Count, Missing);
    }
    if (!FieldAttenuation)
    {
        // Broadcast field-mic treatment: spatialized so left/right reads, but
        // with a stadium-scale radius and gentle ceiling so the director's
        // distant cameras still hear bat, stumps and gloves clearly.
        FieldAttenuation = NewObject<USoundAttenuation>(this);
        FieldAttenuation->Attenuation.bAttenuate = true;
        FieldAttenuation->Attenuation.bSpatialize = true;
        FieldAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        FieldAttenuation->Attenuation.AttenuationShapeExtents = FVector(11000.f);
        FieldAttenuation->Attenuation.dBAttenuationAtMax = -18.f;
    }
    if (!Ambience)
    {
        Ambience = NewVoice(GetOwner(), true);
        if (TObjectPtr<USoundBase>* S = Sounds.Find(TEXT("crowd_ambience")))
            Ambience->SetSound(S->Get());
    }
    if (!TensionLayer)
    {
        TensionLayer = NewVoice(GetOwner(), true);
        if (TObjectPtr<USoundBase>* S = Sounds.Find(TEXT("crowd_anticipation")))
            TensionLayer->SetSound(S->Get());
    }
    if (!CommentaryVoice)
        CommentaryVoice = NewVoice(GetOwner(), true);
    if (Channels.Num() == 0)
    {
        for (int I = 0; I < 8; ++I)
            Channels.Add(NewVoice(GetOwner(), true));
    }
    if (FieldPool.Num() == 0)
    {
        for (int I = 0; I < 6; ++I)
        {
            UAudioComponent* C = NewVoice(GetOwner(), false);
            C->AttenuationSettings = FieldAttenuation;
            FieldPool.Add(C);
        }
    }
    bInitialized = true;
    if (Ambience && Ambience->GetSound() && !Ambience->IsPlaying())
    {
        Ambience->SetVolumeMultiplier(.18f * CrowdVol * Master);
        Ambience->Play();
    }
}

float UC26Audio::BusFor(FName Name) const
{
    const FString Key = Name.ToString();
    if (Key.StartsWith(TEXT("crowd")))
        return CrowdVol;
    if (Key == TEXT("ui_button_click"))
        return UIVol;
    if (Key == TEXT("ui_result_sting") || Key == TEXT("final_ball_pulse"))
        return MusicVol;
    return SFXVol;
}

void UC26Audio::Cue(FName Name, float Volume)
{
    if (!bInitialized || Channels.IsEmpty())
        return;
    // Sub-60 ms duplicate guard: overlapping gameplay events must never
    // machine-gun the same transient.
    const double T = FPlatformTime::Seconds();
    if (const double* Last = LastCueTime.Find(Name))
    {
        if (T - *Last < .06)
            return;
    }
    LastCueTime.Add(Name, T);
    TObjectPtr<USoundBase>* S = Sounds.Find(Name);
    if (!S || !S->Get())
        return;
    UAudioComponent* C = Channels[NextChannel++ % Channels.Num()];
    const FString Key = Name.ToString();
    const bool Impact = Key.StartsWith(TEXT("bat")) || Key.StartsWith(TEXT("ball"))
        || Key.StartsWith(TEXT("stump")) || Key.StartsWith(TEXT("keeper")) || Key.StartsWith(TEXT("fielder"))
        || Key.StartsWith(TEXT("runup")) || Key.StartsWith(TEXT("foot"));
    const bool CrowdOne = Key.StartsWith(TEXT("crowd")) || Key == TEXT("wicket_roar");
    C->Stop();
    C->SetSound(S->Get());
    C->SetVolumeMultiplier(Volume * Master * BusFor(Name) * (Impact ? FMath::FRandRange(.91f, 1.09f) : 1.f));
    C->SetPitchMultiplier(Impact && !CrowdOne ? FMath::FRandRange(.93f, 1.07f) : 1.f);
    C->Play();
}

void UC26Audio::CueAt(FName Name, const FVector& At, float Volume)
{
    if (!bInitialized || FieldPool.IsEmpty())
    {
        Cue(Name, Volume);
        return;
    }
    const double T = FPlatformTime::Seconds();
    const FName Key2(*FString::Printf(TEXT("3D_%s"), *Name.ToString()));
    if (const double* Last = LastCueTime.Find(Key2))
    {
        if (T - *Last < .06)
            return;
    }
    LastCueTime.Add(Key2, T);
    TObjectPtr<USoundBase>* S2 = Sounds.Find(Name);
    if (!S2 || !S2->Get())
        return;
    // Prefer a free voice; steal round-robin only when all are busy.
    UAudioComponent* Chosen = nullptr;
    for (int I = 0; I < FieldPool.Num(); ++I)
    {
        UAudioComponent* C = FieldPool[(NextField + I) % FieldPool.Num()];
        if (!C->IsPlaying())
        {
            Chosen = C;
            NextField = (NextField + I + 1) % FieldPool.Num();
            break;
        }
    }
    if (!Chosen)
        Chosen = FieldPool[NextField++ % FieldPool.Num()];
    Chosen->Stop();
    Chosen->SetWorldLocation(At);
    Chosen->SetSound(S2->Get());
    Chosen->SetVolumeMultiplier(Volume * Master * SFXVol * FMath::FRandRange(.93f, 1.07f));
    Chosen->SetPitchMultiplier(FMath::FRandRange(.94f, 1.06f));
    Chosen->Play();
}

void UC26Audio::SetTension(float Amount, float Volume)
{
    Master = Volume;
    CrowdTension = FMath::Clamp(Amount, 0.f, 1.f);
}

void UC26Audio::Reset()
{
    if (CommentaryVoice)
        CommentaryVoice->Stop();
    for (auto C : Channels)
        if (C)
            C->Stop();
    for (auto C : FieldPool)
        if (C)
            C->Stop();
    Queue.Reset();
    ActiveRow = -1;
    ActivePriority = 0;
    GapUntil = 0.f;
    LastPrimaryEnd = -1.f;
    LastPrimaryFamily.Empty();
    LastPrimaryPriority = 0;
    LastUsedBall.Reset();
    CurrentBall = 1;
    ConsecutiveBoundaries = 0;
    ConsecutiveDots = 0;
    bPrevWasWicket = false;
    ActiveSubtitle.Empty();
    SubtitleUntil = -1.f;
    DuckFactor = 1.f;
    CrowdTension = .15f;
    SetCrowd(EC26CrowdState::Calm, .22f);
    SyncVolumesFromSettings();
    // The stadium never dies on reset: exactly one bed keeps playing.
    if (bInitialized && Ambience && Ambience->GetSound() && !Ambience->IsPlaying())
    {
        Ambience->SetVolumeMultiplier(.18f * CrowdVol * Master);
        Ambience->Play();
    }
}

int32 UC26Audio::PickLineSingle(const TCHAR* Category, uint32 BallId, bool bFollowOnly, int32 VoiceFilter)
{
    TArray<const TCHAR*> Cats;
    Cats.Add(Category);
    return PickLine(Cats, BallId, bFollowOnly, VoiceFilter);
}

int32 UC26Audio::PickLine(const TArray<const TCHAR*>& Categories, uint32 BallId, bool bFollowOnly, int32 VoiceFilter)
{
    int32 Total = 0;
    TArray<int32> Bag;
    TArray<int32> Weights;
    Bag.Reserve(16);
    for (int32 I = 0; I < VoiceLines.Num(); ++I)
    {
        const FC26CommentaryRowSrc& R = GVoiceRows[VoiceLines[I].Row];
        if (!VoiceLines[I].Sound)
            continue;
        bool bCat = false;
        for (const TCHAR* C : Categories)
        {
            if (FCString::Strcmp(R.Category, C) == 0)
            {
                bCat = true;
                break;
            }
        }
        if (!bCat || R.Follow != bFollowOnly)
            continue;
        if (VoiceFilter >= 0 && R.Commentator != VoiceFilter)
            continue;
        if (const uint32* Last = LastUsedBall.Find(I))
        {
            if (BallId < *Last || BallId - *Last < R.Cooldown)
                continue;
        }
        int32 W = FMath::Max(1, (int32)R.Weight);
        // Specific beats generic: consecutive-boundary lines win when the
        // batter is dominating; final-ball lines only come from FINAL_BALL.
        if (!bFollowOnly && ConsecutiveBoundaries >= 1
            && (FCString::Strstr(R.Id, TEXT("Four.009")) || FCString::Strstr(R.Id, TEXT("Six.008"))))
            W *= 5;
        Bag.Add(I);
        Weights.Add(W);
        Total += W;
    }
    if (Bag.IsEmpty())
        return -1;
    int32 Roll = FMath::RandRange(0, Total - 1);
    for (int32 I = 0; I < Bag.Num(); ++I)
    {
        Roll -= Weights[I];
        if (Roll < 0)
            return Bag[I];
    }
    return Bag.Last();
}

void UC26Audio::QueueLine(int32 RowIdx, uint32 BallId)
{
    if (!VoiceLines.IsValidIndex(RowIdx))
        return;
    const FC26CommentaryRowSrc& R = GVoiceRows[VoiceLines[RowIdx].Row];
    const float T = Now();
    const bool bBusy = CommentaryVoice && CommentaryVoice->IsPlaying();
    if (bBusy)
    {
        // Higher priority interrupts; near-priority queues; lower drops.
        if (R.Priority >= ActivePriority + 15)
        {
            CommentaryVoice->Stop();
            ActiveRow = -1;
            Queue.Reset();
        }
        else if (R.Priority >= ActivePriority && Queue.Num() < 2)
        {
            // fall through to queue
        }
        else
            return;
    }
    if (!bBusy && Queue.IsEmpty() && T >= GapUntil)
    {
        PlayLine(RowIdx, BallId);
        return;
    }
    if (Queue.Num() >= 2)
    {
        // Replace the weakest queued line rather than growing the queue.
        int32 Weakest = 0;
        for (int32 I = 1; I < Queue.Num(); ++I)
            if (Queue[I].Priority < Queue[Weakest].Priority)
                Weakest = I;
        if (R.Priority > Queue[Weakest].Priority)
            Queue.RemoveAt(Weakest);
        else
            return;
    }
    FQueuedLine Q;
    Q.Row = RowIdx;
    Q.Priority = R.Priority;
    Q.StartAt = T + R.Delay;
    Q.Ball = BallId;
    Queue.Add(Q);
}

void UC26Audio::PlayLine(int32 RowIdx, uint32 BallId)
{
    if (!VoiceLines.IsValidIndex(RowIdx) || !CommentaryVoice)
        return;
    if (CommentaryVol <= .01f || Master <= .01f)
        return; // commentary off: stadium and SFX carry the match
    const FC26CommentaryRowSrc& R = GVoiceRows[VoiceLines[RowIdx].Row];
    USoundBase* S = VoiceLines[RowIdx].Sound;
    if (!S)
        return;
    CommentaryVoice->Stop();
    CommentaryVoice->SetSound(S);
    CommentaryVoice->SetVolumeMultiplier(.95f * CommentaryVol * Master);
    CommentaryVoice->SetPitchMultiplier(1.f); // never wobble the voice
    CommentaryVoice->Play();
    ActiveRow = RowIdx;
    ActivePriority = R.Priority;
    ActiveBall = BallId;
    LastUsedBall.Add(RowIdx, BallId);
    LastPrimaryFamily = R.Category;
    LastPrimaryPriority = R.Priority;
    const float Dur = FMath::Max(1.2f, S->GetDuration() + .25f);
    ActiveSubtitle = R.Text;
    SubtitleUntil = Now() + Dur;
    UE_LOG(LogC26, Display, TEXT("C26_COMMENTARY play=%s cat=%s vox=%d pri=%d ball=%u"), R.Id, R.Category, R.Commentator, R.Priority, BallId);
}

void UC26Audio::OnPrimaryFinished()
{
    const float T = Now();
    LastPrimaryEnd = T;
    ActiveRow = -1;
    ActivePriority = 0;
    GapUntil = T + .35f;
    // Occasional analyst follow-up: boundaries and wickets earn them most.
    const bool bBig = LastPrimaryFamily == TEXT("FOUR") || LastPrimaryFamily == TEXT("SIX")
        || LastPrimaryFamily == TEXT("WICKET") || LastPrimaryFamily == TEXT("BOWLED")
        || LastPrimaryFamily == TEXT("CAUGHT") || LastPrimaryFamily == TEXT("KEEPER_CATCH");
    const float P = bBig ? .35f : .18f;
    if (FMath::FRand() > P)
        return;
    int32 Follow = -1;
    if (LastPrimaryFamily == TEXT("FOUR"))
    {
        TArray<const TCHAR*> C;
        C.Add(TEXT("FOUR"));
        C.Add(TEXT("ANALYSIS"));
        Follow = PickLine(C, CurrentBall, true);
    }
    else if (LastPrimaryFamily == TEXT("SIX"))
    {
        TArray<const TCHAR*> C;
        C.Add(TEXT("SIX"));
        C.Add(TEXT("ANALYSIS"));
        Follow = PickLine(C, CurrentBall, true);
    }
    else if (bBig)
    {
        TArray<const TCHAR*> C;
        C.Add(TEXT("WICKET"));
        C.Add(TEXT("ANALYSIS"));
        Follow = PickLine(C, CurrentBall, true);
    }
    else
    {
        Follow = PickLineSingle(TEXT("ANALYSIS"), CurrentBall, true);
    }
    if (Follow >= 0)
    {
        // Follow-ups ignore ball cooldowns (sentinel ball id) but still
        // respect their own repeat distance via LastUsedBall stamping.
        FQueuedLine Q;
        Q.Row = Follow;
        Q.Priority = GVoiceRows[VoiceLines[Follow].Row].Priority;
        Q.StartAt = T + .3f;
        Q.Ball = CurrentBall;
        if (Queue.Num() < 2)
            Queue.Add(Q);
    }
}

void UC26Audio::PumpQueue(float CurrentTime)
{
    if (!CommentaryVoice)
        return;
    if (CommentaryVoice->IsPlaying())
        return;
    if (ActiveRow >= 0)
        OnPrimaryFinished();
    if (!ActiveSubtitle.IsEmpty() && CurrentTime > SubtitleUntil)
        ActiveSubtitle.Empty();
    if (Queue.IsEmpty() || CurrentTime < GapUntil)
        return;
    int32 Best = -1;
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        if (Queue[I].StartAt <= CurrentTime && (Best < 0 || Queue[I].Priority > Queue[Best].Priority))
            Best = I;
    }
    if (Best >= 0)
    {
        const int32 Row = Queue[Best].Row;
        const uint32 Ball = Queue[Best].Ball;
        Queue.RemoveAt(Best);
        PlayLine(Row, Ball);
    }
}

void UC26Audio::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bInitialized)
        return;
    const float T = Now();
    PumpQueue(T);
    VolumeSyncClock += DeltaTime;
    if (VolumeSyncClock > .5f)
    {
        VolumeSyncClock = 0.f;
        SyncVolumesFromSettings();
    }
    // Crowd decay: reactions settle back toward a living base, never silence.
    float Base = .22f;
    switch (CrowdState)
    {
    case EC26CrowdState::Anticipation: Base = .34f; break;
    case EC26CrowdState::Excited: Base = .5f; break;
    case EC26CrowdState::Boundary: Base = .62f; break;
    case EC26CrowdState::Six: Base = .78f; break;
    case EC26CrowdState::Wicket: Base = .85f; break;
    case EC26CrowdState::Tense: Base = .5f; break;
    case EC26CrowdState::Win: Base = 1.f; break;
    case EC26CrowdState::Loss: Base = .3f; break;
    default: break;
    }
    const bool bSustain = CrowdState == EC26CrowdState::Win;
    CrowdIntensity = bSustain ? Base : FMath::FInterpTo(CrowdIntensity, Base, DeltaTime, .55f);
    if (!bSustain && CrowdIntensity - Base < .02f
        && (CrowdState == EC26CrowdState::Boundary || CrowdState == EC26CrowdState::Six || CrowdState == EC26CrowdState::Wicket || CrowdState == EC26CrowdState::Excited))
        CrowdState = EC26CrowdState::Anticipation;
    // Commentary ducking: beds dip ~4 dB under the voice, transients untouched.
    const bool bVoice = CommentaryVoice && CommentaryVoice->IsPlaying();
    const float DuckTarget = bVoice ? .6f : 1.f;
    DuckFactor = FMath::FInterpTo(DuckFactor, DuckTarget, DeltaTime, bVoice ? 8.f : 1.5f);
    if (Ambience)
    {
        if (Ambience->GetSound() && !Ambience->IsPlaying())
            Ambience->Play();
        Ambience->SetVolumeMultiplier((.13f + CrowdIntensity * .22f) * CrowdVol * Master * DuckFactor);
    }
    if (TensionLayer)
    {
        if (TensionLayer->GetSound() && CrowdTension > .05f && !TensionLayer->IsPlaying())
            TensionLayer->Play();
        else if (TensionLayer->IsPlaying() && CrowdTension <= .05f)
            TensionLayer->Stop();
        if (TensionLayer->IsPlaying())
            TensionLayer->SetVolumeMultiplier(CrowdTension * .17f * CrowdVol * Master * DuckFactor);
    }
}

void UC26Audio::SetCrowd(EC26CrowdState State, float Intensity)
{
    CrowdState = State;
    CrowdIntensity = FMath::Max(CrowdIntensity, Intensity);
}

void UC26Audio::NotifyMatchStart()
{
    SetCrowd(EC26CrowdState::Anticipation, .4f);
    const int32 A = PickLineSingle(TEXT("MATCH_START"), 0, false, 0);
    if (A >= 0)
        QueueLine(A, 0);
    if (FMath::FRand() < .6f)
    {
        const int32 B = PickLineSingle(TEXT("MATCH_START"), 0, false, 1);
        if (B >= 0)
            QueueLine(B, 0);
    }
}

void UC26Audio::NotifyPreBall(const FC26CommentaryContext& Ctx)
{
    CurrentBall = Ctx.DeliveryId;
    if ((CommentaryVoice && CommentaryVoice->IsPlaying()) || !Queue.IsEmpty())
        return;
    float P = .45f;
    if (Ctx.bPressure || Ctx.bFinalBall)
        P = .7f;
    if (Ctx.ConsecutiveDots >= 2)
        P = FMath::Max(P, .6f);
    if (FMath::FRand() > P)
        return;
    const TCHAR* Cat = TEXT("PRE_BALL");
    const float R = FMath::FRand();
    if (bPrevWasWicket)
        Cat = TEXT("AFTER_WICKET");
    else if (Ctx.ConsecutiveBoundaries >= 1)
        Cat = TEXT("AFTER_BOUNDARY");
    else if (R < .18f)
        Cat = TEXT("BOWLER_BUILDUP");
    else if (R < .36f)
        Cat = TEXT("BATTER_BUILDUP");
    const int32 L = PickLineSingle(Cat, Ctx.DeliveryId, false);
    if (L >= 0)
        QueueLine(L, Ctx.DeliveryId);
    else if (FCString::Strcmp(Cat, TEXT("PRE_BALL")) != 0)
    {
        const int32 G = PickLineSingle(TEXT("PRE_BALL"), Ctx.DeliveryId, false);
        if (G >= 0)
            QueueLine(G, Ctx.DeliveryId);
    }
}

void UC26Audio::NotifyFinalBallPre()
{
    SetCrowd(EC26CrowdState::Tense, .55f);
    Queue.Reset();
    const int32 L = PickLineSingle(TEXT("FINAL_BALL"), 0xFFFFFFFDu, false, 0);
    if (L >= 0)
        QueueLine(L, 0xFFFFFFFDu);
    else
    {
        const int32 P = PickLineSingle(TEXT("PRESSURE"), 0xFFFFFFFDu, false, 0);
        if (P >= 0)
            QueueLine(P, 0xFFFFFFFDu);
    }
}

void UC26Audio::NotifyDelivery(const FC26CommentaryContext& Ctx)
{
    CurrentBall = Ctx.DeliveryId;
    // Delivery/shot analysis. Beaten/mistimed always earn a word; stock
    // good balls stay quiet half the time so the cricket can breathe.
    const TCHAR* Cat = nullptr;
    float P = .5f;
    if (Ctx.bBeaten)
    {
        Cat = TEXT("DELIVERY");
        P = .8f;
    }
    else if (Ctx.bMistimed)
    {
        Cat = TEXT("SHOT_MISTIMED");
        P = .75f;
    }
    else if (Ctx.TimingResult == 4) // Edge: result line covers it
        return;
    else if (Ctx.ContactQuality > .15f && Ctx.ContactQuality < .5f)
        Cat = TEXT("SHOT_DEFENCE");
    else
        Cat = TEXT("DELIVERY");
    if (FMath::FRand() > P)
        return;
    const int32 L = PickLineSingle(Cat, Ctx.DeliveryId, false);
    if (L >= 0)
        QueueLine(L, Ctx.DeliveryId);
}

void UC26Audio::NotifyResult(const FC26CommentaryContext& Ctx)
{
    CurrentBall = Ctx.DeliveryId;
    if (Ctx.bFour)
    {
        const int32 L = PickLineSingle(TEXT("FOUR"), Ctx.DeliveryId, false);
        if (L >= 0)
            QueueLine(L, Ctx.DeliveryId);
        SetCrowd(EC26CrowdState::Boundary, .7f);
        Cue(TEXT("crowd_four"), .7f);
    }
    else if (Ctx.bSix)
    {
        const int32 L = PickLineSingle(TEXT("SIX"), Ctx.DeliveryId, false);
        if (L >= 0)
            QueueLine(L, Ctx.DeliveryId);
        SetCrowd(EC26CrowdState::Six, .85f);
        Cue(TEXT("crowd_six"), .85f);
    }
    else if (Ctx.bEdge)
    {
        const int32 L = PickLineSingle(TEXT("EDGE"), Ctx.DeliveryId, false);
        if (L >= 0)
            QueueLine(L, Ctx.DeliveryId);
        SetCrowd(EC26CrowdState::Excited, .55f);
        Cue(TEXT("crowd_anticipation"), .3f);
    }
    else if (Ctx.RunsScored <= 0)
    {
        if (FMath::FRand() < .55f)
        {
            const int32 L = PickLineSingle(TEXT("DOT"), Ctx.DeliveryId, false);
            if (L >= 0)
                QueueLine(L, Ctx.DeliveryId);
        }
        SetCrowd(EC26CrowdState::Anticipation, .34f);
    }
    else
    {
        if (FMath::FRand() < .7f)
        {
            const int32 L = PickLineSingle(TEXT("RUNS"), Ctx.DeliveryId, false);
            if (L >= 0)
                QueueLine(L, Ctx.DeliveryId);
        }
        SetCrowd(EC26CrowdState::Excited, .45f);
        Cue(TEXT("crowd_anticipation"), .22f);
    }
}

void UC26Audio::NotifyWicket(const FC26CommentaryContext& Ctx)
{
    CurrentBall = Ctx.DeliveryId;
    const TCHAR* Cat = TEXT("WICKET");
    if (Ctx.WicketType == 1)
        Cat = TEXT("BOWLED");
    else if (Ctx.WicketType == 2)
        Cat = TEXT("CAUGHT");
    else if (Ctx.WicketType == 4)
        Cat = TEXT("KEEPER_CATCH");
    int32 L = PickLineSingle(Cat, Ctx.DeliveryId, false);
    if (L < 0 && FCString::Strcmp(Cat, TEXT("WICKET")) != 0)
        L = PickLineSingle(TEXT("WICKET"), Ctx.DeliveryId, false);
    if (L >= 0)
        QueueLine(L, Ctx.DeliveryId);
    SetCrowd(EC26CrowdState::Wicket, .9f);
    Cue(TEXT("wicket_roar"), .85f);
}

void UC26Audio::NotifyInningsBreak()
{
    const int32 L = PickLineSingle(TEXT("INNINGS_BREAK"), 0xFFFFFFFBu, false, 0);
    if (L >= 0)
        QueueLine(L, 0xFFFFFFFBu);
    SetCrowd(EC26CrowdState::Excited, .55f);
}

void UC26Audio::NotifyChaseStart()
{
    const int32 A = PickLineSingle(TEXT("CHASE_START"), 0xFFFFFFFAu, false, 0);
    if (A >= 0)
        QueueLine(A, 0xFFFFFFFAu);
    if (FMath::FRand() < .5f)
    {
        const int32 B = PickLineSingle(TEXT("CHASE_START"), 0xFFFFFFFAu, false, 1);
        if (B >= 0)
            QueueLine(B, 0xFFFFFFFAu);
    }
    SetCrowd(EC26CrowdState::Anticipation, .45f);
}

void UC26Audio::NotifyMatchResult(bool bPlayerWon, bool bTie)
{
    const TCHAR* Cat = bTie ? TEXT("MATCH_TIE") : (bPlayerWon ? TEXT("MATCH_WIN") : TEXT("MATCH_LOSS"));
    int32 L = PickLineSingle(Cat, 0xFFFFFFF9u, false, 0);
    if (L < 0)
        L = PickLineSingle(bTie ? TEXT("MATCH_TIE") : TEXT("MATCH_WIN"), 0xFFFFFFF9u, false);
    SetCrowd(bTie || bPlayerWon ? EC26CrowdState::Win : EC26CrowdState::Loss, 1.f);
    if (L >= 0)
    {
        // Result outranks everything, but a boundary call already in flight
        // gets to finish: the result line is queued behind it instead of
        // cutting it off mid-sentence.
        const bool bBoundarySpeaking = CommentaryVoice && CommentaryVoice->IsPlaying() && ActivePriority >= 70;
        if (bBoundarySpeaking)
            QueueLine(L, 0xFFFFFFF9u);
        else
        {
            if (CommentaryVoice && CommentaryVoice->IsPlaying())
                CommentaryVoice->Stop();
            ActiveRow = -1;
            Queue.Reset();
            PlayLine(L, 0xFFFFFFF9u);
        }
        if (!bTie && FMath::FRand() < .6f)
        {
            const int32 F = PickLineSingle(Cat, 0xFFFFFFF8u, true, 1);
            if (F >= 0)
                QueueLine(F, 0xFFFFFFF8u);
        }
    }
    UE_LOG(LogC26, Display, TEXT("C26_COMMENTARY result won=%d tie=%d"), bPlayerWon ? 1 : 0, bTie ? 1 : 0);
}

void UC26Audio::NoteBallCompleted(int32 RunsScored, bool bWicket, bool bBoundary)
{
    if (bWicket)
    {
        ConsecutiveBoundaries = 0;
        ConsecutiveDots = 0;
        bPrevWasWicket = true;
    }
    else if (bBoundary)
    {
        ++ConsecutiveBoundaries;
        ConsecutiveDots = 0;
        bPrevWasWicket = false;
    }
    else if (RunsScored <= 0)
    {
        ++ConsecutiveDots;
        ConsecutiveBoundaries = 0;
        bPrevWasWicket = false;
    }
    else
    {
        ConsecutiveBoundaries = 0;
        ConsecutiveDots = 0;
        bPrevWasWicket = false;
    }
}

void UC26Audio::TestCommentary(FName Category)
{
    const FString C = Category.ToString().ToUpper();
    const TCHAR* Cat = TEXT("FOUR");
    if (C == TEXT("SIX"))
        Cat = TEXT("SIX");
    else if (C == TEXT("WICKET") || C == TEXT("BOWLED") || C == TEXT("CAUGHT"))
        Cat = TEXT("WICKET");
    else if (C == TEXT("FINALBALL") || C == TEXT("FINAL_BALL"))
        Cat = TEXT("FINAL_BALL");
    else if (C == TEXT("DOT"))
        Cat = TEXT("DOT");
    else if (C == TEXT("RUNS") || C == TEXT("ONE") || C == TEXT("TWO"))
        Cat = TEXT("RUNS");
    else if (C == TEXT("DELIVERY") || C == TEXT("BEATEN"))
        Cat = TEXT("DELIVERY");
    else if (C == TEXT("PRESSURE"))
        Cat = TEXT("PRESSURE");
    else if (C == TEXT("WIN") || C == TEXT("RESULT"))
        Cat = TEXT("MATCH_WIN");
    else if (C == TEXT("LOSS"))
        Cat = TEXT("MATCH_LOSS");
    else if (C == TEXT("START") || C == TEXT("MATCH"))
        Cat = TEXT("MATCH_START");
    const int32 L = PickLineSingle(Cat, 0xFFFFFFF0u, false);
    if (L >= 0)
    {
        if (CommentaryVoice && CommentaryVoice->IsPlaying())
            CommentaryVoice->Stop();
        ActiveRow = -1;
        Queue.Reset();
        PlayLine(L, 0xFFFFFFF9u);
        UE_LOG(LogC26, Display, TEXT("C26_COMMENTARY_TEST cat=%s row=%d"), Cat, L);
    }
    else
        UE_LOG(LogC26, Warning, TEXT("C26_COMMENTARY_TEST cat=%s no line (assets missing?)"), Cat);
}

void UC26Audio::DumpState() const
{
    UE_LOG(LogC26, Display, TEXT("C26_AUDIO crowd=%d intensity=%.2f tension=%.2f duck=%.2f queue=%d active=%d sub=%s"),
        (int32)CrowdState, CrowdIntensity, CrowdTension, DuckFactor, Queue.Num(), ActiveRow, *ActiveSubtitle);
}
