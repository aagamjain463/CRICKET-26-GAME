#include "C26CommentaryDirector.h"
#include "C26Types.h"
#include "C26CommentaryLibrary.h"
#include "C26Audio.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UC26CommentaryDirector::UC26CommentaryDirector()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UC26CommentaryDirector::BeginPlay()
{
    Super::BeginPlay();

    if (AActor* Owner = GetOwner())
    {
        AudioDirector = Owner->FindComponentByClass<UC26Audio>();
    }

    // Ensure disk cache directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString CacheDir = GetCacheDirectory();
    if (!PlatformFile.DirectoryExists(*CacheDir))
    {
        PlatformFile.CreateDirectoryTree(*CacheDir);
    }

    UE_LOG(LogC26, Display, TEXT("C26_COMMENTARY_DIRECTOR Initialized: Mode=%d, CacheDir=%s"), (int32)CommentaryMode, *CacheDir);
}

void UC26CommentaryDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const float CurrentTime = Now();

    // Check if voice finished speaking
    if (bIsSpeaking)
    {
        const bool bVoiceActive = AudioDirector && AudioDirector->IsCommentaryPlaying();
        if (!bVoiceActive && CurrentTime >= SpeechCooldownUntil)
        {
            bIsSpeaking = false;
            CurrentSpeechPriority = 0;
            LastSpeechEndTime = CurrentTime;
        }
    }

    PumpQueue(CurrentTime);
}

float UC26CommentaryDirector::Now() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

FString UC26CommentaryDirector::GetCacheDirectory() const
{
    return FPaths::ProjectSavedDir() / TEXT("CommentaryCache");
}

FString UC26CommentaryDirector::GetSecureApiKey() const
{
    // 1. Command-line override: -ElevenLabsKey=<key>
    FString Key;
    if (FParse::Value(FCommandLine::Get(), TEXT("ElevenLabsKey="), Key))
    {
        return Key.TrimStartAndEnd();
    }

    // 2. Environment variable: ELEVENLABS_API_KEY
    FString EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ELEVENLABS_API_KEY"));
    if (!EnvKey.IsEmpty())
    {
        return EnvKey.TrimStartAndEnd();
    }

    // 3. Untracked local config file: Saved/Config/ElevenLabs.ini
    const FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/ElevenLabs.ini");
    if (FPaths::FileExists(ConfigPath))
    {
        FString FileContent;
        if (FFileHelper::LoadFileToString(FileContent, *ConfigPath))
        {
            TArray<FString> Lines;
            FileContent.ParseIntoArrayLines(Lines);
            for (const FString& L : Lines)
            {
                FString Trimmed = L.TrimStartAndEnd();
                if (Trimmed.StartsWith(TEXT("ApiKey=")))
                {
                    return Trimmed.RightChop(7).TrimStartAndEnd();
                }
            }
        }
    }

    return FString();
}

float UC26CommentaryDirector::CalculatePressure(const FC26CommentaryEvent& Event) const
{
    float Pressure = 0.20f; // Base match pressure

    // Innings 2 chase calculation
    if (Event.InningsNumber >= 1)
    {
        const int32 Needed = Event.RunsRequired;
        const int32 Left = Event.BallsRemaining;

        if (Left > 0)
        {
            const float RRR = (float)Needed / (float)Left * 6.0f;
            if (RRR >= 18.0f) Pressure = 0.90f;
            else if (RRR >= 12.0f) Pressure = 0.75f;
            else if (RRR >= 8.0f) Pressure = 0.55f;
            else Pressure = 0.35f;

            if (Left <= 2) Pressure = FMath::Max(Pressure, 0.85f);
            if (Left == 1) Pressure = FMath::Max(Pressure, 0.95f);
        }
        else
        {
            Pressure = 0.95f;
        }
    }
    else
    {
        // 1st innings Super Over
        if (Event.BallNumber >= 4) Pressure = 0.60f;
        if (Event.BallNumber >= 5) Pressure = 0.75f;
    }

    // Wickets pressure (in Super Over, 2 wickets = innings over)
    if (Event.Wickets >= 1)
    {
        Pressure = FMath::Clamp(Pressure + 0.20f, 0.0f, 1.0f);
    }

    // Consecutive dot balls build bowling pressure
    if (Event.ConsecutiveDots >= 2)
    {
        Pressure = FMath::Clamp(Pressure + 0.15f, 0.0f, 1.0f);
    }

    return FMath::Clamp(Pressure, 0.0f, 1.0f);
}

ECommentaryEmotion UC26CommentaryDirector::SelectEmotion(const FC26CommentaryEvent& Event, float Pressure, float& OutIntensity) const
{
    OutIntensity = 0.5f;

    if (Event.bIsMatchWinningEvent || Event.EventType == ECommentaryEventType::MatchWin)
    {
        OutIntensity = 1.0f;
        return ECommentaryEmotion::Celebratory;
    }

    if (Event.bIsFinalBall || Event.EventType == ECommentaryEventType::FinalBall)
    {
        OutIntensity = 0.95f;
        return ECommentaryEmotion::Dramatic;
    }

    if (Event.bIsSix || Event.EventType == ECommentaryEventType::Six)
    {
        if (Pressure >= 0.75f)
        {
            OutIntensity = 0.95f;
            return ECommentaryEmotion::VeryExcited;
        }
        OutIntensity = 0.75f;
        return ECommentaryEmotion::Excited;
    }

    if (Event.bIsBoundary || Event.EventType == ECommentaryEventType::Four)
    {
        if (Pressure >= 0.70f)
        {
            OutIntensity = 0.85f;
            return ECommentaryEmotion::Excited;
        }
        OutIntensity = 0.65f;
        return ECommentaryEmotion::Appreciative;
    }

    if (Event.bIsWicket || Event.EventType == ECommentaryEventType::Wicket ||
        Event.EventType == ECommentaryEventType::Bowled || Event.EventType == ECommentaryEventType::Caught)
    {
        if (Pressure >= 0.65f)
        {
            OutIntensity = 0.90f;
            return ECommentaryEmotion::Shocked;
        }
        OutIntensity = 0.80f;
        return ECommentaryEmotion::Dramatic;
    }

    if (Event.EventType == ECommentaryEventType::Edge || Event.EventType == ECommentaryEventType::Beaten)
    {
        OutIntensity = 0.65f;
        return Pressure >= 0.6f ? ECommentaryEmotion::Tense : ECommentaryEmotion::Analytical;
    }

    if (Pressure >= 0.70f)
    {
        OutIntensity = 0.70f;
        return ECommentaryEmotion::Tense;
    }

    OutIntensity = 0.40f;
    return ECommentaryEmotion::Neutral;
}

bool UC26CommentaryDirector::ShouldCommentate(const FC26CommentaryEvent& Event, float Pressure) const
{
    // Critical events never get suppressed
    if (Event.bIsMatchWinningEvent || Event.bIsFinalBall || Event.bIsWicket || Event.bIsSix || Event.bIsBoundary)
    {
        return true;
    }

    if (Event.EventType == ECommentaryEventType::MatchStart ||
        Event.EventType == ECommentaryEventType::ChaseStart ||
        Event.EventType == ECommentaryEventType::InningsBreak ||
        Event.EventType == ECommentaryEventType::MatchWin ||
        Event.EventType == ECommentaryEventType::MatchLoss ||
        Event.EventType == ECommentaryEventType::MatchTie)
    {
        return true;
    }

    // High pressure situations warrant commentary
    if (Pressure >= 0.75f)
    {
        return true;
    }

    // Routine balls are suppressed ~60% of the time to let the stadium breathe
    const float Roll = FMath::FRand();
    return (Roll < 0.40f);
}

const FC26CommentaryLineDef* UC26CommentaryDirector::PickLine(
    FName Category,
    ECommentatorRole Role,
    ECommentaryEmotion Emotion,
    float Pressure,
    float Intensity,
    bool bIsFinalBall,
    bool bIsMatchWinning,
    bool bFollowUp)
{
    TArray<const FC26CommentaryLineDef*> Pool = FC26CommentaryLibrary::FindMatchingLines(
        Category, Role, Emotion, Pressure, Intensity, bIsFinalBall, bIsMatchWinning, bFollowUp
    );

    if (Pool.IsEmpty())
    {
        // Broaden search
        Pool = FC26CommentaryLibrary::FindMatchingLines(
            Category, Role, ECommentaryEmotion::Neutral, Pressure, Intensity, false, false, bFollowUp
        );
    }

    if (Pool.IsEmpty())
    {
        return nullptr;
    }

    // Filter out recently spoken lines (anti-repetition)
    TArray<const FC26CommentaryLineDef*> Filtered;
    for (const FC26CommentaryLineDef* L : Pool)
    {
        if (!RecentLineHistory.Contains(L->LineId))
        {
            Filtered.Add(L);
        }
    }

    const TArray<const FC26CommentaryLineDef*>& FinalPool = Filtered.IsEmpty() ? Pool : Filtered;

    // Weighted random selection
    int32 TotalWeight = 0;
    for (const FC26CommentaryLineDef* L : FinalPool)
    {
        TotalWeight += FMath::Max(1, (int32)L->Weight);
    }

    int32 Roll = FMath::RandRange(0, FMath::Max(0, TotalWeight - 1));
    for (const FC26CommentaryLineDef* L : FinalPool)
    {
        Roll -= FMath::Max(1, (int32)L->Weight);
        if (Roll < 0)
        {
            return L;
        }
    }

    return FinalPool.Last();
}

void UC26CommentaryDirector::EvaluateAndQueueEvent(const FC26CommentaryEvent& Event, FName Category, bool bForcePlay)
{
    const float Pressure = CalculatePressure(Event);
    if (!bForcePlay && !ShouldCommentate(Event, Pressure))
    {
        return;
    }

    float Intensity = 0.5f;
    const ECommentaryEmotion Emotion = SelectEmotion(Event, Pressure, Intensity);

    // Pick Lead commentator line
    const FC26CommentaryLineDef* Line = PickLine(
        Category, ECommentatorRole::Lead, Emotion, Pressure, Intensity, Event.bIsFinalBall, Event.bIsMatchWinningEvent, false
    );

    if (!Line)
    {
        return;
    }

    // Add to anti-repetition history
    RecentLineHistory.Add(Line->LineId);
    if (RecentLineHistory.Num() > MaxHistorySize)
    {
        RecentLineHistory.RemoveAt(0);
    }

    FCommentaryQueueItem Item;
    Item.LineDef = Line;
    Item.ResolvedText = Line->Text;
    Item.ElevenLabsPrompt = Line->ElevenLabsPrompt;
    Item.Role = Line->Role;
    Item.Emotion = Emotion;
    Item.Intensity = Intensity;
    Item.Pressure = Pressure;
    Item.Priority = Line->Priority;
    Item.PlayAtTime = Now() + Line->Delay;
    Item.ExpiryTime = Item.PlayAtTime + 4.5f;
    Item.DeliveryId = Event.DeliveryId;
    Item.bIsFollowUp = false;

    // Interrupt or enqueue based on priority
    if (bIsSpeaking)
    {
        if (Item.Priority >= CurrentSpeechPriority + 15)
        {
            if (AudioDirector)
            {
                AudioDirector->StopCommentary();
            }
            Queue.Reset();
            DispatchLine(Item);
            return;
        }
        else if (Item.Priority >= CurrentSpeechPriority && Queue.Num() < 2)
        {
            Queue.Add(Item);
            return;
        }
        else
        {
            return;
        }
    }

    if (Queue.IsEmpty())
    {
        DispatchLine(Item);
    }
    else if (Queue.Num() < 2)
    {
        Queue.Add(Item);
    }

    // Chance of analyst follow-up for big events
    if (Event.bIsSix || Event.bIsWicket || Event.bIsBoundary || Event.bIsMatchWinningEvent)
    {
        ScheduleAnalystFollowUp(Event, Line);
    }
}

void UC26CommentaryDirector::ScheduleAnalystFollowUp(const FC26CommentaryEvent& Event, const FC26CommentaryLineDef* LeadLine)
{
    if (!LeadLine || FMath::FRand() > 0.45f)
    {
        return;
    }

    const float Pressure = CalculatePressure(Event);
    float Intensity = 0.4f;
    const ECommentaryEmotion Emotion = ECommentaryEmotion::Analytical;

    const FC26CommentaryLineDef* AnalystLine = PickLine(
        LeadLine->Category, ECommentatorRole::Analyst, Emotion, Pressure, Intensity, Event.bIsFinalBall, Event.bIsMatchWinningEvent, true
    );

    if (!AnalystLine)
    {
        return;
    }

    FCommentaryQueueItem FollowItem;
    FollowItem.LineDef = AnalystLine;
    FollowItem.ResolvedText = AnalystLine->Text;
    FollowItem.ElevenLabsPrompt = AnalystLine->ElevenLabsPrompt;
    FollowItem.Role = ECommentatorRole::Analyst;
    FollowItem.Emotion = Emotion;
    FollowItem.Intensity = Intensity;
    FollowItem.Pressure = Pressure;
    FollowItem.Priority = AnalystLine->Priority;
    FollowItem.PlayAtTime = Now() + AnalystLine->Delay;
    FollowItem.ExpiryTime = FollowItem.PlayAtTime + 6.0f;
    FollowItem.DeliveryId = Event.DeliveryId;
    FollowItem.bIsFollowUp = true;

    if (Queue.Num() < 2)
    {
        Queue.Add(FollowItem);
    }
}

void UC26CommentaryDirector::DispatchLine(const FCommentaryQueueItem& Item)
{
    PlaySoundOrSynthesize(Item);

    LastPlayedLineId = Item.LineDef ? Item.LineDef->LineId.ToString() : TEXT("UNKNOWN");
    LastEmotion = Item.Emotion;
    LastPressure = Item.Pressure;
    LastIntensity = Item.Intensity;

    if (bDebugOverlay)
    {
        UE_LOG(LogC26, Display, TEXT("C26_DIRECTOR_DISPATCH: Line=%s Role=%s Emotion=%d Pressure=%.2f Intensity=%.2f Pri=%d"),
            *LastPlayedLineId,
            Item.Role == ECommentatorRole::Lead ? TEXT("Lead") : TEXT("Analyst"),
            (int32)Item.Emotion, Item.Pressure, Item.Intensity, Item.Priority);
    }
}

void UC26CommentaryDirector::PlaySoundOrSynthesize(const FCommentaryQueueItem& Item)
{
    const FString VoiceId = (Item.Role == ECommentatorRole::Lead) ? LeadVoiceId : AnalystVoiceId;
    const FString CacheHash = ComputeCacheHash(VoiceId, Item.ElevenLabsPrompt, Item.Emotion, Item.Intensity);
    const FString CachePath = GetCacheDirectory() / (CacheHash + TEXT(".wav"));

    // 1. Check disk cache
    if (FPaths::FileExists(CachePath))
    {
        USoundWave* CachedWave = LoadCachedWav(CachePath);
        if (CachedWave)
        {
            if (AudioDirector)
            {
                AudioDirector->PlayCommentarySound(CachedWave, Item.ResolvedText, CachedWave->GetDuration() + 0.35f);
            }
            bIsSpeaking = true;
            CurrentSpeechPriority = Item.Priority;
            SpeechCooldownUntil = Now() + FMath::Max(2.5f, CachedWave->GetDuration() + 0.35f);
            return;
        }
    }

    // 2. Check dynamic generation eligibility
    const FString ApiKey = GetSecureApiKey();
    const bool bCanDynamic = (CommentaryMode == ECommentaryMode::Dynamic) ||
        (CommentaryMode == ECommentaryMode::Hybrid && (Item.Pressure >= 0.70f || Item.LineDef->bMatchWinningOnly || Item.LineDef->bFinalBallOnly));

    if (bCanDynamic && !ApiKey.IsEmpty() && (DynamicRequestsUsed < MaxDynamicRequestsPerMatch))
    {
        RequestElevenLabsSynthesis(Item, VoiceId, CachePath);
    }

    // 3. Pre-generated fallback
    if (Item.LineDef && Item.LineDef->PreGenFile != NAME_None && AudioDirector)
    {
        const FString AssetPath = FString::Printf(TEXT("/Game/Cricket26/Audio/Commentary/%s.%s"),
            *Item.LineDef->PreGenFile.ToString(), *Item.LineDef->PreGenFile.ToString());
        USoundBase* PreGenSound = LoadObject<USoundBase>(nullptr, *AssetPath);
        if (PreGenSound)
        {
            const float Duration = FMath::Max(2.0f, PreGenSound->GetDuration() + 0.35f);
            AudioDirector->PlayCommentarySound(PreGenSound, Item.ResolvedText, Duration);
            bIsSpeaking = true;
            CurrentSpeechPriority = Item.Priority;
            SpeechCooldownUntil = Now() + Duration;
        }
    }
}

FString UC26CommentaryDirector::ComputeCacheHash(const FString& VoiceId, const FString& Text, ECommentaryEmotion Emotion, float Intensity) const
{
    const FString Composite = FString::Printf(TEXT("%s_%s_%d_%.2f"), *VoiceId, *Text, (int32)Emotion, Intensity);
    return FMD5::HashAnsiString(*Composite);
}

USoundWave* UC26CommentaryDirector::LoadCachedWav(const FString& FilePath)
{
    TArray<uint8> FileData;
    if (FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        return CreateSoundWaveFromPcm(FileData);
    }
    return nullptr;
}

USoundWave* UC26CommentaryDirector::CreateSoundWaveFromPcm(const TArray<uint8>& RawWavData)
{
    if (RawWavData.Num() < 44)
    {
        return nullptr;
    }

    USoundWave* Wave = NewObject<USoundWave>(this);
    if (!Wave)
    {
        return nullptr;
    }

    // Parse standard 44-byte RIFF WAV header
    const uint16 Channels = *reinterpret_cast<const uint16*>(&RawWavData[22]);
    const uint32 SampleRate = *reinterpret_cast<const uint32*>(&RawWavData[24]);
    const uint16 BitsPerSample = *reinterpret_cast<const uint16*>(&RawWavData[34]);

    Wave->NumChannels = Channels > 0 ? Channels : 1;
    Wave->SetSampleRate(SampleRate > 0 ? SampleRate : 44100);
    Wave->RawPCMDataSize = RawWavData.Num() - 44;

    const float Duration = (float)(Wave->RawPCMDataSize) / (float)(Wave->GetSampleRateForCurrentPlatform() * Wave->NumChannels * (BitsPerSample / 8));
    Wave->Duration = FMath::Max(1.0f, Duration);

    Wave->RawPCMData = (uint8*)FMemory::Malloc(Wave->RawPCMDataSize);
    FMemory::Memcpy(Wave->RawPCMData, RawWavData.GetData() + 44, Wave->RawPCMDataSize);
    Wave->bLooping = false;

    return Wave;
}

void UC26CommentaryDirector::RequestElevenLabsSynthesis(const FCommentaryQueueItem& Item, const FString& VoiceId, const FString& CachePath)
{
    const FString ApiKey = GetSecureApiKey();
    if (ApiKey.IsEmpty())
    {
        return;
    }

    ++DynamicRequestsUsed;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    const FString Url = FString::Printf(TEXT("https://api.elevenlabs.io/v1/text-to-speech/%s"), *VoiceId);

    HttpRequest->SetURL(Url);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("xi-api-key"), ApiKey);
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Accept"), TEXT("audio/wav"));

    // Build JSON payload
    TSharedPtr<FJsonObject> JsonPayload = MakeShared<FJsonObject>();
    JsonPayload->SetStringField(TEXT("text"), Item.ElevenLabsPrompt.IsEmpty() ? Item.ResolvedText : Item.ElevenLabsPrompt);
    JsonPayload->SetStringField(TEXT("model_id"), ElevenLabsModel);

    TSharedPtr<FJsonObject> VoiceSettings = MakeShared<FJsonObject>();
    const float Stability = (Item.Emotion == ECommentaryEmotion::Analytical) ? 0.70f : 0.45f;
    const float Similarity = 0.80f;
    const float Style = FMath::Clamp(Item.Intensity, 0.0f, 1.0f);

    VoiceSettings->SetNumberField(TEXT("stability"), Stability);
    VoiceSettings->SetNumberField(TEXT("similarity_boost"), Similarity);
    VoiceSettings->SetNumberField(TEXT("style"), Style);
    VoiceSettings->SetBoolField(TEXT("use_speaker_boost"), true);

    JsonPayload->SetObjectField(TEXT("voice_settings"), VoiceSettings);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonPayload.ToSharedRef(), Writer);

    HttpRequest->SetContentAsString(RequestBody);

    // Bind async completion callback
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, CachePath, Item](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
        {
            if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                const TArray<uint8>& AudioData = Response->GetContent();
                if (AudioData.Num() > 44)
                {
                    FFileHelper::SaveArrayToFile(AudioData, *CachePath);
                    UE_LOG(LogC26, Display, TEXT("C26_ELEVENLABS Synthesis success: saved %d bytes to %s"), AudioData.Num(), *CachePath);
                }
            }
            else
            {
                const int32 Code = Response.IsValid() ? Response->GetResponseCode() : -1;
                UE_LOG(LogC26, Warning, TEXT("C26_ELEVENLABS Synthesis failed: code=%d"), Code);
            }
        }
    );

    HttpRequest->ProcessRequest();
    UE_LOG(LogC26, Display, TEXT("C26_ELEVENLABS Request dispatched: Voice=%s DynamicUsed=%d/%d"), *VoiceId, DynamicRequestsUsed, MaxDynamicRequestsPerMatch);
}

void UC26CommentaryDirector::PumpQueue(float CurrentTime)
{
    if (bIsSpeaking || Queue.IsEmpty())
    {
        return;
    }

    int32 BestIndex = -1;
    for (int32 i = 0; i < Queue.Num(); ++i)
    {
        if (Queue[i].PlayAtTime <= CurrentTime)
        {
            if (BestIndex < 0 || Queue[i].Priority > Queue[BestIndex].Priority)
            {
                BestIndex = i;
            }
        }
    }

    if (BestIndex >= 0)
    {
        FCommentaryQueueItem Item = Queue[BestIndex];
        Queue.RemoveAt(BestIndex);
        DispatchLine(Item);
    }
}

// Gameplay hooks
void UC26CommentaryDirector::OnMatchStart()
{
    FC26CommentaryEvent Event;
    Event.EventType = ECommentaryEventType::MatchStart;
    EvaluateAndQueueEvent(Event, FName(TEXT("MATCH_START")), true);
}

void UC26CommentaryDirector::OnChaseStart(const FC26CommentaryEvent& Event)
{
    EvaluateAndQueueEvent(Event, FName(TEXT("CHASE_START")), true);
}

void UC26CommentaryDirector::OnInningsBreak(const FC26CommentaryEvent& Event)
{
    EvaluateAndQueueEvent(Event, FName(TEXT("INNINGS_BREAK")), true);
}

void UC26CommentaryDirector::OnPreBall(const FC26CommentaryEvent& Event)
{
    FName Category = FName(TEXT("PRE_BALL"));
    if (Event.bIsFinalBall)
    {
        Category = FName(TEXT("FINAL_BALL"));
    }
    else if (Event.MatchPressure >= 0.70f)
    {
        Category = FName(TEXT("PRESSURE"));
    }
    EvaluateAndQueueEvent(Event, Category, false);
}

void UC26CommentaryDirector::OnDeliveryStruck(const FC26CommentaryEvent& Event)
{
    FName Category = FName(TEXT("DELIVERY"));
    if (Event.EventType == ECommentaryEventType::Edge)
    {
        Category = FName(TEXT("EDGE"));
    }
    EvaluateAndQueueEvent(Event, Category, false);
}

void UC26CommentaryDirector::OnBallCompleted(const FC26CommentaryEvent& Event)
{
    FName Category = FName(TEXT("RUNS"));
    if (Event.bIsSix)
    {
        Category = FName(TEXT("SIX"));
    }
    else if (Event.bIsBoundary)
    {
        Category = FName(TEXT("FOUR"));
    }
    else if (Event.RunsScored <= 0)
    {
        Category = FName(TEXT("DOT"));
    }
    EvaluateAndQueueEvent(Event, Category, Event.bIsSix || Event.bIsBoundary);
}

void UC26CommentaryDirector::OnWicket(const FC26CommentaryEvent& Event)
{
    FName Category = FName(TEXT("WICKET"));
    if (Event.DismissalType == 1)
    {
        Category = FName(TEXT("BOWLED"));
    }
    else if (Event.DismissalType == 2)
    {
        Category = FName(TEXT("CAUGHT"));
    }
    EvaluateAndQueueEvent(Event, Category, true);
}

void UC26CommentaryDirector::OnBoundary(const FC26CommentaryEvent& Event)
{
    EvaluateAndQueueEvent(Event, Event.bIsSix ? FName(TEXT("SIX")) : FName(TEXT("FOUR")), true);
}

void UC26CommentaryDirector::OnMatchResult(const FC26CommentaryEvent& Event)
{
    FName Category = FName(TEXT("MATCH_WIN"));
    if (Event.EventType == ECommentaryEventType::MatchLoss)
    {
        Category = FName(TEXT("MATCH_LOSS"));
    }
    else if (Event.EventType == ECommentaryEventType::MatchTie)
    {
        Category = FName(TEXT("MATCH_TIE"));
    }
    EvaluateAndQueueEvent(Event, Category, true);
}

void UC26CommentaryDirector::TestScenario(ECommentaryScenario Scenario)
{
    FC26CommentaryEvent Event;
    Event.BatterName = TEXT("Rohit");
    Event.BowlerName = TEXT("Starc");

    switch (Scenario)
    {
    case ECommentaryScenario::RoutineDot:
        Event.EventType = ECommentaryEventType::Dot;
        Event.RunsScored = 0;
        Event.InningsNumber = 0;
        Event.BallNumber = 2;
        Event.MatchPressure = 0.25f;
        EvaluateAndQueueEvent(Event, FName(TEXT("DOT")), true);
        break;

    case ECommentaryScenario::NormalFour:
        Event.EventType = ECommentaryEventType::Four;
        Event.RunsScored = 4;
        Event.bIsBoundary = true;
        Event.InningsNumber = 0;
        Event.BallNumber = 3;
        Event.MatchPressure = 0.35f;
        EvaluateAndQueueEvent(Event, FName(TEXT("FOUR")), true);
        break;

    case ECommentaryScenario::NormalSix:
        Event.EventType = ECommentaryEventType::Six;
        Event.RunsScored = 6;
        Event.bIsSix = true;
        Event.bIsBoundary = true;
        Event.InningsNumber = 0;
        Event.BallNumber = 4;
        Event.MatchPressure = 0.40f;
        EvaluateAndQueueEvent(Event, FName(TEXT("SIX")), true);
        break;

    case ECommentaryScenario::HighPressureSix:
        Event.EventType = ECommentaryEventType::Six;
        Event.RunsScored = 6;
        Event.bIsSix = true;
        Event.bIsBoundary = true;
        Event.InningsNumber = 1;
        Event.Target = 18;
        Event.RunsRequired = 8;
        Event.BallsRemaining = 2;
        Event.MatchPressure = 0.85f;
        EvaluateAndQueueEvent(Event, FName(TEXT("SIX")), true);
        break;

    case ECommentaryScenario::WicketBowled:
        Event.EventType = ECommentaryEventType::Bowled;
        Event.bIsWicket = true;
        Event.DismissalType = 1;
        Event.InningsNumber = 1;
        Event.Wickets = 1;
        Event.MatchPressure = 0.80f;
        EvaluateAndQueueEvent(Event, FName(TEXT("BOWLED")), true);
        break;

    case ECommentaryScenario::WicketCaught:
        Event.EventType = ECommentaryEventType::Caught;
        Event.bIsWicket = true;
        Event.DismissalType = 2;
        Event.InningsNumber = 0;
        Event.MatchPressure = 0.50f;
        EvaluateAndQueueEvent(Event, FName(TEXT("CAUGHT")), true);
        break;

    case ECommentaryScenario::FinalBallTense:
        Event.EventType = ECommentaryEventType::FinalBall;
        Event.InningsNumber = 1;
        Event.Target = 16;
        Event.RunsRequired = 4;
        Event.BallsRemaining = 1;
        Event.bIsFinalBall = true;
        Event.MatchPressure = 0.95f;
        EvaluateAndQueueEvent(Event, FName(TEXT("FINAL_BALL")), true);
        break;

    case ECommentaryScenario::MatchWinningSix:
        Event.EventType = ECommentaryEventType::Six;
        Event.RunsScored = 6;
        Event.bIsSix = true;
        Event.bIsBoundary = true;
        Event.bIsMatchWinningEvent = true;
        Event.InningsNumber = 1;
        Event.Target = 14;
        Event.RunsRequired = 2;
        Event.BallsRemaining = 1;
        Event.bIsFinalBall = true;
        Event.MatchPressure = 0.99f;
        EvaluateAndQueueEvent(Event, FName(TEXT("SIX")), true);
        break;

    default:
        break;
    }
}
