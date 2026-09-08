#include "C26Simulation.h"
DEFINE_LOG_CATEGORY(LogC26);

FC26Simulation::FC26Simulation()
{
    for(int I=0;I<128;++I) BoundaryPolygon.Add(C26Field::RopePoint(I*2.f*PI/128.f));
}
void FC26Simulation::Reset(){Ball={};CrossedContact=BounceEvent=StumpEvent=BoundaryEvent=false;ContactTime=0;}
void FC26Simulation::Release(const FC26DeliveryPlan& InPlan,const FVector& Origin)
{
    Reset();Plan=InPlan;Ball.Position=Origin;Ball.Active=true;
    const float Time=FMath::Max(.18f,(Plan.Length-Origin.Y)/Plan.Speed);
    Ball.Velocity=FVector((Plan.Line-Origin.X)/Time-.5f*Plan.Swing*Time,Plan.Speed,
        (Tuning.BallRadius-Origin.Z+.5f*Tuning.Gravity*Time*Time)/Time);
    FC26BallState P=Ball;
    for(int I=0;I<600;++I){Integrate(P,1.f/600.f);if(P.Position.Y>=C26Field::ContactY){ContactTime=P.Age;ContactPosition=P.Position;break;}}
}
void FC26Simulation::Integrate(FC26BallState& S,float Dt) const
{
    if(!S.Active)return;
    S.Age+=Dt;
    const float G=Tuning.Gravity;
    if(S.Position.Z<=Tuning.BallRadius+.1f && FMath::Abs(S.Velocity.Z)<45.f && S.Struck)
    {
        S.Velocity.Z=0;S.Position.Z=Tuning.BallRadius;
        const FVector Flat(S.Velocity.X,S.Velocity.Y,0);
        S.Velocity=Flat.GetSafeNormal()*FMath::Max(0.f,Flat.Size()-Tuning.GrassDeceleration*Dt);
    }
    else S.Velocity.Z-=G*Dt;
    if(!S.Struck&&!S.Bounced)S.Velocity.X+=Plan.Swing*Dt;
    S.Position+=S.Velocity*Dt;
    if(S.Position.Z<Tuning.BallRadius)
    {
        S.Position.Z=Tuning.BallRadius;
        if(!S.Struck&&!S.Bounced){S.Velocity.Z=FMath::Abs(S.Velocity.Z)*Plan.Bounce;S.Velocity.Y*=.9f;S.Velocity.X+=Plan.Seam;}
        else{S.Velocity.Z=FMath::Abs(S.Velocity.Z)*.38f;S.Velocity.X*=.81f;S.Velocity.Y*=.81f;}
        S.Bounced=true;if(S.Struck)S.PostHitBounce=true;
    }
}
bool FC26Simulation::CrossesRope(const FVector& From,const FVector& To) const
{
    // The same 128-segment polygon is drawn by the venue. Segment intersection prevents tunnelling.
    const FVector2D P(From.X,From.Y),R(To.X-From.X,To.Y-From.Y);
    for(int I=0;I<BoundaryPolygon.Num();++I)
    {
        const FVector& A=BoundaryPolygon[I];const FVector& B=BoundaryPolygon[(I+1)%BoundaryPolygon.Num()];
        const FVector2D Q(A.X,A.Y),S(B.X-A.X,B.Y-A.Y);
        const float D=R.X*S.Y-R.Y*S.X;if(FMath::Abs(D)<.001f)continue;
        const FVector2D QP=Q-P;
        const float T=(QP.X*S.Y-QP.Y*S.X)/D,U=(QP.X*R.Y-QP.Y*R.X)/D;
        if(T>=0&&T<=1&&U>=0&&U<=1)return true;
    }return false;
}
void FC26Simulation::Step(float Dt)
{
    BounceEvent=false;StumpEvent=false;BoundaryEvent=false;
    const int Steps=FMath::Max(1,FMath::CeilToInt(Dt*240.f));const float H=Dt/Steps;
    for(int I=0;I<Steps&&Ball.Active;++I)
    {
        const FC26BallState Prev=Ball;Integrate(Ball,H);
        BounceEvent|=(!Prev.Bounced&&Ball.Bounced)||(!Prev.PostHitBounce&&Ball.PostHitBounce);
        if(!Ball.Struck&&Prev.Position.Y<C26Field::ContactY&&Ball.Position.Y>=C26Field::ContactY)CrossedContact=true;
        if(Prev.Position.Y<C26Field::WicketY&&Ball.Position.Y>=C26Field::WicketY)
        {
            const FVector P=FMath::Lerp(Prev.Position,Ball.Position,(C26Field::WicketY-Prev.Position.Y)/(Ball.Position.Y-Prev.Position.Y));
            if(FMath::Abs(P.X)<15.f&&P.Z<76.f){StumpEvent=true;Ball.Position=P;Ball.Active=false;}
        }
        if(CrossesRope(Prev.Position,Ball.Position)){BoundaryEvent=true;Ball.Active=false;}
    }
}
FC26BallState FC26Simulation::Predict(float Seconds) const
{
    FC26BallState S=Ball;const float H=1.f/120.f;
    for(float T=0;T<Seconds;T+=H)Integrate(S,FMath::Min(H,Seconds-T));return S;
}
FVector FC26Simulation::PredictLanding(float MaxSeconds) const
{
    FC26BallState S=Ball;
    for(float T=0;T<MaxSeconds;T+=.025f){Integrate(S,.025f);if(S.Position.Z<20.f&&S.Velocity.Z<=0)return S.Position;}
    return S.Position;
}
FC26Contact FC26Simulation::Hit(const FC26ShotIntent& Intent,float Error,int Difficulty,FRandomStream& Random)
{
    FC26Contact C;
    const float DifficultyScale=Difficulty==0?1.35f:Difficulty==2?.8f:1.f;
    const float Reach=FMath::Abs(Ball.Position.X-Intent.Footwork*42.f);
    const float Window=Tuning.ContactWindow*DifficultyScale;
    if(FMath::Abs(Error)>Window||Reach>105.f||Ball.Position.Z>195.f||Ball.Position.Z<2.f)return C;
    const float Spatial=1.f-FMath::Clamp((Reach-15.f)/110.f,0.f,.8f);
    const float Temporal=1.f-FMath::Clamp(FMath::Abs(Error)/Window,0.f,1.f);
    C.Quality=Spatial*Temporal;
    if(FMath::Abs(Error)<Tuning.PerfectWindow*DifficultyScale&&Spatial>.83f)C.Timing=EC26Timing::Perfect;
    else if(FMath::Abs(Error)<Tuning.GoodWindow*DifficultyScale&&Spatial>.6f)C.Timing=EC26Timing::Good;
    else C.Timing=Error<0?EC26Timing::Early:EC26Timing::Late;
    const bool Edge=(Reach>77.f||Temporal<.24f)&&!Intent.Defend;
    if(Edge)C.Timing=EC26Timing::Edge;
    float Angle=FMath::DegreesToRadians(Intent.Angle+FMath::Clamp(Error*155.f,-28.f,28.f));
    float Speed=(1650.f+Intent.Power*2400.f)*(.4f+.6f*C.Quality)*Tuning.BatPower;
    float Elevation=Intent.Loft?FMath::Lerp(26.f,44.f,C.Quality):FMath::Lerp(3.f,9.f,Intent.Power);
    if(Intent.Defend){Speed=450.f+300.f*C.Quality;Elevation=5.f;C.Shot=TEXT("DEFENSIVE BLOCK");}
    else if(Ball.Position.Z>112.f){C.Shot=FMath::Abs(Intent.Angle)>55?TEXT("HOOK"):TEXT("PULL");Elevation+=8;}
    else if(Intent.Angle>60)C.Shot=TEXT("CUT");
    else if(Intent.Angle>24)C.Shot=Intent.Loft?TEXT("LOFTED COVER DRIVE"):TEXT("COVER DRIVE");
    else if(Intent.Angle<-65)C.Shot=TEXT("LEG GLANCE");
    else if(Intent.Angle<-25)C.Shot=Intent.Loft?TEXT("SLOG"):TEXT("FLICK");
    else C.Shot=Intent.Loft?TEXT("LOFTED STRAIGHT DRIVE"):TEXT("STRAIGHT DRIVE");
    if(Edge){Angle=FMath::DegreesToRadians(Random.FRandRange(125.f,165.f)*(Ball.Position.X>=0?1:-1));Speed*=.52f;Elevation=Random.FRandRange(12.f,43.f);C.Shot=TEXT("EDGE");}
    const float E=FMath::DegreesToRadians(Elevation);
    C.Velocity=FVector(FMath::Sin(Angle)*FMath::Cos(E),-FMath::Cos(Angle)*FMath::Cos(E),FMath::Sin(E))*Speed;
    Ball.Struck=true;Ball.PostHitBounce=false;Ball.Velocity=C.Velocity;
    return C;
}
FC26DeliveryPlan FC26AI::Bowl(const C26::Match& Rules,int Difficulty)
{
    FC26DeliveryPlan P;const int B=Rules.Now().LegalBalls;
    const float R=Random.FRand();
    if(B>=4&&R<.52f)P.Type=EC26Delivery::Yorker;
    else if(History.Boundaries>0&&R<.45f)P.Type=EC26Delivery::Slower;
    else if(B>0&&R>.8f)P.Type=EC26Delivery::Bouncer;
    else P.Type=R<.3f?EC26Delivery::Inswing:R<.6f?EC26Delivery::Outswing:EC26Delivery::Pace;
    P.Speed=Random.FRandRange(3020.f,3540.f)+(Difficulty-1)*120;
    P.Length=P.Type==EC26Delivery::Yorker?810:P.Type==EC26Delivery::Bouncer?40:430;
    if(P.Type==EC26Delivery::Slower){P.Speed=2450;P.Length=570;}
    P.Bounce=P.Type==EC26Delivery::Bouncer?.72f:.55f;
    P.Line=Random.FRandRange(-20.f,35.f)+History.OffsideBias*20;
    P.Swing=P.Type==EC26Delivery::Outswing?180:P.Type==EC26Delivery::Inswing?-180:0;
    P.Seam=Random.FRandRange(-28,28);
    P.Length+=Random.FRandRange(-45.f,45.f);
    // Occasional genuine accuracy errors, not forced score outcomes.
    if(Random.FRand()<.025f)P.Line+=125;
    return P;
}
FC26ShotIntent FC26AI::Bat(const FC26DeliveryPlan& Delivery,const C26::Match& Rules,int Difficulty)
{
    FC26ShotIntent S;
    const float Required=Rules.Current==1?float(Rules.RunsRequired())/FMath::Max(1,Rules.BallsRemaining()):2.6f;
    const float Risk=FMath::Clamp(.32f+Required*.115f-Rules.Now().Wickets*.15f,.25f,.92f);
    const bool Short=Delivery.Length<180.f,Full=Delivery.Length>670.f;
    S.Angle=Short?Random.FRandRange(-105.f,-50.f):Delivery.Line>15?Random.FRandRange(28.f,72.f):Random.FRandRange(-42.f,32.f);
    S.Power=FMath::Clamp(.57f+Required*.055f+Random.FRandRange(-.13f,.13f),.4f,1.f);
    S.Loft=Random.FRand()<Risk*(Full?.55f:1.f);
    S.Defend=Full&&Required<1.8f&&Random.FRand()<.28f;
    S.Footwork=FMath::Clamp(Delivery.Line/65.f,-1.f,1.f);
    if(Difficulty==0)S.Footwork*=.65f;
    return S;
}
float FC26AI::TimingError(int Difficulty)
{
    const float Sigma=Difficulty==0?.12f:Difficulty==2?.053f:.085f;
    // Triangular timing distribution; independent of predicted contact/outcome.
    return (Random.FRand()+Random.FRand()-1.f)*Sigma*1.8f;
}
TArray<FVector> FC26AI::Field(bool ProtectOffside)
{
    TArray<FVector> P={FVector(0,-1700,0),FVector(0,1320,0),FVector(2600,400,0),FVector(2600,-1800,0),
        FVector(1100,-5400,0),FVector(-1600,-5400,0),FVector(-3700,-3300,0),FVector(-3600,200,0),
        FVector(-2400,4500,0),FVector(3100,4300,0),FVector(4950,-2100,0)};
    if(ProtectOffside){P[6]=FVector(4300,-3500,0);P[8]=FVector(-2100,1800,0);}return P;
}
