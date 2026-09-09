#include "C26Audio.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/Actor.h"
void UC26Audio::Initialize()
{
    for(const TCHAR* N:{TEXT("bat_sweet_spot"),TEXT("bat_edge"),TEXT("bat_defensive"),TEXT("ball_bounce"),TEXT("stump_hit"),TEXT("crowd_ambience"),TEXT("crowd_four"),TEXT("crowd_six"),TEXT("wicket_roar"),TEXT("ui_button_click"),TEXT("keeper_catch"),TEXT("ui_result_sting"),TEXT("fielder_gather")})
        if(auto* S=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Cricket26/Audio/%s.%s"),N,N)))Sounds.Add(N,S);
    Ambience=NewObject<UAudioComponent>(GetOwner());Ambience->bAutoActivate=false;Ambience->bIsUISound=true;Ambience->RegisterComponent();
    if(auto* S=Sounds.Find(TEXT("crowd_ambience"))){Ambience->SetSound(*S);Ambience->SetVolumeMultiplier(.18f*Master);Ambience->Play();}
    for(int I=0;I<8;++I){auto* C=NewObject<UAudioComponent>(GetOwner());C->bAutoActivate=false;C->bIsUISound=true;C->RegisterComponent();Channels.Add(C);}
}
void UC26Audio::Cue(FName Name,float Volume)
{
    auto* S=Sounds.Find(Name);
    if(!S||Channels.IsEmpty())return;
    auto C=Channels[NextChannel++%Channels.Num()];
    // Every hard contact in the game -- bat, bounce, stumps, gloves -- gets its own pitch and gain
    // so a six-ball over never plays the identical transient twice. Crowd and UI stay unvaried,
    // because a wobbling ambience bed or a detuned button is immediately noticeable as a defect.
    const FString Key=Name.ToString();
    const bool Impact=Key.StartsWith(TEXT("bat"))||Key.StartsWith(TEXT("ball"))
        ||Key.StartsWith(TEXT("stump"))||Key.StartsWith(TEXT("keeper"));
    C->Stop();C->SetSound(*S);
    C->SetVolumeMultiplier(Volume*Master*(Impact?FMath::FRandRange(.91f,1.09f):1.f));
    C->SetPitchMultiplier(Impact?FMath::FRandRange(.93f,1.07f):1.f);
    C->Play();
}
void UC26Audio::SetTension(float Amount,float Volume){Master=Volume;if(Ambience)Ambience->AdjustVolume(.8f,(.16f+Amount*.17f)*Volume);}
void UC26Audio::Reset(){for(auto C:Channels)C->Stop();NextChannel=0;SetTension(0,Master);}
