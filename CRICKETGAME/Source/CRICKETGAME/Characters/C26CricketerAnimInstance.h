#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "C26CricketerAnimInstance.generated.h"
class UAnimSequence;

/** Subtle procedural life layered over the authored pose: breathing, a slow weight sway, head/eye aim
    and torso follow-through inertia. Degrees, already weighted. The presentation component zeroes it for
    every gameplay action, so contact, release and gather poses are exactly the authored ones. */
struct FC26SecondaryMotion
{
    float Breath=0,Sway=0;              // weights 0..1
    float BreathPhase=0,SwayPhase=0;    // radians, advanced on the game thread
    float BreathDegrees=.9f,SwayDegrees=.7f;
    float LookYaw=0,LookPitch=0;        // + right, + up
    float LeanPitch=0,LeanRoll=0;       // + forward, + right
    bool IsZero() const{return Breath<=0&&Sway<=0&&LookYaw==0&&LookPitch==0&&LeanPitch==0&&LeanRoll==0;}
};

/** Two-handed bat control, applied after the clip blend (see FC26BatNode): hands driven from the clip's keys so
    interpolation between keys cannot pull them off the handle, and a reach offset around one clip's contact
    frame so the blade meets the simulated ball. */
struct FC26BatControl
{
    bool bGripLock=false,bLeftHandTop=true;
    TObjectPtr<UAnimSequence> ReachSequence=nullptr;
    float ReachTime=0;
    FVector Reach=FVector::ZeroVector;   // component space, cm
};

/** Native animation graph: previous full-body clip -> current full-body clip, with a smooth
    entry blend controlled by the presentation component, then the secondary-motion layer.
    Authored sequences own the entire pose. Animation time represents simulation time. It never
    changes scoring, movement or ball physics. */
UCLASS(Transient, Blueprintable)
class CRICKETGAME_API UC26CricketerAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UAnimSequence> PreviousSequence;
    UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UAnimSequence> CurrentSequence;
    UPROPERTY(Transient, BlueprintReadOnly) float PreviousTime=0;
    UPROPERTY(Transient, BlueprintReadOnly) float CurrentTime=0;
    UPROPERTY(Transient, BlueprintReadOnly) float BlendAlpha=1;
    UPROPERTY(Transient, BlueprintReadOnly) float GroundSpeed=0;
    UPROPERTY(Transient, BlueprintReadOnly) float MovementDirection=0;
    UPROPERTY(Transient, BlueprintReadOnly) FVector Acceleration=FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly) float TurnRate=0;
    UPROPERTY(Transient, BlueprintReadOnly) FName State;
    FC26SecondaryMotion Life;
    FC26BatControl BatControl;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
