#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "C26AnimNotifies.generated.h"

/** Contact and release markers for the authored cricket clips.
 *
 *  UAnimNotify::GetNotifyName_Implementation strips the "AnimNotify_" prefix from the class name,
 *  so FAnimNotifyEvent::NotifyName comes out as "BatContact", "BallRelease" and so on - exactly the
 *  keys UC26CharacterProfile::Validate looks for. A notify added with no class is named NAME_None,
 *  which is why these exist as real classes rather than bare named markers.
 *
 *  They are presentation markers only. The simulation decides the outcome and the presentation
 *  component warps clip time onto these instants; nothing here feeds back into scoring.
 */
UCLASS(meta=(DisplayName="C26 Bat Contact"))
class CRICKETGAME_API UAnimNotify_BatContact : public UAnimNotify
{
    GENERATED_BODY()
};

UCLASS(meta=(DisplayName="C26 Ball Release"))
class CRICKETGAME_API UAnimNotify_BallRelease : public UAnimNotify
{
    GENERATED_BODY()
};

UCLASS(meta=(DisplayName="C26 Pickup"))
class CRICKETGAME_API UAnimNotify_Pickup : public UAnimNotify
{
    GENERATED_BODY()
};

UCLASS(meta=(DisplayName="C26 Throw Release"))
class CRICKETGAME_API UAnimNotify_ThrowRelease : public UAnimNotify
{
    GENERATED_BODY()
};

UCLASS(meta=(DisplayName="C26 Catch"))
class CRICKETGAME_API UAnimNotify_Catch : public UAnimNotify
{
    GENERATED_BODY()
};
