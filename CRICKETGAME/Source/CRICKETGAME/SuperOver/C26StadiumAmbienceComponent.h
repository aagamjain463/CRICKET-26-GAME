#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C26StadiumAmbienceComponent.generated.h"

class USoundAttenuation;

/**
 * 3D stadium room tone and spatial broadcast field mic component.
 */
UCLASS(ClassGroup=(Cricket))
class CRICKETGAME_API UC26StadiumAmbienceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UC26StadiumAmbienceComponent();

    void Initialize();
    USoundAttenuation* GetFieldAttenuation() const { return FieldAttenuation; }

private:
    UPROPERTY() TObjectPtr<USoundAttenuation> FieldAttenuation = nullptr;
};
