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
    /** -C26ShotReview: every authored stroke on the real batter profile (body + fitted equipment). */
    void TickShotReview(class ASkeletalMeshActor* Actor);
    float Age=0;
    int32 Frame=-1;
    int32 ShotStep=-1;
    int32 MeasuredShot=-1;
    TArray<TObjectPtr<class UStaticMeshComponent>> ReviewGear;
    FString ShotReport;
};
