#pragma once
#include "CoreMinimal.h"

/** Original sports poses in athlete centimetres: forward, right, up.
 * Timing markers are shared with the release authority. No animation owns a result.
 */
namespace C26Motion
{
    struct FPacePose
    {
        float Time;
        FVector LeftFoot,RightFoot,LeftHand,RightHand;
        float HipDrop,Turn,Lean,Side;
    };
    inline FPacePose Pace(float Time,float Ankle)
    {
        // Gather -> bound -> back-foot contact -> brace -> release -> wrap -> recovery.
        const FPacePose Keys[]={
            {0.f, {-12,-9,0},{26,10,15},{30,-12,146},{24,15,132},-4,-15,14,-3},
            {.16f,{22,-10,30},{-22,10,22},{22,-14,180},{-18,18,137},2,-35,8,-9},
            {.31f,{59,-11,15},{-29,11,0},{27,-14,196},{-32,19,132},-5,-43,4,-13},
            {.48f,{86,-12,0},{-28,12,12},{31,-19,162},{-22,18,184},-11,-26,7,-16},
            {.62f,{79,-12,0},{-43,14,28},{23,-22,118},{17,17,218},-8,3,17,-10},
            {.78f,{37,-11,0},{-18,16,34},{-17,-25,112},{49,5,154},-14,28,37,4},
            {1.02f,{-35,-10,25},{33,12,0},{-26,-21,105},{20,-21,83},-10,32,28,8},
            {1.34f,{11,-11,0},{-12,12,0},{22,-20,100},{22,20,102},-7,0,12,0}
        };
        int I=0;while(I+1<int(UE_ARRAY_COUNT(Keys))-1&&Time>Keys[I+1].Time)++I;
        const auto& A=Keys[I];const auto& B=Keys[I+1];
        float T=FMath::Clamp((Time-A.Time)/(B.Time-A.Time),0.f,1.f);
        T=T*T*(3.f-2.f*T);
        FPacePose R={Time,FMath::Lerp(A.LeftFoot,B.LeftFoot,T),FMath::Lerp(A.RightFoot,B.RightFoot,T),FMath::Lerp(A.LeftHand,B.LeftHand,T),FMath::Lerp(A.RightHand,B.RightHand,T),
            FMath::Lerp(A.HipDrop,B.HipDrop,T),FMath::Lerp(A.Turn,B.Turn,T),FMath::Lerp(A.Lean,B.Lean,T),FMath::Lerp(A.Side,B.Side,T)};
        R.LeftFoot.Z+=Ankle;R.RightFoot.Z+=Ankle;return R;
    }
    inline FVector RunningFoot(float Phase,float Speed,float Side,float Ankle,bool PaceBowler)
    {
        const float Cycle=FMath::Frac(Phase/(2.f*PI)+(Side>0?.5f:0.f));
        const float Span=FMath::Clamp(Speed*.155f,20.f,PaceBowler?112.f:100.f);
        // The support foot travels back relative to the pelvis while remaining on the ground.
        const float Support=.60f;
        const float X=Cycle<Support?FMath::Lerp(Span*.5f,-Span*.5f,Cycle/Support):FMath::Lerp(-Span*.5f,Span*.5f,(Cycle-Support)/(1-Support));
        const float Z=Cycle<Support?0.f:FMath::Sin((Cycle-Support)/(1-Support)*PI)*(PaceBowler?37.f:28.f);
        return FVector(X,Side,Ankle+Z);
    }
}
