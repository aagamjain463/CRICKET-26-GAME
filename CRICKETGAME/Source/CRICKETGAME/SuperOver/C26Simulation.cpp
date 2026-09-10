#include "C26Simulation.h"
#include "C26Delivery.h"
DEFINE_LOG_CATEGORY(LogC26);

FC26Simulation::FC26Simulation()
{
    for(int I=0;I<128;++I) BoundaryPolygon.Add(C26Field::RopePoint(I*2.f*PI/128.f));
}
void FC26Simulation::Reset(){Ball={};CrossedContact=BounceEvent=StumpEvent=BoundaryEvent=false;ContactTime=0;ContactPosition=FVector::ZeroVector;}
void FC26Simulation::Release(const FC26DeliveryPlan& InPlan,const FVector& Origin)
{
    Reset();Plan=InPlan;Ball.Position=Origin;Ball.Active=true;
    Plan.Speed=FMath::Clamp(Plan.Speed,1600.f,4600.f);
    Plan.Bounce=FMath::Clamp(Plan.Bounce,.2f,.85f);
    Ball.Spin=FVector(Plan.Speed/Tuning.BallRadius*.12f,0,Plan.Swing*.03f);
    const float Time=FMath::Max(.18f,(Plan.Length-Origin.Y)/Plan.Speed);
    Ball.Velocity=FVector((Plan.Line-Origin.X)/Time-.5f*Plan.Swing*Time,Plan.Speed,
        (Tuning.BallRadius-Origin.Z+.5f*Tuning.Gravity*Time*Time)/Time);
    FC26BallState P=Ball;
    for(int I=0;I<720;++I)
    {
        const auto Prev=P;Integrate(P,1.f/240.f);
        if(P.Position.Y>=C26Field::ContactY)
        {
            const float Fraction=(C26Field::ContactY-Prev.Position.Y)/(P.Position.Y-Prev.Position.Y);
            ContactTime=FMath::Lerp(Prev.Age,P.Age,Fraction);
            ContactPosition=FMath::Lerp(Prev.Position,P.Position,Fraction);
            ContactPosition.Y=C26Field::ContactY;break;
        }
    }
}
void FC26Simulation::Integrate(FC26BallState& S,float Dt) const
{
    if(!S.Active||Dt<=0)return;
    S.Age+=Dt;
    const float G=FMath::Max(1.f,Tuning.Gravity),Radius=Tuning.BallRadius;
    float Remaining=Dt;
    // Analytic motion and exact time of ground impact avoid frame-rate-dependent bounce height.
    for(int Impact=0;Impact<4&&Remaining>SMALL_NUMBER;++Impact)
    {
        if(S.Position.Z<=Radius+.05f&&FMath::Abs(S.Velocity.Z)<45.f&&S.Struck)
        {
            S.Position.Z=Radius;S.Velocity.Z=0;
            const float Speed=S.Velocity.Size2D(),Decel=FMath::Max(1.f,Tuning.GrassDeceleration);
            const float MoveTime=FMath::Min(Remaining,Speed/Decel);
            const FVector Direction=S.Velocity.GetSafeNormal();
            S.Position+=Direction*(Speed*MoveTime-.5f*Decel*MoveTime*MoveTime);
            S.Velocity=Direction*FMath::Max(0.f,Speed-Decel*Remaining);
            S.Spin=FVector(-S.Velocity.Y,S.Velocity.X,0)/FMath::Max(1.f,Radius);
            break;
        }
        const float Height=FMath::Max(0.f,float(S.Position.Z)-Radius);
        const float ImpactTime=(S.Velocity.Z+FMath::Sqrt(S.Velocity.Z*S.Velocity.Z+2.f*G*Height))/G;
        const bool HitsGround=ImpactTime>=0&&ImpactTime<=Remaining;
        const float H=HitsGround?ImpactTime:Remaining;
        const float Swing=!S.Struck&&!S.Bounced?Plan.Swing:0.f;
        if(S.Struck)
        {
            // Restrained air drag; the actual delivery pace is never slowed for readability.
            const float Drag=.035f,Decay=FMath::Exp(-Drag*H);
            S.Position.X+=S.Velocity.X*(1.f-Decay)/Drag;
            S.Position.Y+=S.Velocity.Y*(1.f-Decay)/Drag;
            S.Velocity.X*=Decay;S.Velocity.Y*=Decay;
        }
        else
        {
            S.Position.X+=S.Velocity.X*H+.5f*Swing*H*H;
            S.Position.Y+=S.Velocity.Y*H;S.Velocity.X+=Swing*H;
        }
        S.Position.Z+=S.Velocity.Z*H-.5f*G*H*H;S.Velocity.Z-=G*H;
        Remaining-=H;
        if(!HitsGround)break;
        S.Position.Z=Radius;
        if(!S.Struck&&!S.Bounced)
        {
            S.Velocity.Z=FMath::Abs(S.Velocity.Z)*Plan.Bounce;
            S.Velocity.Y*=.9f;S.Velocity.X=S.Velocity.X*.92f+Plan.Seam;
        }
        else
        {
            S.Velocity.Z=FMath::Abs(S.Velocity.Z)*.38f;
            S.Velocity.X*=.81f;S.Velocity.Y*=.81f;S.Spin*=.7f;
        }
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
    if(!FMath::IsFinite(Dt)||Dt<=0)return;
    const int Steps=FMath::Max(1,FMath::CeilToInt(Dt*240.f));const float H=Dt/Steps;
    for(int I=0;I<Steps&&Ball.Active;++I)
    {
        const FC26BallState Prev=Ball;Integrate(Ball,H);
        BounceEvent|=(!Prev.Bounced&&Ball.Bounced)||(!Prev.PostHitBounce&&Ball.PostHitBounce);
        if(!Ball.Struck&&Prev.Position.Y<C26Field::ContactY&&Ball.Position.Y>=C26Field::ContactY)CrossedContact=true;
        for(int End=-1;End<=1;End+=2)
        {
            const float Y=End*C26Field::WicketY,Travel=Ball.Position.Y-Prev.Position.Y;
            if(FMath::Abs(Travel)<SMALL_NUMBER)continue;
            const float T=(Y-Prev.Position.Y)/Travel;
            if(T<0||T>1)continue;
            const FVector P=FMath::Lerp(Prev.Position,Ball.Position,T);
            if(FMath::Abs(P.X)<C26Field::WicketWidth*.5f+Tuning.BallRadius&&P.Z>=0&&P.Z<C26Field::StumpHeight+Tuning.BallRadius)
            {StumpEvent=true;Ball.Position=P;Ball.Active=false;break;}
        }
        if(Ball.Active&&CrossesRope(Prev.Position,Ball.Position)){BoundaryEvent=true;Ball.Active=false;}
    }
}
FC26BallState FC26Simulation::Predict(float Seconds) const
{
    FC26BallState S=Ball;const float H=1.f/240.f;
    for(float T=0;T<Seconds;T+=H)Integrate(S,FMath::Min(H,Seconds-T));return S;
}
void FC26Simulation::Forecast(TArray<FC26BallState>& Out,float Horizon,float Interval) const
{
    Out.Reset();if(Horizon<=0||Interval<=0)return;
    Horizon=FMath::Min(Horizon,12.f);Interval=FMath::Max(Interval,1.f/60.f);
    Out.Reserve(FMath::CeilToInt(Horizon/Interval));
    FC26BallState S=Ball;
    for(float Time=0;Time<Horizon;)
    {
        const float Duration=FMath::Min(Interval,Horizon-Time);
        for(float T=0;T<Duration;)
        {const float H=FMath::Min(1.f/240.f,Duration-T);Integrate(S,H);T+=H;}
        Time+=Duration;Out.Add(S);
        if(!C26Field::Inside(S.Position)||S.Velocity.IsNearlyZero(.1f))break;
    }
}
FVector FC26Simulation::PredictLanding(float MaxSeconds) const
{
    FC26BallState S=Ball;
    for(float T=0;T<MaxSeconds;T+=1.f/240.f){Integrate(S,1.f/240.f);if(S.Position.Z<20.f&&S.Velocity.Z<=0)return S.Position;}
    return S.Position;
}
FC26Contact FC26Simulation::Hit(const FC26ShotIntent& Intent,float Error,int Difficulty,FRandomStream& Random)
{
    (void)Random; // The contact result is entirely geometric; randomness belongs to AI intent.
    FC26Contact C;C.ContactPoint=Ball.Position;
    if(!Ball.Active||Ball.Struck||!FMath::IsFinite(Error))return C;
    const float DifficultyScale=Difficulty==0?1.35f:Difficulty==2?.8f:1.f;
    const float Offset=Ball.Position.X-FMath::Clamp(Intent.Footwork,-1.f,1.f)*42.f;
    const float Reach=FMath::Abs(Offset),Height=Ball.Position.Z;
    const float Window=Tuning.ContactWindow*DifficultyScale;
    if(FMath::Abs(Error)>Window||Reach>105.f||Height>195.f||Height<2.f)return C;
    const float Spatial=1.f-FMath::Clamp((Reach-15.f)/110.f,0.f,.8f);
    const float Temporal=1.f-FMath::Clamp(FMath::Abs(Error)/Window,0.f,1.f);
    const float Power=FMath::Clamp(Intent.Power,0.f,1.f),Stride=FMath::Clamp(Intent.Stride,-1.f,1.f);
    float ShotAngle=FMath::Clamp(Intent.Angle,-135.f,135.f);
    const bool Short=Height>108.f||Plan.Length<180.f;
    const bool Yorker=Height<27.f&&Plan.Length>710.f;
    C.Suitability=1.f;
    // A front press helps a full ball; a back-foot transfer buys space for a short ball.
    const float IdealStride=Short?-.8f:Plan.Length>560.f?.75f:.25f;
    C.Suitability-=FMath::Abs(Stride-IdealStride)*.13f;
    if(Offset>35.f&&ShotAngle<-30.f)C.Suitability-=.28f;
    if(Offset<-30.f&&ShotAngle>45.f)C.Suitability-=.25f;
    if(Intent.Defend)
    {
        C.Shot=Short?TEXT("BACK-FOOT DEFENCE"):Yorker?TEXT("YORKER BLOCK"):TEXT("DEFENSIVE PUSH");
        ShotAngle=FMath::Clamp(ShotAngle,-45.f,45.f);
    }
    else if(Short)
    {
        if(FMath::Abs(ShotAngle)<38.f){C.Suitability-=.26f;ShotAngle=Offset>20.f?72.f:-68.f;}
        if(ShotAngle>0)C.Shot=Height>148.f?TEXT("UPPER CUT"):TEXT("SQUARE CUT");
        else C.Shot=Height>148.f?TEXT("HOOK"):TEXT("PULL");
    }
    else if(Yorker)
    {
        if(FMath::Abs(ShotAngle)>60.f)C.Suitability-=.32f;
        if(Intent.Loft)C.Suitability-=.25f;
        ShotAngle=FMath::Clamp(ShotAngle,-55.f,55.f);C.Shot=TEXT("DUG-OUT DRIVE");
    }
    else if(ShotAngle>78.f){C.Shot=TEXT("LATE CUT");if(Height<45.f)C.Suitability-=.22f;}
    else if(ShotAngle>55.f){C.Shot=Stride<-.3f?TEXT("BACK-FOOT PUNCH"):TEXT("EXTRA-COVER DRIVE");}
    else if(ShotAngle>22.f)C.Shot=Intent.Loft?TEXT("LOFTED COVER DRIVE"):TEXT("COVER DRIVE");
    else if(ShotAngle<-75.f)C.Shot=TEXT("LEG GLANCE");
    else if(ShotAngle<-34.f)C.Shot=Intent.Loft?TEXT("LEG-SIDE PICKUP"):TEXT("FLICK");
    else if(ShotAngle<-15.f)C.Shot=TEXT("ON DRIVE");
    else C.Shot=Intent.Loft?TEXT("LOFTED STRAIGHT DRIVE"):TEXT("STRAIGHT DRIVE");
    C.Suitability=FMath::Clamp(C.Suitability,.22f,1.f);
    C.Quality=Spatial*Temporal*C.Suitability;
    if(FMath::Abs(Error)<Tuning.PerfectWindow*DifficultyScale&&Spatial>.83f&&C.Suitability>.80f)
    {C.Timing=EC26Timing::Perfect;C.ContactType=EC26ContactType::Perfect;}
    else if(FMath::Abs(Error)<Tuning.GoodWindow*DifficultyScale&&Spatial>.6f&&C.Suitability>.57f)
    {C.Timing=EC26Timing::Good;C.ContactType=EC26ContactType::Good;}
    else
    {C.Timing=Error<0?EC26Timing::Early:EC26Timing::Late;C.ContactType=Error<0?EC26ContactType::Early:EC26ContactType::Late;}
    C.FaceAngle=ShotAngle+FMath::Clamp(Error*155.f,-28.f,28.f);
    float Speed=(1450.f+Power*2600.f)*(.38f+.62f*C.Quality)*Tuning.BatPower;
    float Elevation=Intent.Loft&&!Yorker?FMath::Lerp(26.f,44.f,C.Quality):FMath::Lerp(2.f,7.f,Power);
    if(Intent.Defend){Speed=360.f+300.f*C.Quality;Elevation=3.f;}
    else if(Short&&Intent.Loft)Elevation+=5.f;
    // Thin contact comes from reach, timing and an unsuitable bat face, never an enum roll.
    const bool Thin=Reach>77.f||Temporal<.22f||C.Suitability<.46f;
    if(Thin)
    {
        C.Timing=EC26Timing::Edge;Speed*=.48f;
        if(Short&&(Intent.Loft||C.Suitability<.46f))
        {C.ContactType=EC26ContactType::TopEdge;C.Shot=TEXT("TOP EDGE");Elevation=58.f+12.f*(1.f-Temporal);}
        else if(Yorker||Height<18.f)
        {C.ContactType=EC26ContactType::BottomEdge;C.Shot=TEXT("BOTTOM EDGE");Elevation=-9.f;C.FaceAngle*=.6f;}
        else if(Offset< -20.f||(Error<0&&Reach<60.f))
        {C.ContactType=EC26ContactType::InsideEdge;C.Shot=TEXT("INSIDE EDGE");C.FaceAngle=-154.f+FMath::Clamp(Offset*.12f,-12.f,12.f);Elevation=6.f+Temporal*12.f;}
        else
        {C.ContactType=EC26ContactType::OutsideEdge;C.Shot=TEXT("OUTSIDE EDGE");C.FaceAngle=145.f+FMath::Clamp(Offset*.14f,-10.f,17.f);Elevation=9.f+(1.f-Temporal)*25.f;}
    }
    const float Angle=FMath::DegreesToRadians(C.FaceAngle);
    const float E=FMath::DegreesToRadians(Elevation);
    C.Velocity=FVector(FMath::Sin(Angle)*FMath::Cos(E),-FMath::Cos(Angle)*FMath::Cos(E),FMath::Sin(E))*Speed;
    Ball.Struck=true;Ball.PostHitBounce=false;Ball.Velocity=C.Velocity;
    Ball.Spin=FVector(-C.Velocity.Y,C.Velocity.X,0).GetSafeNormal()*(Intent.Loft?-35.f:55.f)+FVector(0,0,Error*100.f);
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
    if(P.Type==History.LastDelivery&&History.RepeatedDelivery>=1)
        P.Type=P.Type==EC26Delivery::Outswing?EC26Delivery::Slower:EC26Delivery::Outswing;
    History.RepeatedDelivery=P.Type==History.LastDelivery?History.RepeatedDelivery+1:0;
    History.LastDelivery=P.Type;++History.PlannedBalls;
    P.Speed=Random.FRandRange(3500.f,3900.f)+(Difficulty-1)*100;
    P.Length=P.Type==EC26Delivery::Yorker?810:P.Type==EC26Delivery::Bouncer?40:430;
    if(P.Type==EC26Delivery::Slower){P.Speed=2450;P.Length=570;}
    P.Bounce=P.Type==EC26Delivery::Bouncer?.72f:.55f;
    const bool DefendingTarget=Rules.Current==1&&Rules.RunsRequired()>Rules.BallsRemaining()*3;
    P.Line=Random.FRandRange(-20.f,35.f)+History.OffsideBias*20+(DefendingTarget?14.f:0.f);
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
    S.Stride=Short?-.8f:Full?.8f:.3f;
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
    TArray<FVector> P={FVector(-20,-2700,5),FVector(0,2450,5),FVector(2600,400,0),FVector(2600,-1800,0),
        FVector(1100,-5400,0),FVector(-1600,-5400,0),FVector(-3700,-3300,0),FVector(-3600,200,0),
        FVector(-2400,4500,0),FVector(3100,4300,0),FVector(4950,-2100,0)};
    if(ProtectOffside){P[6]=FVector(4300,-3500,0);P[8]=FVector(-2100,1800,0);}return P;
}
