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
    /** Selected before activation; changing this never bypasses profile validation/approval.
        The default path and development command-line override retain their existing behavior. */
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="C26|Presentation") TSoftObjectPtr<UC26CharacterProfile> ProfileAsset;
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
    bool IsRecovered() const{return bRecovered;}
    float GetReactionClock() const{return ReactionClock;}
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
    /** One-shot variant (running pickup, dive side, catch height...) chosen when the action starts and
        held until it ends, so a clip never swaps mid-action as the ball or the athlete moves. */
    EC26Action LatchedAction=EC26Action::Ready;
    FName LatchedKey;
    float EntrySpeed=0.f,LastActionTime=0.f;
    /** Round 7 presentation layer. A one-shot (stroke, delivery, reaction, signal) that has played out hands
        back to the ready loop instead of freezing on its last frame; latched until the action changes. */
    bool bRecovered=false;
    /** Clip time of the reaction being shown, after this athlete's beat and temperament rate; <0 otherwise. */
    float ReactionClock=-1.f;
    float ActiveBlend=.12f,PreviousActionTime=0.f;
    /** Temperament-driven presentation (never gameplay): reaction playback rate, extra beat, idle amplitude. */
    float StyleRate=1.f,StyleDelay=0.f,StyleLife=1.f,StyleLookSpeed=4.f;
    float BreathWeight=0,SwayWeight=0,LookWeight=0,LeanWeight=0,BreathPhase=0,SwayPhase=0,BreathRate=1.f;
    FVector2D Look=FVector2D::ZeroVector,Lean=FVector2D::ZeroVector,LeanVelocity=FVector2D::ZeroVector;
    FVector SmoothedAcceleration=FVector::ZeroVector;
    FName ReactionState(const AC26Athlete* Athlete);
    FName IdleState(const AC26Athlete* Athlete,float Dt);
    void UpdateLife(const AC26Athlete* Athlete,float Dt);
    FName Variant(const AC26Athlete* Athlete);
    void UpdateWarp(const AC26Athlete* Athlete,const FC26CricketClip* Clip);
    void LearnWarp(const AC26Athlete* Athlete);
    /** Bat control inputs for the anim graph: grip lock for anyone holding a bat, and the reach that puts the
        blade on the simulated contact point, measured once per stroke from the clip's own contact pose. */
    void UpdateBatControl(const AC26Athlete* Athlete);
    /** Component-space socket transform of one clip frame, evaluated without blend, life or bat control. */
    FTransform PosedSocket(UAnimSequence* Sequence,float Time,FName Socket);
    FTransform EquipmentOffset(const FC26EquipmentDefinition& Item);
    TOptional<FTransform> LeftBatOffset;
    const FC26CricketClip* ReachClip=nullptr;
    FVector ReachTarget=FVector::ZeroVector;
    uint8 ReachTiming=0xFF;
    float GripLastReport=-1;
    int32 QualityTier=0,EvaluationCounter=0;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReviewCamera;
    TMap<FName,int32> ReviewSamples;
    float ReviewTime=0,ReviewLastCapture=-1;
    void HideLegacy(AC26Athlete* Athlete);
    void AssignBodyMesh(USkeletalMesh* Model);
    void ApplyMaterialOverrides();
    void ApplyBodyMaterials(int32 Team);
    void RefreshEquipmentAttachments();
    void DressEquipment(int32 TeamId);
    static int32 Dress(UStaticMeshComponent* Part,const TCHAR* Key,UMaterialInstanceDynamic* M);
    FName ReadyKey() const;
    FName SelectState(const AC26Athlete* Athlete,float Dt);
    void Debug(const AC26Athlete* Athlete,float Dt);
};
