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

struct FC26FootLockState
{
    bool bLocked = false;
    FVector LockedWorldPos = FVector::ZeroVector;
    float LockAlpha = 0.f;
};

/** Shared presentation tuning and the pure rules behind it. Declared here so each number has one
    home and the rules can be asserted without a live athlete -- SelectState and the orientation
    filter are the only runtime callers. */
namespace C26Presentation
{
    /** Locomotion hysteresis: entering costs more speed than leaving it keeps. */
    inline constexpr float StartSpeed=22.f,StopSpeed=10.f;
    /** Hip-to-ankle reach; a lock is released before the solver has to straighten past this. */
    inline constexpr float LegReach=86.f;
    /** Authored ankle clearance that counts as the stride having genuinely lifted the foot. */
    inline constexpr float LiftHeight=8.f;
    /** A re-aim larger than this inside one frame is a snap, not a turn the athlete ran through. */
    inline constexpr float OrientationSnapDegrees=15.f;
    /** Bound on the mesh yaw lag, so even a 180 degree re-aim cannot spin the body. */
    inline constexpr float MaxMeshYawLag=135.f;
    /** Shortest blend the presentation will run; below this the switch is a visible pop. */
    inline constexpr float MinBlendSeconds=.02f;

    /** True when the athlete should be in locomotion. The band between StopSpeed and StartSpeed
        holds whatever state the athlete is already in, so a speed hovering at the boundary cannot
        flap idle<->locomotion and restart Start/Stop every few frames. */
    CRICKETGAME_API bool WantsMove(bool bWasMoving,float GroundSpeed);
    /** True once a Start/Stop clip has done its job and should hand back to locomotion or stance.
        A turn clip always plays out. */
    CRICKETGAME_API bool TransitionSpent(FName Transition,float GroundSpeed,float TransitionAge);
    /** One frame of mesh-only yaw lag: absorbs an authoritative re-aim discontinuity, then unwinds
        it. Returns the new lag in degrees; the actor rotation is never involved. */
    CRICKETGAME_API float StepMeshYawOffset(float Offset,float AuthoritativeYawDelta,float Dt);
    /** Advances the outgoing pose while a blend is running. Freezing it at the switch frame reads
        as the body stalling for the whole blend, which is what an action->recovery hand-back
        looked like; the outgoing clip keeps playing until it is inaudible. */
    CRICKETGAME_API float AdvanceOutgoingPose(float PreviousTime,float PreviousLength,float Dt);
    /** Weight of the incoming clip after BlendClock seconds of a BlendSeconds blend. Smoothstep,
        so the pose leaves and arrives with zero velocity instead of snapping at both ends. */
    CRICKETGAME_API float BlendWeight(float BlendClock,float BlendSeconds);
}

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
    /** Whether the shared foundation may take a foot mark in this state. Public so the scope
        boundary is assertable: only the shared locomotion and stance clips are stabilized, and
        the batting/bowling/fielding action clips another agent owns are left untouched. */
    static bool StateAllowsFootLock(FName State);
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

    /** Foot stabilization: a planted foot is pinned to the world point it was planted at,
        so the animated stride cannot drag it across the pitch. */
    FC26FootLockState LeftFootLock,RightFootLock;
    float PelvisCompensationZ=0.f;
    /** Cached ground plane under the athlete; re-traced only when it can have changed. */
    FVector GroundProbePos=FVector::ZeroVector;
    float GroundProbeZ=0.f,GroundProbeAge=1.f;
    /** Standing ankle height measured from this mesh's own reference pose, not assumed. */
    float RefAnkleHeight=-1.f;
    /** Mesh-only yaw lag absorbing authoritative-rotation snaps. Gameplay rotation is untouched. */
    float MeshYawOffset=0.f,LastAuthoritativeYaw=0.f;
    bool bInitializedYaw=false;
    /** One-shot so the reason the stabilizer is skipped is visible in a log, not just on screen. */
    bool bLoggedFootSkip=false;
    /** Continuous stride phase in seconds, advanced by distance travelled, so a clip change
        (Walk->Run) resumes the cycle instead of jumping to a new absolute time. */
    float LocomotionPhase=0.f;

    void UpdateFootStabilization(const AC26Athlete* Athlete,float Dt);
    void UpdateOrientationSmoothing(const AC26Athlete* Athlete,float Dt);

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
