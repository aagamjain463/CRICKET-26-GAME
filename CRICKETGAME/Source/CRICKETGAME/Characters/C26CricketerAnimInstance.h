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
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
