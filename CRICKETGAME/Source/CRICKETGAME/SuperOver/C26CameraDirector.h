#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26Types.h"
#include "C26CameraDirector.generated.h"
class UCameraComponent;
class AC26Athlete;
struct FC26ReplayFrame
{
    float Time=0;
    FVector Ball;
    TArray<FTransform> Actors;
    TArray<EC26Action> Actions;
    TArray<float> ActionTimes;
};
UCLASS()
class CRICKETGAME_API AC26CameraDirector : public AActor
{
    GENERATED_BODY()
public:
    AC26CameraDirector();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    void Reset();
    void Direct(EC26Phase Phase,float PhaseTime,bool PlayerBatting,const FVector& Ball,const FVector& Velocity,bool Aerial);
    void Record(float Dt,const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors);
    bool BeginReplay();
    bool PlayReplay(float Dt,FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors);
    void Restore(const TArray<TObjectPtr<AC26Athlete>>& Actors);
    bool IsReplaying=false;
    float ReplayClock=0;
private:
    TArray<FC26ReplayFrame> Frames;
    FC26ReplayFrame Live;
    float RecordClock=0,RecordAccumulator=0;
    EC26Phase LastPhase=EC26Phase::Result;
    void Look(const FVector& From,const FVector& At,float Fov,bool Cut,float Dt);
};
