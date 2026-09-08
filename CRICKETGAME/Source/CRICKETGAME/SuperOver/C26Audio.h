#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26Audio.generated.h"
class UAudioComponent;
class USoundBase;
UCLASS(ClassGroup=(Cricket))
class CRICKETGAME_API UC26Audio : public UActorComponent
{
    GENERATED_BODY()
public:
    void Initialize();
    void Cue(FName Name,float Volume=1.f);
    void SetTension(float Amount,float Volume);
    void Reset();
    float Master=.75f;
private:
    UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Sounds;
    UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Channels;
    int NextChannel=0;
};
