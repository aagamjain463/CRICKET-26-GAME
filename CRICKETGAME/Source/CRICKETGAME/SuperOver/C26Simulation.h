#pragma once
#include "CoreMinimal.h"
#include "C26Types.h"
#include "Core/C26Rules.h"

struct FC26BallState
{
    FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector;
    bool Active=false, Struck=false, Bounced=false, PostHitBounce=false;
    float Age=0;
};
struct FC26Contact
{
    EC26Timing Timing=EC26Timing::Miss;
    FVector Velocity=FVector::ZeroVector;
    FString Shot=TEXT("LEAVE");
    float Quality=0;
};

class FC26Simulation
{
public:
    FC26BallState Ball;
    FC26DeliveryPlan Plan;
    FC26Tuning Tuning;
    float ContactTime=0;
    FVector ContactPosition;
    bool CrossedContact=false, BounceEvent=false, StumpEvent=false, BoundaryEvent=false;
    TArray<FVector> BoundaryPolygon;
    FC26Simulation();
    void Reset();
    void Release(const FC26DeliveryPlan& InPlan,const FVector& Origin);
    void Step(float Dt);
    FC26BallState Predict(float Seconds) const;
    FVector PredictLanding(float MaxSeconds=6.f) const;
    FC26Contact Hit(const FC26ShotIntent& Intent,float TimingError,int Difficulty,FRandomStream& Random);
    bool CrossesRope(const FVector& From,const FVector& To) const;
private:
    void Integrate(FC26BallState& State,float Dt) const;
};

struct FC26AIHistory
{
    float OffsideBias=0,LastPower=0;
    int Boundaries=0;
    void Reset(){ *this={}; }
};
class FC26AI
{
public:
    FRandomStream Random;
    FC26AIHistory History;
    void Reset(int Seed){Random.Initialize(Seed);History.Reset();}
    FC26DeliveryPlan Bowl(const C26::Match& Rules,int Difficulty);
    FC26ShotIntent Bat(const FC26DeliveryPlan& VisibleDelivery,const C26::Match& Rules,int Difficulty);
    float TimingError(int Difficulty);
    static TArray<FVector> Field(bool ProtectOffside=false);
};
