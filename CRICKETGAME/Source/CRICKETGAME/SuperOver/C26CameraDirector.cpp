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
// Pitch physical reference points:
// CameraStrikerMark stands at (-38, 900); Bowler releases near (-20, -940).
const FVector CameraStrikerMark(-38,900,0);
constexpr float StrikerEnd=900.f;

// Secondary broadcast tower position for long outfield ball tracking
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
    Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(46);
    Camera->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
    Camera->PostProcessBlendWeight=1.f;
    Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance=true;
    Camera->PostProcessSettings.DepthOfFieldFocalDistance=0.f;

    // TELEVISION LIVE BROADCAST CAMERAS (MATCH STREAMS / CRICKET 24 STYLE):
    // Positioned backward and above so the entire bowler (from head to spikes)
    // is completely visible on the turf, looking straight down the 22 yards of the pitch.
    BattingRig.Eye = FVector(-10.f, -4400.f, 640.f);
    BattingRig.Aim = FVector(-10.f, 900.f, 65.f);
    BattingRig.FOV = 46.f;

    BowlingRig.Eye = FVector(-10.f, -4400.f, 640.f);
    BowlingRig.Aim = FVector(-10.f, 900.f, 65.f);
    BowlingRig.FOV = 46.f;

    ReleaseRig.Eye = FVector(-10.f, -2600.f, 540.f);
    ReleaseRig.Aim = FVector(-10.f, 900.f, 65.f);
    ReleaseRig.FOV = 42.f;
}

void AC26CameraDirector::Reset()
{
    Frames.Reset();Live={};RecordClock=RecordAccumulator=ReplayClock=Impulse=Shake=0;
    ContactStamp=-1;ReplayEnd=0;PlaybackRate=1;ReplayShot=0;
    IsReplaying=HasFielder=ShotAerial=Runners=false;
    IsReplayOutro=false;ReplayOutroTime=0.f;ReplayOutroAlpha=0.f;
    ContactPending=ReleasePending=OutcomePending=false;ReleaseStamp=-1;
    HaveCamera=false;LastPhase=EC26Phase::Result;EventName=NAME_None;EventDetail=NAME_None;
    ContactPoint=FVector::ZeroVector;
    bFieldPlanning=false;
}

void AC26CameraDirector::SetFieldingTarget(const FVector& Position,bool HasTarget,bool RunnersActive)
{
    Fielder=Position;HasFielder=HasTarget;Runners=RunnersActive;
}

void AC26CameraDirector::MarkContact(float Quality,bool Aerial,const FVector& Where)
{
    ContactPending=true;ShotAerial=Aerial;ContactPoint=Where;
    Impulse=Quality>.85f?.30f:Quality>.5f?.17f:.09f;Shake=Impulse;
    UE_LOG(LogC26Camera,Verbose,TEXT("Contact stamped at %.2f quality %.2f"),ContactStamp,Quality);
}

void AC26CameraDirector::MarkOutcome(FName Event,const FVector& Focus,FName Detail)
{
    EventName=Event;EventDetail=Detail;EventFocus=Focus;ReplayEnd=RecordClock;OutcomePending=true;
}

void AC26CameraDirector::Look(EC26CameraMode NewMode,const FVector& From,const FVector& At,float Fov,bool Cut,float Dt,float TrackRate,float MaxDrop)
{
    const bool Switch=Mode!=NewMode;Mode=NewMode;
    if(Switch)UE_LOG(LogC26Camera,Verbose,TEXT("Camera mode %d"),int(Mode));
    const bool Snap=Cut||!HaveCamera;HaveCamera=true;
    FVector Safe=From;Safe.Z=FMath::Max(95.f,Safe.Z);

    if(MaxDrop<89.f)
    {
        const float Rise=Safe.Z-At.Z;
        if(Rise>1.f)
        {
            const float Need=Rise/FMath::Tan(FMath::DegreesToRadians(MaxDrop));
            FVector Flat(Safe.X-At.X,Safe.Y-At.Y,0);
            const float Have=Flat.Size();
            if(Have<Need)\
            {
                Flat=Have>1.f?Flat/Have:FVector(0,1,0);
                Safe.X=At.X+Flat.X*Need;Safe.Y=At.Y+Flat.Y*Need;
            }
        }
    }

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
    if(bFieldPlanning)
    {
        // Elevated tactical camera framing the whole cricket ground cleanly:
        // Eye at Z = 23500 with FOV 50 frames the entire ground boundary oval
        // at a compact, readable scale (~550px circle diameter) comfortably centered on screen,
        // leaving clean breathing margins from the top status header and the bottom formation bar.
        // All 11 players and the boundary rope are 100% visible and easily draggable.
        // ROTATION ONLY: the eye sits south of the aim (same height/FOV as before) so the
        // top-down view settles yaw-north: striker/north at the top, bowler/south at the
        // bottom, matching the broadcast behind-the-bowler view. A perfectly vertical
        // eye==aim leaves yaw ambiguous (defaults east-up) and reads as rotated.
        Look(EC26CameraMode::FieldPlanning,FVector(0.f,-1750.f,23500.f),FVector(0.f,250.f,0.f),50.f,Mode!=EC26CameraMode::FieldPlanning,Dt,6.f,90.f);
        return;
    }
    if(Phase==EC26Phase::Menu)
    {
        const float A=-.62f+Time*.0055f;
        Look(EC26CameraMode::Establishing,FVector(6250*FMath::Cos(A),6900*FMath::Sin(A),780),FVector(-300,300,620),50,Cut,Dt,1.6f);
    }
    else if(Phase==EC26Phase::Intro)
    {
        if(Time<2.1f)
        {
            const float T=Time/2.1f;
            Look(EC26CameraMode::Establishing,FVector(FMath::Lerp(9200.f,4600.f,T),FMath::Lerp(-11500.f,-6600.f,T),FMath::Lerp(6200.f,2600.f,T)),
                FVector(0,300,500),58,Cut,Dt,1.4f);
        }
        else if(Time<3.6f)
        {
            const float T=(Time-2.1f)/1.5f;
            Look(EC26CameraMode::Establishing,FVector(3900,-5100,760),FVector(FMath::Lerp(5200.f,900.f,T),FMath::Lerp(-6800.f,-1200.f,T),FMath::Lerp(4200.f,240.f,T)),
                46,Mode!=EC26CameraMode::Establishing,Dt,2.2f);
        }
        else if(Time<5.1f)
        {
            const float T=(Time-3.6f)/1.5f;
            Look(EC26CameraMode::PreDeliveryBroadcast,FVector(238,FMath::Lerp(-260.f,420.f,T),FMath::Lerp(74.f,132.f,T)),CameraStrikerMark+FVector(0,-40,118),31,Mode!=EC26CameraMode::PreDeliveryBroadcast,Dt,2.6f);
        }
        else
            Look(EC26CameraMode::BowlerRunup,FVector(300,-1720,196),FVector(-20,-2340,128),34,Mode!=EC26CameraMode::BowlerRunup,Dt,2.4f);
    }
    else if(Phase==EC26Phase::Ready||Phase==EC26Phase::RunUp||Phase==EC26Phase::Delivery)
    {
        // TELEVISION LIVE BROADCAST CAMERA (BOTH WHILE BATTING AND BOWLING):
        // Stationed generously backward and above so the entire bowler (from head to spikes)
        // is completely visible on the turf, looking straight down the 22 yards of the pitch.
        // Dollies forward smoothly in lockstep with the bowler's run-up with zero jump cuts.
        const FVector BowlerPos = BowlerActor ? BowlerActor->GetActorLocation() : FVector(-20.f, -2700.f, 5.f);
        const FVector StrikerTarget = StrikerActor ? StrikerActor->GetActorLocation() + FVector(0, 0, 65.f) : FVector(-10.f, 900.f, 65.f);
        
        // Progress of bowler's physical advance along the pitch (from -2700 to -995):
        const float BowlerRunFraction = FMath::Clamp((BowlerPos.Y - (-2700.f)) / (-995.f - (-2700.f)), 0.f, 1.f);
        const float SmoothRun = FMath::InterpEaseInOut(0.f, 1.f, BowlerRunFraction, 1.4f);
        
        // Dynamic camera tracking: backward and above, framing the full bowler, tracking smoothly into delivery
        const float CamY = FMath::Lerp(-4400.f, -2600.f, SmoothRun);
        const float CamZ = FMath::Lerp(640.f, 540.f, SmoothRun);
        const float CamFov = FMath::Lerp(46.f, 42.f, SmoothRun);
        const FVector Eye(-10.f, CamY, CamZ);
        
        if (Phase == EC26Phase::Ready)
        {
            // Backward and above: full bowler in frame from head to boots looking down the wicket
            Look(EC26CameraMode::BowlerGameplay, Eye, StrikerTarget, CamFov, Cut, Dt, 5.0f);
        }
        else if (Phase == EC26Phase::RunUp)
        {
            // Glides smoothly forward in lockstep with the bowler's run up - zero cut or snap!
            Look(EC26CameraMode::BowlerRunup, Eye, StrikerTarget, CamFov, false, Dt, 6.5f);
        }
        else // Phase == EC26Phase::Delivery
        {
            // Delivery stride & release: completely frames bowler, umpire in midground, pitch, and striker
            Look(EC26CameraMode::Release, Eye, StrikerTarget, CamFov, false, Dt, 6.5f);
        }
    }
    else if(Phase==EC26Phase::InPlay)
    {
        const float Range=FVector::Dist2D(Ball,CameraStrikerMark);
        const float RopeFraction=FMath::Sqrt(FMath::Square(Ball.X/C26Field::RadiusX)+FMath::Square(Ball.Y/C26Field::RadiusY));
        const FVector Flat=FVector(Velocity.X,Velocity.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
        const FVector Side(-Flat.Y,Flat.X,0);
        if(Time<.38f)
        {
            // Television broadcast view: hold elevated and backward behind bowler's end, watching delivery reach batsman and stroke played!
            const FVector Eye(-10.f, -2600.f, 540.f);
            const FVector StrikerTarget = StrikerActor ? StrikerActor->GetActorLocation() + FVector(0, 0, 65.f) : FVector(-10.f, 900.f, 65.f);
            Look(EC26CameraMode::BatContact, Eye, StrikerTarget, 42.f, false, Dt, 5.5f);
        }
        else if(Time<1.25f&&RopeFraction<.82f)
        {
            FVector Aim=Ahead(Ball,Velocity,Aerial?.34f:.24f);
            if(HasFielder)Aim=FMath::Lerp(Aim,Fielder+FVector(0,0,110),.18f);
            const float Handover=FMath::Clamp(Range/2400.f,0.f,1.f);
            Aim=FMath::Lerp(CameraStrikerMark+FVector(0,0,150),Aim,Handover);
            const float HeightTighten=Aerial?FMath::Clamp((Ball.Z-250.f)/300.f,0.f,9.f):0.f;
            Look(Aerial?EC26CameraMode::LoftedShotTracking:EC26CameraMode::GroundShotTracking,
                MainTower,Aim,FMath::Clamp(29.f+Range/230.f-HeightTighten,26.f,52.f),Mode==EC26CameraMode::BatContact,Dt,5.5f,17.f);
        }
        else if(Runners&&RopeFraction<.55f)
        {
            FVector Mid=FMath::Lerp(FVector(0,0,120),FVector(Ball.X,Ball.Y,FMath::Min(Ball.Z,320.f)),.35f);
            Look(EC26CameraMode::Running,FVector(3250,240,540),Mid,FMath::Clamp(26.f+Range/210.f,30.f,44.f),Mode!=EC26CameraMode::Running,Dt,4.f,15.f);
        }
        else if(RopeFraction>.82f)
        {
            const FVector Radial=FVector(Ball.X,Ball.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
            FVector From=Ball+Radial*1450.f+FVector(-Radial.Y,Radial.X,0)*900.f;
            From.Z=Aerial?FMath::Clamp(560.f+Ball.Z*.42f,620.f,1750.f):430.f;
            Look(EC26CameraMode::BoundaryTracking,From,Ahead(Ball,Velocity,.22f),Aerial?46.f:40.f,Mode!=EC26CameraMode::BoundaryTracking,Dt,5.f,Aerial?34.f:20.f);
        }
        else
        {
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
            Look(EC26CameraMode::Celebration,CameraStrikerMark+FVector(360,-520,175),CameraStrikerMark+FVector(0,0,138),30,Cut,Dt,3.4f);
        else
            Look(EC26CameraMode::Celebration,CameraStrikerMark+FVector(318,-455,182),CameraStrikerMark+FVector(0,-30,132),34,Cut||Mode==EC26CameraMode::Wicket,Dt,3.f);
    }
    else if(Phase==EC26Phase::Interval)
    {
        const float T=FMath::Clamp(Time*.06f,0.f,1.f);
        Look(EC26CameraMode::InningsTransition,FVector(-4250+T*700,-4550,1420-T*180),FVector(0,200,420),50,Cut,Dt,1.5f);
    }
    else if(Phase==EC26Phase::Result)
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
        A.ShotLabel=Actor->ShotLabel;
        if(Actor->Presentation&&Actor->Presentation->IsActive())A.CharacterPose=Actor->Presentation->CapturePose();
        A.MoveSpeed=Actor->MoveSpeed;A.Gait=Actor->GaitPhase;A.Trigger=Actor->Trigger;
        F.Athletes.Add(A);
    }
    for(const auto& Prop:ReplayProps)F.Props.Add(Prop->GetComponentTransform());
    return F;
}

void AC26CameraDirector::Record(float Dt,const FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    RecordClock+=Dt;RecordAccumulator+=Dt;
    if(ReleasePending){ReleaseStamp=RecordClock;ReleasePending=false;}
    else if(ContactPending){ContactStamp=RecordClock;ContactPending=false;}
    if(RecordAccumulator<1.f/60.f)return;
    RecordAccumulator=0;
    Frames.Add(CaptureState(Ball,Actors));
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

bool AC26CameraDirector::PlayReplay(float Dt,FVector& Ball,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    if(Frames.Num()<8)return false;
    const bool Wicket=EventName==TEXT("WICKET");
    const bool Boundary=EventName==TEXT("SIX")||EventName==TEXT("FOUR");
    const float Window=ReplayEnd>0?ReplayEnd:Frames.Last().Time;

    // Generous broadcast replay window: starts 1.35s before release to capture the full rhythmic
    // approach, gather, explosive delivery, ball flight, and stroke impact with zero jump cuts.
    const float Start=FMath::Max(Frames[0].Time,ReleaseStamp>=0?ReleaseStamp-1.35f:Window-2.5f);
    const float Total=Window-Start;
    if(Total<.25f)return false;

    // Broadcast outro length: the replay holds and smoothly glides to a soft stop over 0.85s
    const float OutroDuration = 0.85f;

    if(!IsReplaying)
    {
        IsReplaying=true;
        IsReplayOutro=false;
        ReplayOutroTime=0.f;
        ReplayOutroAlpha=0.f;
        ReplayClock=Start;
        ReplayShot=0;
        Live=CaptureState(Ball,Actors);
    }

    // Check if main action replay reached the window end
    if(ReplayClock >= Window)
    {
        if(!IsReplayOutro)
        {
            IsReplayOutro = true;
            ReplayOutroTime = 0.f;
        }

        ReplayOutroTime += Dt;
        ReplayOutroAlpha = FMath::Clamp(ReplayOutroTime / OutroDuration, 0.f, 1.f);

        // Gently damp camera tracking velocity during outro so it doesn't freeze or hitch
        if(ReplayOutroTime >= OutroDuration)
        {
            return false; // Replay sequence completely and smoothly finished!
        }
    }
    else
    {
        // Event-tailored slow motion playback rate curve:
        // Run-up: 0.95x | Delivery stride: 0.45x | Contact/Wicket impact: 0.22x | Outfield flight: 0.58x | Outro: 0.75x
        float TargetRate = 0.68f;
        if (ReleaseStamp >= 0 && ReplayClock < ReleaseStamp - 0.25f)
        {
            TargetRate = 0.95f;
        }
        else if (ReleaseStamp >= 0 && ReplayClock < ReleaseStamp + 0.15f)
        {
            TargetRate = 0.45f;
        }
        else if (ContactStamp >= 0 && FMath::Abs(ReplayClock - ContactStamp) < 0.22f)
        {
            TargetRate = 0.22f;
        }
        else if (Wicket && ContactStamp < 0 && ReleaseStamp >= 0 && FMath::Abs(ReplayClock - (ReleaseStamp + 0.55f)) < 0.25f)
        {
            TargetRate = 0.20f;
        }
        else if (ReplayShot == 1)
        {
            TargetRate = 0.58f;
        }
        else
        {
            TargetRate = 0.72f;
        }
        PlaybackRate = FMath::FInterpTo(PlaybackRate, TargetRate, Dt, 4.5f);
        ReplayClock += Dt * PlaybackRate;
    }

    bool Cut=false;
    if(ReplayShot==0&&ContactStamp>=0&&ReplayClock>ContactStamp+.65f){ReplayShot=1;Cut=true;}
    else if(ReplayShot==0&&Wicket&&ContactStamp<0&&ReleaseStamp>=0&&ReplayClock>ReleaseStamp+.85f){ReplayShot=1;Cut=true;}
    if(ReplayShot==1&&ReplayEnd>ReplayClock+1.35f&&ReplayClock>(ContactStamp>=0?ContactStamp:ReleaseStamp)+1.5f)
    {ReplayClock=FMath::Max(ReplayClock,ReplayEnd-1.5f);ReplayShot=2;Cut=true;}

    const float SampleTime = FMath::Min(ReplayClock, Window);
    int Idx=0;
    for(int I=0;I<Frames.Num()-1;++I)
    {
        if(Frames[I].Time<=SampleTime&&Frames[I+1].Time>=SampleTime){Idx=I;break;}
    }
    const auto& A=Frames[Idx];const auto& B=Frames[FMath::Min(Idx+1,Frames.Num()-1)];
    const float Span=B.Time-A.Time;
    const float T=Span>UE_SMALL_NUMBER?FMath::Clamp((SampleTime-A.Time)/Span,0.f,1.f):0.f;
    Ball=FMath::Lerp(A.Ball,B.Ball,T);
    ApplyFrame(A,B,T,Actors);

    const FVector Anchor=ContactPoint.IsZero()?CameraStrikerMark+FVector(0,0,110):ContactPoint;

    if(ReplayShot==0 && ReleaseStamp>=0 && ReplayClock < ReleaseStamp + 0.15f)
    {
        Look(EC26CameraMode::ReplayPitch,FVector(-10.f,-2600.f,540.f),CameraStrikerMark+FVector(0,0,65.f),42.f,Mode!=EC26CameraMode::ReplayPitch,Dt,6.5f);
    }
    else if(ReplayShot==0)
    {
        const float InnerAe=ShotAerial?.55f:.20f,OuterAe=ShotAerial?.62f:.42f;
        const FVector Eye=FVector(Anchor.X,FMath::Min(Anchor.Y,StrikerEnd),0)+FVector(-620,-150,0)+FVector(0,0,FMath::Max(145.f,Anchor.Z+20.f));
        Look(EC26CameraMode::ReplayClose,Eye,
            FMath::Lerp(CameraStrikerMark+FVector(0,0,112),FMath::Lerp(Anchor,Ball,InnerAe),OuterAe),36,Cut||Mode==EC26CameraMode::ReplayPitch,Dt,6.0f,26.f);
    }
    else if(ReplayShot==1&&Wicket)
    {
        if(EventDetail==TEXT("BOWLED"))
        {
            // Reverse-pitch stump camera looking back toward the bowler through broken timber
            const FVector ReverseStumpEye(18.f,1140.f,58.f);
            const FVector ReverseStumpAim=FMath::Lerp(FVector(0.f,-400.f,120.f),Ball,0.20f);
            Look(EC26CameraMode::ReplayPitch,ReverseStumpEye,ReverseStumpAim,34.f,Cut,Dt,6.0f);
        }
        else if(EventDetail==TEXT("CAUGHT"))
        {
            // Fielder intercept camera: tightly framed on catching fielder
            const FVector FielderCam=EventFocus+FVector(320.f,-380.f,110.f);
            Look(EC26CameraMode::Catch,FielderCam,EventFocus+FVector(0.f,0.f,70.f),32.f,Cut,Dt,5.5f);
        }
        else
        {
            Look(EC26CameraMode::RunOut,EventFocus+FVector(-380.f,-420.f,95.f),FMath::Lerp(EventFocus+FVector(0.f,0.f,48.f),Ball,0.30f),35.f,Cut,Dt,5.5f);
        }
    }
    else if(ReplayShot==1&&EventName==TEXT("SIX"))
    {
        // High soaring tracking arc: elevated deep mid-wicket camera tracking ball into the illuminated upper stands
        const FVector SixCam=FVector(3800.f,1800.f,1400.f);
        const float ElevFov=FMath::Clamp(44.f-(Ball.Z-300.f)*0.008f,30.f,48.f);
        Look(EC26CameraMode::LoftedShotTracking,SixCam,Ball,ElevFov,Cut,Dt,5.0f,40.f);
    }
    else if(ReplayShot==1&&EventName==TEXT("FOUR"))
    {
        // Low turf rail camera: skimming across striped lawn chasing the skidding ball into LED boundary
        const FVector Trajectory=FVector(Ball.X-Striker.X,Ball.Y-Striker.Y,0.f).GetSafeNormal(UE_SMALL_NUMBER,FVector(0.f,1.f,0.f));
        const FVector Side(-Trajectory.Y,Trajectory.X,0.f);
        const FVector TurfCam=Ball-Trajectory*820.f+Side*360.f+FVector(0.f,0.f,38.f);
        Look(EC26CameraMode::BoundaryTracking,TurfCam,Ball,36.f,Cut,Dt,6.0f,18.f);
    }
    else if(ReplayShot==1)
    {
        Look(EC26CameraMode::ReplayPitch,FVector(-20.f,-1450.f,195.f),FMath::Lerp(Anchor,Ball,0.50f),34.f,Cut,Dt,5.5f);
    }
    else // ReplayShot == 2
    {
        if(Boundary)
        {
            const FVector Radial=FVector(Ball.X,Ball.Y,0).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,-1,0));
            Look(EC26CameraMode::ReplayBoundary,Ball-Radial*1450.f+FVector(-Radial.Y,Radial.X,0)*850.f+FVector(0,0,420.f),Ball,38.f,Cut,Dt,4.5f);
        }
        else if(Wicket)
        {
            Look(EC26CameraMode::Celebration,FVector(-20.f,-1100.f,160.f),FVector(-20.f,-700.f,120.f),32.f,Cut,Dt,4.0f);
        }
        else
        {
Look(
    EC26CameraMode::Celebration,
    Striker + FVector(300.f, -400.f, 170.f),
    Striker + FVector(0.f, -20.f, 125.f),
    32.f,
    Cut,
    Dt,
    4.0f
);
        }
    }
    return true;
}

void AC26CameraDirector::Restore(const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    if(IsReplaying)ApplyFrame(Live,Live,0,Actors);
    IsReplaying=false;
    IsReplayOutro=false;
    ReplayOutroTime=0.f;
    ReplayOutroAlpha=0.f;
    HaveCamera=false;
    LastPhase=EC26Phase::Menu;
    PlaybackRate=1;
    ReplayShot=0;
    bFieldPlanning=false;
}

void AC26CameraDirector::ApplyFrame(const FC26ReplayFrame& A,const FC26ReplayFrame& B,float T,const TArray<TObjectPtr<AC26Athlete>>& Actors)
{
    const int N=FMath::Min(Actors.Num(),FMath::Min(A.Athletes.Num(),B.Athletes.Num()));
    for(int I=0;I<N;++I)
    {
        const auto& AA=A.Athletes[I];const auto& BB=B.Athletes[I];
        auto* Actor=Actors[I].Get();
        if(!Actor)continue;
        FTransform Tr;
        Tr.SetLocation(FMath::Lerp(AA.Transform.GetLocation(),BB.Transform.GetLocation(),T));
        Tr.SetRotation(FQuat::Slerp(AA.Transform.GetRotation(),BB.Transform.GetRotation(),T));
        Tr.SetScale3D(FMath::Lerp(AA.Transform.GetScale3D(),BB.Transform.GetScale3D(),T));
        Actor->SetActorTransform(Tr);
        Actor->Action=T<.5f?AA.Action:BB.Action;
        Actor->DeliveryStyle=T<.5f?AA.DeliveryStyle:BB.DeliveryStyle;
        Actor->ActionTime=FMath::Lerp(AA.ActionTime,BB.ActionTime,T);
        Actor->MotionTime=FMath::Lerp(AA.MotionTime,BB.MotionTime,T);
        Actor->ShotAngle=FMath::Lerp(AA.ShotAngle,BB.ShotAngle,T);
        Actor->FootworkIntent=FMath::Lerp(AA.Footwork,BB.Footwork,T);
        Actor->StrideIntent=FMath::Lerp(AA.Stride,BB.Stride,T);
        Actor->Defending=T<.5f?AA.Defend:BB.Defend;
        Actor->Loft=T<.5f?AA.Loft:BB.Loft;
        Actor->ContactTarget=FMath::Lerp(AA.Contact,BB.Contact,T);
        Actor->LookAt=FMath::Lerp(AA.LookAt,BB.LookAt,T);
        Actor->MoveSpeed=FMath::Lerp(AA.MoveSpeed,BB.MoveSpeed,T);
        Actor->GaitPhase=FMath::Lerp(AA.Gait,BB.Gait,T);
        Actor->Trigger=FMath::Lerp(AA.Trigger,BB.Trigger,T);
        Actor->ShotLabel=T<.5f?AA.ShotLabel:BB.ShotLabel;
        if(Actor->Presentation&&Actor->Presentation->IsActive())Actor->Presentation->ApplyReplayPose(AA.CharacterPose,BB.CharacterPose,T);
        else Actor->Animate(0);
    }
    const int P=FMath::Min(ReplayProps.Num(),FMath::Min(A.Props.Num(),B.Props.Num()));
    for(int I=0;I<P;++I)
    {
        FTransform Tr;
        Tr.SetLocation(FMath::Lerp(A.Props[I].GetLocation(),B.Props[I].GetLocation(),T));
        Tr.SetRotation(FQuat::Slerp(A.Props[I].GetRotation(),B.Props[I].GetRotation(),T));
        Tr.SetScale3D(FMath::Lerp(A.Props[I].GetScale3D(),B.Props[I].GetScale3D(),T));
        if(ReplayProps[I])ReplayProps[I]->SetWorldTransform(Tr);
    }
}

void AC26CameraDirector::DirectPresentation(EC26CinematicCamera Lens, const FVector& FocusPrimary, const FVector& FocusSecondary, float NormalizedProgress, float Dt, bool bCut)
{
    FVector Eye = FVector::ZeroVector;
    FVector Aim = FocusPrimary + FVector(0, 0, 140.f);
    float Fov = 38.f;
    float TrackRate = 5.5f;

    FVector Separation = FocusSecondary - FocusPrimary;
    Separation.Z = 0.f;
    const float SepDist = Separation.Size();
    const FVector Forward = SepDist > 10.f ? (Separation / SepDist) : FVector(0, 1, 0);
    const FVector Right = FVector(-Forward.Y, Forward.X, 0.f);

    switch (Lens)
    {
    case EC26CinematicCamera::StadiumWide:
    {
        Eye = FVector(2800.f, -5400.f, 3200.f);
        Aim = FocusPrimary.IsZero() ? FVector(0.f, 0.f, 100.f) : FocusPrimary + FVector(0, 0, 80.f);
        Fov = 56.f;
        TrackRate = 3.5f;
        break;
    }
    case EC26CinematicCamera::HighAngleToss:
    {
        Eye = FocusPrimary + FVector(220.f, -280.f, 480.f);
        Aim = FocusPrimary + FVector(0.f, 0.f, 90.f);
        Fov = 42.f;
        TrackRate = 4.5f;
        break;
    }
    case EC26CinematicCamera::TwoShot:
    {
        const FVector Midpoint = (FocusPrimary + FocusSecondary) * 0.5f;
        const float OffsetSide = FMath::Max(320.f, SepDist * 1.5f);
        Eye = Midpoint + Right * OffsetSide + FVector(0, 0, 145.f);
        Aim = Midpoint + FVector(0, 0, 135.f);
        Fov = 35.f;
        TrackRate = 5.0f;
        break;
    }
    case EC26CinematicCamera::CloseUpFace:
    {
        const FVector CamDir = Forward.IsZero() ? FVector(0, -1, 0) : -Forward;
        Eye = FocusPrimary + CamDir * 190.f + Right * 35.f + FVector(0, 0, 160.f);
        Aim = FocusPrimary + FVector(0, 0, 165.f);
        Fov = 26.f;
        TrackRate = 6.0f;
        break;
    }
    case EC26CinematicCamera::LowAngleDramatic:
    {
        const FVector CamDir = Forward.IsZero() ? FVector(0, -1, 0) : -Forward;
        Eye = FocusPrimary + CamDir * 230.f + Right * 40.f + FVector(0, 0, 32.f);
        Aim = FocusPrimary + FVector(0, 0, 150.f);
        Fov = 42.f;
        TrackRate = 5.0f;
        break;
    }
    case EC26CinematicCamera::OrbitCelebration:
    {
        const float Angle = NormalizedProgress * 2.f * PI * 0.65f + 0.4f;
        const float OrbitRadius = 340.f;
        Eye = FocusPrimary + FVector(FMath::Cos(Angle) * OrbitRadius, FMath::Sin(Angle) * OrbitRadius, 155.f);
        Aim = FocusPrimary + FVector(0, 0, 135.f);
        Fov = 36.f;
        TrackRate = 7.0f;
        break;
    }
    case EC26CinematicCamera::OverShoulderBatter:
    {
        Eye = FocusPrimary - Forward * 180.f + Right * 55.f + FVector(0, 0, 168.f);
        Aim = FocusSecondary + FVector(0, 0, 140.f);
        Fov = 32.f;
        TrackRate = 5.5f;
        break;
    }
    case EC26CinematicCamera::OverShoulderBowler:
    {
        Eye = FocusPrimary - Forward * 190.f + Right * 55.f + FVector(0, 0, 175.f);
        Aim = FocusSecondary + FVector(0, 0, 135.f);
        Fov = 30.f;
        TrackRate = 5.5f;
        break;
    }
    case EC26CinematicCamera::PitchTrackWalking:
    {
        const float TrackY = FMath::Lerp(-400.f, 400.f, NormalizedProgress);
        Eye = FVector(FocusPrimary.X + 360.f, FocusPrimary.Y + TrackY, 150.f);
        Aim = FocusPrimary + FVector(0, 0, 130.f);
        Fov = 38.f;
        TrackRate = 4.5f;
        break;
    }
    case EC26CinematicCamera::UmpirePOV:
    {
        Eye = FocusPrimary + FVector(0.f, -50.f, 178.f);
        Aim = FocusSecondary + FVector(0.f, 0.f, 130.f);
        Fov = 36.f;
        TrackRate = 6.0f;
        break;
    }
    case EC26CinematicCamera::DugoutReaction:
    {
        Eye = FVector(-4400.f, -2700.f, 210.f);
        Aim = FVector(-4100.f, -2400.f, 145.f);
        Fov = 28.f;
        TrackRate = 4.0f;
        break;
    }
    default:
    {
        Eye = FocusPrimary + FVector(350.f, -400.f, 200.f);
        Aim = FocusPrimary + FVector(0, 0, 130.f);
        Fov = 40.f;
        break;
    }
    }

    Look(EC26CameraMode::Presentation, Eye, Aim, Fov, bCut, Dt, TrackRate);
}
