#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26Effects.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/** One short-lived camera-facing billboard. The whole system is integrated in C++ and rebuilt into
    a single mesh section each frame, so a match's worth of dust costs one draw call and carries no
    Niagara or Cascade dependency onto mobile. */
struct FC26Puff
{
    FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
    FLinearColor Colour=FLinearColor::White;
    float Age=0,Life=0,Size=0,Growth=0,Drag=0,Gravity=0,Peak=1.f;
};

UCLASS()
class CRICKETGAME_API AC26Effects : public AActor
{
    GENERATED_BODY()
public:
    AC26Effects();
    /** Dry dust where the ball pitches. Strength scales with delivery speed. */
    void PitchDust(const FVector& At,float Strength);
    /** Landing dust under the bowler's braced front foot. */
    void FootPlant(const FVector& At,float Strength);
    /** Turf scuffed along a direction by a skidding ball or a sliding fielder. */
    void TurfScuff(const FVector& At,const FVector& Along,float Strength);
    /** Sharp bright burst when the ball breaks the stumps. */
    void StumpBurst(const FVector& At);
    /** Whisper-thin wake behind a ball travelling at pace. Called every frame while fast; the
        system itself throttles so the trail stays subliminal, never an arcade streak. */
    void BallStreak(const FVector& At, const FVector& Velocity);
    /** Integrate and rebuild the billboard sheet. View vectors come from the live broadcast lens. */
    void Advance(float Dt,const FVector& ViewRight,const FVector& ViewUp,const FVector& ViewForward);
    void Clear();
    int32 Live() const{return Puffs.Num();}
    /** Hard ceiling so a long rally of events can never grow the vertex buffer without bound. */
    static constexpr int32 Capacity=260;
private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Sheet;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Paint;
    TArray<FC26Puff> Puffs;
    void Emit(int Count,const FVector& At,const FVector& Bias,float Speed,float Spread,
        const FLinearColor& Colour,float Size,float Growth,float Life,float Gravity,float Drag,float Peak);
    FRandomStream Random{2604};
    bool Built=false;
};
