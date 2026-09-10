#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26Types.h"
#include "C26CameraDirector.generated.h"
class UCameraComponent;
class UStaticMeshComponent;
class AC26Athlete;

UENUM(BlueprintType)
enum class EC26CameraMode : uint8
{
    Establishing, PreDeliveryBroadcast, BatterGameplay, BowlerGameplay, BowlerRunup,
    Release, BatContact, GroundShotTracking, LoftedShotTracking, BoundaryTracking,
    Catch, RunOut, Wicket, Running, ReplayPitch, ReplayClose, ReplayBoundary,
    Celebration, InningsTransition, MatchResult
};

/** All lenses live in the director. Phase logic supplies context, never view transforms. */
USTRUCT(BlueprintType)
struct FC26BroadcastRig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Eye=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Aim=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="20",ClampMax="80")) float FOV=48.f;
};

struct FC26ReplayAthlete
{
    FTransform Transform;
    EC26Action Action=EC26Action::Ready;
    EC26Delivery DeliveryStyle=EC26Delivery::Pace;
    float ActionTime=0,MotionTime=0,ShotAngle=0,Footwork=0,Stride=0,MoveSpeed=0,Gait=0,Trigger=0;
    bool Loft=false,Defend=false;
    FVector Contact=FVector::ZeroVector,LookAt=FVector::ZeroVector;
};
struct FC26ReplayFrame
{
    float Time=0;
    FVector Ball=FVector::ZeroVector;
    TArray<FC26ReplayAthlete> Athletes;
    TArray<FTransform> Props;
};

UCLASS()
class CRICKETGAME_API AC26CameraDirector : public AActor
{
    GENERATED_BODY()
public:
    AC26CameraDirector();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(BlueprintReadOnly) EC26CameraMode Mode=EC26CameraMode::Establishing;
    UPROPERTY(EditAnywhere,Category="Broadcast|Gameplay") FC26BroadcastRig BattingRig;
    UPROPERTY(EditAnywhere,Category="Broadcast|Gameplay") FC26BroadcastRig BowlingRig;
    UPROPERTY(EditAnywhere,Category="Broadcast|Gameplay") FC26BroadcastRig ReleaseRig;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ReplayProps;
    void Reset();
    void Direct(EC26Phase Phase,float PhaseTime,bool PlayerBatting,const FVector& Ball,const FVector& Velocity,bool Aerial,float Dt=1.f/60.f);
    /** Live fielding context. The shot cameras bias toward a committed interceptor and cut to a
        square angle while the batters are running, exactly as a broadcast director would. */
    void SetFieldingTarget(const FVector& Position,bool HasTarget,bool RunnersActive=false);
    void MarkContact(float Quality,bool Aerial,const FVector& Where);
    void MarkRelease(){ReleasePending=true;}
    void MarkOutcome(FName Event,const FVector& Focus);
    void Record(float Dt,const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors);
    bool BeginReplay(const TArray<TObjectPtr<AC26Athlete>>& Actors,const FVector& Ball);
    bool PlayReplay(float Dt,FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors);
    void Restore(const TArray<TObjectPtr<AC26Athlete>>& Actors);
    FVector RestoredBall() const{return Live.Ball;}
    /** Slow-motion factor the HUD prints on the replay badge. */
    float ReplaySpeed() const{return PlaybackRate;}
    bool IsReplaying=false;
    float ReplayClock=0,PlaybackRate=1;
private:
    TArray<FC26ReplayFrame> Frames;
    FC26ReplayFrame Live;
    float RecordClock=0,RecordAccumulator=0,ContactStamp=-1,ReplayEnd=0,Impulse=0,Shake=0;
    int32 ReplayShot=0;
    bool HaveCamera=false,HasFielder=false,ShotAerial=false,Runners=false;
    bool ContactPending=false,OutcomePending=false;
    bool ReleasePending=false;
    float ReleaseStamp=-1.f;
    FVector SmoothedAim=FVector::ZeroVector,Fielder=FVector::ZeroVector,EventFocus=FVector::ZeroVector,ContactPoint=FVector::ZeroVector;
    FName EventName;
    EC26Phase LastPhase=EC26Phase::Result;
    FC26ReplayFrame CaptureState(const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors) const;
    void ApplyFrame(const FC26ReplayFrame& A,const FC26ReplayFrame& B,float T,const TArray<TObjectPtr<AC26Athlete>>& Actors);
    void Look(EC26CameraMode NewMode,const FVector& From,const FVector& At,float Fov,bool Cut,float Dt,float TrackRate=7.f,float MaxDrop=90.f);
};
