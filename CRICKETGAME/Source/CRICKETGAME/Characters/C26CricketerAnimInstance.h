#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "C26CricketerAnimInstance.generated.h"
class UAnimSequence;

/** Native animation graph: previous full-body clip -> current full-body clip, with a smooth
    entry blend controlled by the presentation component. Authored sequences own the entire pose.
    Animation time represents simulation time. It never changes scoring, movement or ball physics. */
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

    /** Weight of foot stabilization and ground conforming (0 = disabled, 1 = full). */
    UPROPERTY(Transient, BlueprintReadOnly) float FootIKWeight = 1.f;

    /** Vertical offset applied to the pelvis in component space for stance conforming & knee relief. */
    UPROPERTY(Transient, BlueprintReadOnly) float PelvisOffsetZ = 0.f;

    /** Left foot lock weight and component-space target position. */
    UPROPERTY(Transient, BlueprintReadOnly) float LeftFootLockAlpha = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) FVector LeftFootTargetCS = FVector::ZeroVector;

    /** Right foot lock weight and component-space target position. */
    UPROPERTY(Transient, BlueprintReadOnly) float RightFootLockAlpha = 0.f;
    UPROPERTY(Transient, BlueprintReadOnly) FVector RightFootTargetCS = FVector::ZeroVector;

    /** Last evaluated ankle positions BEFORE stabilization, published back for the component.
        Reading the rendered skeleton instead would feed the solver its own correction, and a
        held foot could then never be seen to lift. Valid only while bAnimatedFeetValid. */
    UPROPERTY(Transient, BlueprintReadOnly) FVector AnimatedLeftFootCS = FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly) FVector AnimatedRightFootCS = FVector::ZeroVector;
    UPROPERTY(Transient, BlueprintReadOnly) bool bAnimatedFeetValid = false;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
