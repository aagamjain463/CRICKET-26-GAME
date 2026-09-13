#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "C26CharacterReviewMode.generated.h"

/** Isolated development map; never selects or approves a match profile. */
UCLASS()
class CRICKETGAME_API AC26CharacterReviewMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AC26CharacterReviewMode();
    virtual void Tick(float Dt) override;
private:
    float Age=0;
    int32 Frame=-1;
};
