#include "C26StadiumAmbienceComponent.h"
#include "Sound/SoundAttenuation.h"

UC26StadiumAmbienceComponent::UC26StadiumAmbienceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UC26StadiumAmbienceComponent::Initialize()
{
    if (!FieldAttenuation)
    {
        FieldAttenuation = NewObject<USoundAttenuation>(this);
        FieldAttenuation->Attenuation.bAttenuate = true;
        FieldAttenuation->Attenuation.bSpatialize = true;
        FieldAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        FieldAttenuation->Attenuation.AttenuationShapeExtents = FVector(12000.f);
        FieldAttenuation->Attenuation.dBAttenuationAtMax = -16.f;
    }
}
