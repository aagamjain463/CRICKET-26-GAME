#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26CharacterProfile.h"
#include "C26CharacterPresentationComponent.generated.h"
class AC26Athlete;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class ACameraActor;

/** Uses measured displacement, including SetActorLocation; zero-time contact samples cannot
    advance locomotion and ResetAt cannot masquerade as a 100m sprint. */
struct CRICKETGAME_API FC26LocomotionSample
{
    FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector,Acceleration=FVector::ZeroVector;
    float Yaw=0,GroundSpeed=0,Direction=0,TurnRate=0,Distance=0;
    bool Initialized=false,Teleported=false;
    void Reset(const FTransform& Transform);
    void Update(const FTransform& Transform,float Dt);
};

struct FC26CharacterPoseSample
{
    FName State,PreviousState;
    float Time=0,PreviousTime=0,Alpha=1,Distance=0,Clock=0,GroundSpeed=0;
};

UCLASS(ClassGroup=(Cricket26),meta=(BlueprintSpawnableComponent))
class CRICKETGAME_API UC26CharacterPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UC26CharacterPresentationComponent();
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UC26CharacterProfile> Profile;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FC26PlayerAppearance Appearance;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EC26VisualRole VisualRole=EC26VisualRole::Fielder;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) EC26CharacterQuality Quality=EC26CharacterQuality::High;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName CurrentState;
    UPROPERTY() TMap<EC26EquipmentSlot,TObjectPtr<UStaticMeshComponent>> Equipment;
    /** No side effects on existing presentation unless all structural AND approval gates pass. */
    bool TryActivate(AC26Athlete* Athlete);
    bool IsActive() const{return bActive;}
    void Configure(AC26Athlete* Athlete);
    void UpdateFromMatch(AC26Athlete* Athlete,float Dt);
    void ResetMotion();
    void SetQualityForView(const FVector& ViewPoint);
    FVector BallHandPosition() const;
    FVector ReceivePosition() const;
    UFUNCTION(BlueprintCallable) void ApplyVisualRole(EC26VisualRole NewRole);
    UFUNCTION(BlueprintCallable) bool ValidateRuntime() const;
    FC26CharacterPoseSample CapturePose() const;
    void ApplyReplayPose(const FC26CharacterPoseSample& A,const FC26CharacterPoseSample& B,float Alpha);
    UStaticMeshComponent* GetBat() const;
    const FC26LocomotionSample& GetLocomotion() const{return Locomotion;}
private:
    bool bActive=false,bWasMoving=false;
    FC26LocomotionSample Locomotion;
    const FC26CricketClip* CurrentClip=nullptr;
    float Clock=0,BlendClock=0,TransitionAge=0;
    FName Transition,PreviousState;
    bool bDebugRole=false;
    TSet<FName> ReportedMissing;
    FVector LastLeftFoot=FVector::ZeroVector,LastRightFoot=FVector::ZeroVector;
    float FrozenSeconds=0;
    float WarpPrevTime=0.f;
    void UpdateWarp(const AC26Athlete* Athlete,const FC26CricketClip* Clip);
    void LearnWarp(const AC26Athlete* Athlete);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    TMap<FName,int32> ReviewSamples;
    float ReviewTime=0,ReviewLastCapture=-1;
    void HideLegacy(AC26Athlete* Athlete);
    void DressEquipment(int32 TeamId);
    static int32 Dress(UStaticMeshComponent* Part,const TCHAR* Key,UMaterialInstanceDynamic* M);
    FName ReadyKey() const;
    FName SelectState(const AC26Athlete* Athlete,float Dt);
    void Debug(const AC26Athlete* Athlete,float Dt);
};
