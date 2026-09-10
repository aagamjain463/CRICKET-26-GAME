#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "C26Settings.generated.h"
UCLASS()
class CRICKETGAME_API UC26Settings : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Difficulty=1;
    UPROPERTY() int32 Quality=2;
    UPROPERTY() float SoundVolume=.75f;
    UPROPERTY() float MusicVolume=.5f;
    UPROPERTY() float CommentaryVolume=.95f;
    UPROPERTY() float CrowdVolume=.85f;
    UPROPERTY() float SFXVolume=.9f;
    UPROPERTY() float UIVol=.8f;
    UPROPERTY() bool Vibration=true;
    UPROPERTY() bool Hints=true;
    UPROPERTY() float Sensitivity=1.f;
    void Save();
    void Apply();
    static UC26Settings* Load();
};
