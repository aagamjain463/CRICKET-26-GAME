#include "C26CameraDirector.h"
#include "C26Athlete.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogC26Camera,Log,All);
DEFINE_LOG_CATEGORY_STATIC(LogC26Replay,Log,All);

namespace
{
// Broadcast geometry. The striker stands at about (-38, 900); the bowler releases near
// (-20, -940). Every gameplay lens is expressed against those two points so the pitch always
// reads its true 20.12 m and the far stand sits behind the bowler rather than above him.
const FVector Striker(-38,900,0);
constexpr float StrikerEnd=900.f;
// Fixed camera behind the striker's off shoulder: the main broadcast position that shot tracking
// pans from before cutting to a ball-local angle. It sits 33 m back and only 7.8 m up, so it looks
// along the ground at about 13 degrees. The previous 26.8 m / 11.8 m placement looked down at 33
// degrees, which is what turned every tracked shot into a plan view of the outfield.
const FVector MainTower(700,4200,780);
FVector Ahead(const FVector& Ball,const FVector& Velocity,float Seconds)
{
    FVector P=Ball+Velocity*Seconds;P.Z=FMath::Max(P.Z,55.f);return P;
}
}

AC26CameraDirector::AC26CameraDirector()
{
    PrimaryActorTick.bCanEverTick=false;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("BroadcastLens"));RootComponent=Camera;
    Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(37);
    Camera->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
    Camera->PostProcessBlendWeight=1.f;
    Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance=true;
    Camera->PostProcessSettings.DepthOfFieldFocalDistance=0.f;
    BattingRig.Eye=FVector(160,1885,165);BattingRig.Aim=FVector(0,-280,152);BattingRig.FOV=48;
    BowlingRig.Eye=FVector(-225,-3440,235);BowlingRig.Aim=FVector(0,480,235);BowlingRig.FOV=46;
    ReleaseRig.Eye=FVector(-215,-1900,235);ReleaseRig.Aim=FVector(0,790,195);ReleaseRig.FOV=46;
}
void AC26CameraDirector::Reset()
{
    Frames.Reset();Live={};RecordClock=RecordAccumulator=ReplayClock=Impulse=Shake=0;
    ContactStamp=-1;ReplayEnd=0;PlaybackRate=1;ReplayShot=0;
    IsReplaying=HasFielder=ShotAerial=Runners=false;
    ContactPending=ReleasePending=OutcomePending=false;ReleaseStamp=-1;
    HaveCamera=false;LastPhase=EC26Phase::Result;EventName=NAME_None;
    ContactPoint=FVector::ZeroVector;
}
void AC26CameraDirector::SetFieldingTarget(const FVector& Position,bool HasTarget,bool RunnersActive)
{Fielder=Position;HasFielder=HasTarget;Runners=RunnersActive;}
void AC26CameraDirector::MarkContact(float Quality,bool Aerial,const FVector& Where)
{
    ContactPending=true;ShotAerial=Aerial;ContactPoint=Where;
    Impulse=Quality>.85f?.30f:Quality>.5f?.17f:.09f;Shake=Impulse;
    UE_LOG(LogC26Camera,Verbose,TEXT("Contact stamped at %.2f quality %.2f"),ContactStamp,Quality);
}
void AC26CameraDirector::MarkOutcome(FName Event,const FVector& Focus){EventName=Event;EventFocus=Focus;ReplayEnd=RecordClock;OutcomePending=true;}
void AC26CameraDirector::Look(EC26CameraMode NewMode,const FVector& From,const FVector& At,float Fov,bool Cut,float Dt,float TrackRate,float MaxDrop)
{
    const bool Switch=Mode!=NewMode;Mode=NewMode;
    if(Switch)UE_LOG(LogC26Camera,Verbose,TEXT("Camera mode %d"),int(Mode));
    const bool Snap=Cut||!HaveCamera;HaveCamera=true;
    FVector Safe=From;Safe.Z=FMath::Max(95.f,Safe.Z);
    // A broadcast camera looks along the ground, never down onto it. As a tracked subject drifts
    // back toward a raised lens the angle steepens into a plan view of the outfield -- the single
    // thing that made this read as a game board rather than a televised match. Hold the framing by
    // standing the camera further off, not by tilting it down.
    if(MaxDrop<89.f)
    {
        const float Rise=Safe.Z-At.Z;
        if(Rise>1.f)
        {
            const float Need=Rise/FMath::Tan(FMath::DegreesToRadians(MaxDrop));
            FVector Flat(Safe.X-At.X,Safe.Y-At.Y,0);
            const float Have=Flat.Size();
            if(Have<Need)
            {
                Flat=Have>1.f?Flat/Have:FVector(0,1,0);
                Safe.X=At.X+Flat.X*Need;Safe.Y=At.Y+Flat.Y*Need;
            }
        }
    }
    // Keep the lens above the turf, under the roof and inside the enclosure when it is at
    // playing height. Establishing shots above the stands are free to sit outside the bowl.
    if(Safe.Z<2700.f)
    {
        const float Extent=FMath::Sqrt(FMath::Square(Safe.X/7050.f)+FMath::Square(Safe.Y/7740.f));
        if(Extent>1){Safe.X/=Extent;Safe.Y/=Extent;}
    }
    const float Track=1-FMath::Exp(-Dt*TrackRate),Dolly=1-FMath::Exp(-Dt*FMath::Max(2.f,TrackRate*.6f));
    SmoothedAim=Snap?At:FMath::Lerp(SmoothedAim,At,Track);
    const FVector P=Snap?Safe:FMath::Lerp(GetActorLocation(),Safe,Dolly);
    FRotator R=(SmoothedAim-P).Rotation();
    if(Shake>0)
    {
        // A single decaying impulse, not continuous handheld noise.
        R.Pitch+=FMath::Sin(Shake*63.f)*Shake*.9f;
        R.Roll+=FMath::Cos(Shake*47.f)*Shake*.7f;
        Shake=FMath::Max(0.f,Shake-Dt*1.9f);
    }
    Impulse=FMath::Max(0.f,Impulse-Dt);
    SetActorLocationAndRotation(P,R);
    Camera->SetFieldOfView(Snap?Fov:FMath::Lerp(Camera->FieldOfView,Fov,Track));
}
void AC26CameraDirector::Direct(EC26Phase Phase,float Time,bool PlayerBatting,const FVector& Ball,const FVector& Velocity,bool Aerial,float Dt)
{
    if(IsReplaying)return;
#if !UE_BUILD_SHIPPING
    // Repeatable world inspection from the actual playable map, without editor-only cameras.
    FString View;
    if(FParse::Value(FCommandLine::Get(),TEXT("C26WorldView="),View))
    {
        if(View==TEXT("wide"))Look(EC26CameraMode::Establishing,FVector(8200,-10800,7200),FVector(0,0,200),66,true,Dt);
        else if(View==TEXT("pitch"))Look(EC26CameraMode::PreDeliveryBroadcast,FVector(510,1770,315),FVector(0,610,15),48,true,Dt);
        else if(View==TEXT("boundary"))Look(EC26CameraMode::Establishing,FVector(-3700,-5350,180),FVector(-1200,-9500,1780),64,true,Dt);
        else if(View==TEXT("bowling"))Look(EC26CameraMode::BowlerGameplay,BowlingRig.Eye,BowlingRig.Aim,BowlingRig.FOV,true,Dt);
        else Look(EC26CameraMode::BatterGameplay,BattingRig.Eye,BattingRig.Aim,BattingRig.FOV,true,Dt);
        return;
    }
#endif
    const bool Cut=LastPhase!=Phase;LastPhase=Phase;
    if(Phase==EC26Phase::Menu)
    {
        // Slow orbit from inside the ground: stands, roof line and pylons all stay in shot.
        const float A=-.62f+Time*.0055f;
        Look(EC26CameraMode::Establishing,FVector(6250*FMath::Cos(A),6900*FMath::Sin(A),780),FVector(-300,300,620),50,Cut,Dt,1.6f);
    }
    else if(Phase==EC26Phase::Intro)
    {
        if(Time<2.1f)
        {
            // Aerial arrival over the roof, dropping into the bowl.
            const float T=Time/2.1f;
            Look(EC26CameraMode::Establishing,FVector(FMath::Lerp(9200.f,4600.f,T),FMath::Lerp(-11500.f,-6600.f,T),FMath::Lerp(6200.f,2600.f,T)),
                FVector(0,300,500),58,Cut,Dt,1.4f);
        }
        else if(Time<3.6f)
        {
            // Look up a floodlight pylon, then tilt down to the square.
            const float T=(Time-2.1f)/1.5f;
            Look(EC26CameraMode::Establishing,FVector(3900,-5100,760),FVector(FMath::Lerp(5200.f,900.f,T),FMath::Lerp(-6800.f,-1200.f,T),FMath::Lerp(4200.f,240.f,T)),
                46,Mode!=EC26CameraMode::Establishing,Dt,2.2f);
        }
        else if(Time<5.1f)
        {
            // Pitch beauty pass along the strip toward the striker.
            const float T=(Time-3.6f)/1.5f;
            Look(EC26CameraMode::PreDeliveryBroadcast,FVector(238,FMath::Lerp(-260.f,420.f,T),FMath::Lerp(74.f,132.f,T)),Striker+FVector(0,-40,118),31,Mode!=EC26CameraMode::PreDeliveryBroadcast,Dt,2.6f);
        }
        else
            Look(EC26CameraMode::BowlerRunup,FVector(300,-1720,196),FVector(-20,-2340,128),34,Mode!=EC26CameraMode::BowlerRunup,Dt,2.4f);
    }
    else if(Phase==EC26Phase::Ready||Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)
    {
        if(PlayerBatting)
        {
            // Include the striker's shoes, bat toe and crease in the vertical safe area.
            // Carry the run-up push through release; phase-local time must not pull the lens back.
            const float Push=Phase==EC26Phase::Delivery?1.f:Phase==EC26Phase::RunUp?FMath::Clamp(Time/C26Field::RunUpDuration,0.f,1.f):0.f;
            const FVector From=BattingRig.Eye-FVector(3,20,0)*Push;
            Look(Phase==EC26Phase::Delivery?EC26CameraMode::Release:Phase==EC26Phase::RunUp?EC26CameraMode::BatterGameplay:EC26CameraMode::PreDeliveryBroadcast,
                From,BattingRig.Aim,BattingRig.FOV-Push*.3f,Mode!=EC26CameraMode::PreDeliveryBroadcast&&Phase==EC26Phase::Ready,Dt,3.4f);
        }
        else if(Phase==EC26Phase::RunUp)
        {
            // Trail the bowler in. Ball position is the bowling hand during the approach.
            const FVector Hand=Ball.IsZero()?FVector(-20,-2250,150):Ball;
            Look(EC26CameraMode::BowlerRunup,FVector(-215,FMath::Min(Hand.Y-910.f,-1900.f),235),ReleaseRig.Aim,46,Mode!=EC26CameraMode::BowlerRunup,Dt,4.2f);
        }
        else
        {
            const auto& Rig=Phase==EC26Phase::Ready?BowlingRig:ReleaseRig;
            Look(Phase==EC26Phase::Ready?EC26CameraMode::BowlerGameplay:EC26CameraMode::Release,
                Rig.Eye,Rig.Aim,Rig.FOV,Phase==EC26Phase::Ready&&Cut,Dt,4.5f);
        }
    }
    else if(Phase==EC26Phase::InPlay)
    {
        const float Range=FVector::Dist2D(Ball,Striker);
        const float RopeFraction=FMath::Sqrt(FMath::Square(Ball.X/C26Field::RadiusX)+FMath::Square(Ball.Y/C26Field::RadiusY));
        const FVector Flat=FVector(Velocity.X,Velocity.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
        const FVector Side(-Flat.Y,Flat.X,0);
        if(Time<.34f)
        {
            // Stay on the striker through contact and the first of the follow-through, punching in
            // slightly. Cutting away from the bat on impact is what made the old shot feel weightless.
            const auto& Rig=PlayerBatting?BattingRig:ReleaseRig;
            Look(EC26CameraMode::BatContact,Rig.Eye-(PlayerBatting?FVector(3,20,0):FVector::ZeroVector),Rig.Aim,Rig.FOV-.3f,false,Dt,5.f);
        }
        else if(Time<1.25f&&RopeFraction<.82f)
        {
            // Main tower pans with the ball: the shot is seen leaving into a deep, populated field.
            FVector Aim=Ahead(Ball,Velocity,Aerial?.34f:.24f);
            if(HasFielder)Aim=FMath::Lerp(Aim,Fielder+FVector(0,0,110),.18f);
            // Hand the frame over from the batter to the ball as the ball runs away. Aiming straight
            // at a ball that is still next to the bat threw the striker out of shot entirely.
            // A high ball shrinks to a pixel against the stands, so the lens tightens as it climbs.
            const float Handover=FMath::Clamp(Range/2400.f,0.f,1.f);
            Aim=FMath::Lerp(Striker+FVector(0,0,150),Aim,Handover);
            const float HeightTighten=Aerial?FMath::Clamp((Ball.Z-250.f)/300.f,0.f,9.f):0.f;
            Look(Aerial?EC26CameraMode::LoftedShotTracking:EC26CameraMode::GroundShotTracking,
                MainTower,Aim,FMath::Clamp(29.f+Range/230.f-HeightTighten,26.f,52.f),Mode==EC26CameraMode::BatContact,Dt,5.5f,17.f);
        }
        else if(Runners&&RopeFraction<.55f)
        {
            // Square-of-the-wicket running camera holding both batters and the throw.
            // A skier's apex is above the lens: aiming at the ball itself tilts up into the
            // stands and loses the field. Hold the ball's ground line instead, the way a
            // broadcast stays wide on the waiting catcher while the ball descends into frame.
            FVector Mid=FMath::Lerp(FVector(0,0,120),FVector(Ball.X,Ball.Y,FMath::Min(Ball.Z,320.f)),.35f);
            Look(EC26CameraMode::Running,FVector(3250,240,540),Mid,FMath::Clamp(26.f+Range/210.f,30.f,44.f),Mode!=EC26CameraMode::Running,Dt,4.f,15.f);
        }
        else if(RopeFraction>.82f)
        {
            // Boundary camera: outside the ball's line, looking back at the rope and the crowd.
            const FVector Radial=FVector(Ball.X,Ball.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
            FVector From=Ball+Radial*1450.f+FVector(-Radial.Y,Radial.X,0)*900.f;
            From.Z=Aerial?FMath::Clamp(560.f+Ball.Z*.42f,620.f,1750.f):430.f;
            Look(EC26CameraMode::BoundaryTracking,From,Ahead(Ball,Velocity,.22f),Aerial?46.f:40.f,Mode!=EC26CameraMode::BoundaryTracking,Dt,5.f,Aerial?34.f:20.f);
        }
        else
        {
            // Ball-local chase: lateral and behind, low for a driven ball, lifted for a lofted one.
            // Same height-tightening as the tower: a climbing ball gets a longer lens, not a wider one.
            FVector Aim=Ahead(Ball,Velocity,Aerial?.30f:.22f);
            if(HasFielder)Aim=FMath::Lerp(Aim,Fielder+FVector(0,0,110),Aerial?.20f:.30f);
            FVector From=Aim-Flat*(Aerial?2450.f:1750.f)+Side*(Aerial?1550.f:1180.f);
            From.Z=Aerial?FMath::Clamp(500.f+Ball.Z*.55f,640.f,1900.f):330.f;
            const float ChaseFov=Aerial?FMath::Clamp(38.f+Range/900.f-FMath::Clamp((Ball.Z-250.f)/300.f,0.f,8.f),30.f,50.f):39.f;
            Look(Aerial?EC26CameraMode::LoftedShotTracking:EC26CameraMode::GroundShotTracking,
                From,Aim,ChaseFov,
                Mode==EC26CameraMode::BatContact||Mode==EC26CameraMode::Running,Dt,5.2f,Aerial?36.f:19.f);
        }
    }
    else if(Phase==EC26Phase::Reaction)
    {
        const bool Wicket=EventName==TEXT("WICKET");
        const bool Boundary=EventName==TEXT("SIX")||EventName==TEXT("FOUR");
        if(Wicket&&Time<.85f)
            Look(EC26CameraMode::Wicket,EventFocus+FVector(305,-395,155),EventFocus+FVector(0,0,58),33,Cut,Dt,4.5f);
        else if(Boundary&&Time<.9f)
            Look(EC26CameraMode::Celebration,Striker+FVector(360,-520,175),Striker+FVector(0,0,138),30,Cut,Dt,3.4f);
        else
            Look(EC26CameraMode::Celebration,Striker+FVector(318,-455,182),Striker+FVector(0,-30,132),34,Cut||Mode==EC26CameraMode::Wicket,Dt,3.f);
    }
    else if(Phase==EC26Phase::Interval)
    {
        // Slow drift across the ground with the pylons and roof in frame.
        const float T=FMath::Clamp(Time*.06f,0.f,1.f);
        Look(EC26CameraMode::InningsTransition,FVector(-4250+T*700,-4550,1420-T*180),FVector(0,200,420),50,Cut,Dt,1.5f);
    }
    else if(Phase==EC26Phase::Result)
        // Wide of the square and well back. The old position sat close enough to the striker's end
        // that the celebrating bowler and the dismissed batter overlapped each other on the lens.
        Look(EC26CameraMode::MatchResult,FVector(2450,3350,980),FVector(0,500,180),38,Cut,Dt,1.8f);
}
FC26ReplayFrame AC26CameraDirector::CaptureState(const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors) const
{
    FC26ReplayFrame F;F.Time=RecordClock;F.Ball=Ball;F.Athletes.Reserve(Actors.Num());
    for(const auto& Actor:Actors)
    {
        FC26ReplayAthlete A;A.Transform=Actor->GetActorTransform();A.Action=Actor->Action;
        A.ActionTime=Actor->ActionTime;A.MotionTime=Actor->MotionTime;A.ShotAngle=Actor->ShotAngle;
        A.Contact=Actor->ContactTarget;A.LookAt=Actor->LookAt;A.Loft=Actor->Loft;
        A.Footwork=Actor->FootworkIntent;A.Stride=Actor->StrideIntent;A.Defend=Actor->Defending;A.DeliveryStyle=Actor->DeliveryStyle;
        A.MoveSpeed=Actor->MoveSpeed;A.Gait=Actor->GaitPhase;A.Trigger=Actor->Trigger;
        F.Athletes.Add(A);
    }
    for(const auto& Prop:ReplayProps)F.Props.Add(Prop->GetComponentTransform());
    return F;
}
void AC26CameraDirector::Record(float Dt,const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    RecordClock+=Dt;RecordAccumulator+=Dt;
    // Preserve the exact rendered impact even if it falls between regular replay samples.
    if(ReleasePending){ReleaseStamp=RecordClock;ReleasePending=false;}
    else if(ContactPending){ContactStamp=RecordClock;ContactPending=false;}
    else if(OutcomePending)OutcomePending=false;
    else if(RecordAccumulator<1.f/45.f)return;
    RecordAccumulator=FMath::Fmod(RecordAccumulator,1.f/45.f);
    Frames.Add(CaptureState(Ball,Actors));
    // 16 seconds at 45 Hz covers run-up and the longest fielding sequence; storage stays bounded.
    if(Frames.Num()>720)Frames.RemoveAt(0,Frames.Num()-720,EAllowShrinking::No);
}
bool AC26CameraDirector::BeginReplay(const TArray<TObjectPtr<AC26Athlete>>& Actors,const FVector& Ball)
{
    if(Frames.Num()<4)return false;
    Live=CaptureState(Ball,Actors);IsReplaying=true;ReplayShot=0;
    ReplayEnd=Frames.Last().Time;
    // Always open on the delivery arriving, so the replay actually contains the bat meeting ball.
    ReplayClock=FMath::Max(Frames[0].Time,ReleaseStamp>=0?ReleaseStamp-.16f:ContactStamp>=0?ContactStamp-.55f:ReplayEnd-1.8f);
    HaveCamera=false;
    UE_LOG(LogC26Replay,Log,TEXT("Replay frames=%d contact=%.2f end=%.2f event=%s"),Frames.Num(),ContactStamp,ReplayEnd,*EventName.ToString());
    return true;
}
void AC26CameraDirector::ApplyFrame(const FC26ReplayFrame& A,const FC26ReplayFrame& B,float T,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    for(int I=0;I<Actors.Num()&&I<A.Athletes.Num()&&I<B.Athletes.Num();++I)
    {
        const auto& X=A.Athletes[I];const auto& Y=B.Athletes[I];const auto& Selected=T<.5f?X:Y;
        auto Actor=Actors[I];FTransform P;P.Blend(X.Transform,Y.Transform,T);Actor->SetActorTransform(P);
        Actor->Action=Selected.Action;Actor->ActionTime=X.Action==Y.Action?FMath::Lerp(X.ActionTime,Y.ActionTime,T):Selected.ActionTime;
        Actor->MotionTime=FMath::Lerp(X.MotionTime,Y.MotionTime,T);Actor->ShotAngle=Selected.ShotAngle;
        Actor->ContactTarget=Selected.Contact;Actor->LookAt=Selected.LookAt;Actor->Loft=Selected.Loft;
        Actor->FootworkIntent=Selected.Footwork;Actor->StrideIntent=Selected.Stride;Actor->Defending=Selected.Defend;Actor->DeliveryStyle=Selected.DeliveryStyle;
        Actor->MoveSpeed=FMath::Lerp(X.MoveSpeed,Y.MoveSpeed,T);Actor->GaitPhase=FMath::Lerp(X.Gait,Y.Gait,T);Actor->Trigger=FMath::Lerp(X.Trigger,Y.Trigger,T);
        Actor->Animate(0);
    }
    for(int I=0;I<ReplayProps.Num()&&I<A.Props.Num()&&I<B.Props.Num();++I)
    {FTransform P;P.Blend(A.Props[I],B.Props[I],T);ReplayProps[I]->SetWorldTransform(P);}
}
bool AC26CameraDirector::PlayReplay(float Dt,FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    if(!IsReplaying)return false;
    const bool Wicket=EventName==TEXT("WICKET");
    // Ramp into slow motion around the moment of contact and again on the dismissal, then recover.
    const float ToContact=ContactStamp>=0?FMath::Abs(ReplayClock-ContactStamp):BIG_NUMBER;
    PlaybackRate=ToContact<.20f?(Wicket?.28f:.38f):ToContact<.45f?.6f:.9f;
    if(Wicket&&ReplayEnd-ReplayClock<.45f)PlaybackRate=.32f;
    const float NextClock=ReplayClock+Dt*PlaybackRate;
    // Display the saved impact once instead of interpolating across its velocity discontinuity.
    ReplayClock=ReplayClock<ReleaseStamp&&NextClock>=ReleaseStamp?ReleaseStamp:ReplayClock<ContactStamp&&NextClock>=ContactStamp?ContactStamp:NextClock;
    bool Cut=false;
    // Three-shot cut: close on the bat, then the outcome angle, then a wide of the result.
    if(ReplayShot==0&&ContactStamp>=0&&ReplayClock>ContactStamp+.70f){ReplayShot=1;Cut=true;}
    if(ReplayShot==1&&ReplayEnd>ReplayClock+1.35f&&ReplayClock>ContactStamp+1.5f)
    {ReplayClock=FMath::Max(ReplayClock,ReplayEnd-1.5f);ReplayShot=2;Cut=true;}
    if(ReplayClock>=ReplayEnd){Restore(Actors);Ball=Live.Ball;return false;}
    int I=0;while(I+1<Frames.Num()&&Frames[I+1].Time<ReplayClock)++I;
    const auto& A=Frames[I];const auto& B=Frames[FMath::Min(I+1,Frames.Num()-1)];
    const float T=FMath::Clamp((ReplayClock-A.Time)/FMath::Max(.001f,B.Time-A.Time),0.f,1.f);
    Ball=FMath::Lerp(A.Ball,B.Ball,T);ApplyFrame(A,B,T,Actors);
    const FVector Anchor=ContactPoint.IsZero()?Striker+FVector(0,0,110):ContactPoint;
    if(ReplayShot==0&&ReleaseStamp>=0&&ReplayClock<ReleaseStamp+.10f)
    {
        const FVector Bowler=Actors[0]->GetActorLocation();
        Look(EC26CameraMode::ReplayPitch,Bowler+FVector(-620,140,185),Bowler+FVector(0,35,135),35,Mode!=EC26CameraMode::ReplayPitch,Dt,7.f);
    }
    else if(ReplayShot==0)
    {
        // Tight side-on on the stroke itself: bat, ball and the batter's shape all in one frame.
        // The stand-off is measured from the contact point rather than a fixed world position, so
        // the shot cannot end up inside the batter when the stroke happens away from the crease.
        // A lofted ball climbs out of a batter-locked frame in a tenth of a second, so the aim
        // hands over to the ball much faster when the shot went up.
        const float InnerAe=ShotAerial?.55f:.20f,OuterAe=ShotAerial?.62f:.42f;
        const FVector Eye=FVector(Anchor.X,FMath::Min(Anchor.Y,StrikerEnd),0)+FVector(-745,-190,0)+FVector(0,0,FMath::Max(150.f,Anchor.Z+22.f));
        Look(EC26CameraMode::ReplayClose,Eye,
            FMath::Lerp(Striker+FVector(0,0,112),FMath::Lerp(Anchor,Ball,InnerAe),OuterAe),34,Cut||Mode==EC26CameraMode::ReplayPitch,Dt,6.5f,26.f);
    }
    else if(ReplayShot==1&&Wicket)
    {
        // Low stump-height angle for the dismissal.
        Look(EC26CameraMode::ReplayPitch,EventFocus+FVector(-455,-505,92),FMath::Lerp(EventFocus+FVector(0,0,48),Ball,.30f),35,Cut,Dt,6.f);
    }
    else if(ReplayShot==1)
    {
        // Behind the bowler's arm down the length of the pitch.
        Look(EC26CameraMode::ReplayPitch,FVector(-40,-2450,236),FMath::Lerp(Anchor,Ball,.45f),31,Cut,Dt,5.5f);
    }
    else
    {
        const FVector Radial=FVector(Ball.X,Ball.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
        Look(EC26CameraMode::ReplayBoundary,Ball-Radial*1650.f+FVector(-Radial.Y,Radial.X,0)*1050.f+FVector(0,0,520),Ball,41,Cut,Dt,4.5f);
    }
    return true;
}
void AC26CameraDirector::Restore(const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    if(IsReplaying)ApplyFrame(Live,Live,0,Actors);
    IsReplaying=false;HaveCamera=false;LastPhase=EC26Phase::Menu;PlaybackRate=1;ReplayShot=0;
}
