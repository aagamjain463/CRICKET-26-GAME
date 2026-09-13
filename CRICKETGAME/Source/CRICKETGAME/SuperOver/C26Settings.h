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
    /** Broadcast session: 0 = Clear Day, 1 = Late Afternoon, 2 = Night (default). */
    UPROPERTY() int32 EnvironmentProfile=2;
    /** Prepared surface: 0 = Fresh, 1 = Used (default), 2 = Dry, 3 = Worn. */
    UPROPERTY() int32 PitchCondition=1;
    UPROPERTY() float SoundVolume=.75f;
    UPROPERTY() float MusicVolume=.5f;
    UPROPERTY() float CommentaryVolume=.95f;
    UPROPERTY() float CrowdVolume=.85f;
    UPROPERTY() float SFXVolume=.9f;
    UPROPERTY() float UIVol=.8f;
    UPROPERTY() bool Vibration=true;
    UPROPERTY() bool Hints=true;
    UPROPERTY() bool Subtitles=true;
    UPROPERTY() bool ReducedMotion=false;
    UPROPERTY() float Sensitivity=1.f;
    UPROPERTY() int32 ControlScheme=0; // 0 = GesturePro, 1 = Legacy
    UPROPERTY() bool LeftHandedUI=false;
    /** Striker bats left-handed: mirrors the batting gesture so off side stays off side. */
    UPROPERTY() bool LeftHandedBatter=false;
    void Save();
    void Apply();
    static UC26Settings* Load();
};
