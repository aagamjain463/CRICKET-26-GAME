#include "C26CameraDirector.h"
#include "C26Athlete.h"
#include "Camera/CameraComponent.h"
AC26CameraDirector::AC26CameraDirector()
{PrimaryActorTick.bCanEverTick=false;Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("BroadcastLens"));RootComponent=Camera;Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(43.f);}
void AC26CameraDirector::Reset(){Frames.Reset();RecordClock=RecordAccumulator=ReplayClock=0;IsReplaying=false;LastPhase=EC26Phase::Result;}
void AC26CameraDirector::Look(const FVector& From,const FVector& At,float Fov,bool Cut,float Dt)
{
    const FVector Safe(From.X,From.Y,FMath::Max(110.f,From.Z));
    const FVector P=Cut?Safe:FMath::VInterpTo(GetActorLocation(),Safe,Dt,4.5f);
    SetActorLocation(P);SetActorRotation((At-P).Rotation());Camera->SetFieldOfView(Cut?Fov:FMath::FInterpTo(Camera->FieldOfView,Fov,Dt,5));
}
void AC26CameraDirector::Direct(EC26Phase Phase,float PhaseTime,bool PlayerBatting,const FVector& Ball,const FVector& Velocity,bool Aerial)
{
    if(IsReplaying)return;
    const bool Cut=LastPhase!=Phase;LastPhase=Phase;float Dt=GetWorld()?GetWorld()->GetDeltaSeconds():.016f;
    if(Phase==EC26Phase::Menu)
    {
        // Slow orbit that keeps the square right of the menu panel and never clips the stands.
        const float A=.30f+PhaseTime*.016f;
        Look(FVector(7100*FMath::Cos(A),-7500*FMath::Sin(A),2450),FVector(-1500,250,300),50,Cut,Dt);
    }
    else if(Phase==EC26Phase::Intro)
    {
        if(PhaseTime<3.1f)
        {
            // Descending flyover from deep midwicket into the square.
            const float T=FMath::Clamp(PhaseTime/3.1f,0.f,1.f);
            Look(FVector(5400-T*2000,-6400+T*1600,2950-T*1750),FVector(0,120,140),52,Cut,Dt);
        }
        else Look(FVector(520,255,215),FVector(-30,895,108),34,Cut||PhaseTime<3.16f,Dt);
    }
    else if(Phase==EC26Phase::Ready||Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)
    {
        // Offset laterally and stay high so the keeper (batting view) or the bowler
        // (bowling view) never stands on the lens axis and occludes the pitch.
        if(PlayerBatting)Look(FVector(430,2620,1080),FVector(-30,480,90),38,Cut,Dt);
        else Look(FVector(-430,-3750,780),FVector(30,350,100),36,Cut,Dt);
    }
    else if(Phase==EC26Phase::InPlay)
    {
        // Chase the struck ball but keep the pitch in frame: bias the aim point
        // toward the square and stay high so the ground (not just stands) reads.
        const FVector Aim=FMath::Lerp(FVector(0,620,70),Ball,Aerial?.55f:.45f);
        const FVector Flat=FVector(Velocity.X,Velocity.Y,0).GetSafeNormal();
        const FVector Side=FVector(-Flat.Y,Flat.X,0);
        const FVector From=Aim-Flat*(Aerial?3800.f:3200.f)+Side*760+FVector(0,0,Aerial?1600.f:1000.f);
        Look(From,Aim,Aerial?44.f:42.f,Cut,Dt);
    }
    else if(Phase==EC26Phase::Reaction)
    {Look(FVector(495,335,225),FVector(-30,890,125),36,Cut,Dt);}
    else if(Phase==EC26Phase::Interval)
    {Look(FVector(-2900,-4100,1180),FVector(0,0,90),44,Cut,Dt);}
    else if(Phase==EC26Phase::Result)
    {Look(FVector(3800,600,1100),FVector(-200,850,80),40,Cut,Dt);}
}
void AC26CameraDirector::Record(float Dt,const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    RecordClock+=Dt;RecordAccumulator+=Dt;if(RecordAccumulator<1.f/30.f)return;RecordAccumulator=0;
    FC26ReplayFrame F;F.Time=RecordClock;F.Ball=Ball;
    for(auto A:Actors){F.Actors.Add(A->GetActorTransform());F.Actions.Add(A->Action);F.ActionTimes.Add(A->ActionTime);}
    Frames.Add(MoveTemp(F));if(Frames.Num()>300)Frames.RemoveAt(0,Frames.Num()-300,EAllowShrinking::No);
}
bool AC26CameraDirector::BeginReplay()
{
    if(Frames.Num()<4)return false;
    Live=Frames.Last();IsReplaying=true;ReplayClock=FMath::Max(Frames[0].Time,Frames.Last().Time-4.2f);return true;
}
bool AC26CameraDirector::PlayReplay(float Dt,FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    if(!IsReplaying)return false;
    ReplayClock+=Dt*.65f;if(ReplayClock>=Frames.Last().Time){Restore(Actors);return false;}
    int Index=0;while(Index+1<Frames.Num()&&Frames[Index+1].Time<ReplayClock)++Index;
    const auto& A=Frames[Index];const auto& B=Frames[FMath::Min(Index+1,Frames.Num()-1)];
    const float T=FMath::Clamp((ReplayClock-A.Time)/FMath::Max(.001f,B.Time-A.Time),0.f,1.f);
    Ball=FMath::Lerp(A.Ball,B.Ball,T);
    for(int I=0;I<Actors.Num()&&I<A.Actors.Num();++I)
    {
        FTransform P;P.Blend(A.Actors[I],B.Actors[I],T);Actors[I]->SetActorTransform(P);
        Actors[I]->Action=A.Actions[I];Actors[I]->ActionTime=FMath::Lerp(A.ActionTimes[I],B.ActionTimes[I],T);Actors[I]->Animate(0);
    }
    const float Distance=FVector::Dist2D(Ball,FVector(0,850,0));
    if(Distance<1700)Look(FVector(-810,690,175),FMath::Lerp(FVector(0,880,110),Ball,.5f),37,true,Dt);
    else Look(Ball+FVector(-1900,1550,FMath::Max(520.f,900-Ball.Z*.22f)),Ball,44,false,Dt);
    return true;
}
void AC26CameraDirector::Restore(const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    for(int I=0;I<Actors.Num()&&I<Live.Actors.Num();++I)
    {Actors[I]->SetActorTransform(Live.Actors[I]);Actors[I]->Action=Live.Actions[I];Actors[I]->ActionTime=Live.ActionTimes[I];Actors[I]->Animate(0);}
    IsReplaying=false;LastPhase=EC26Phase::Menu;
}
