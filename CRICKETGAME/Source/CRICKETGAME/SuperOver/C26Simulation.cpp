#include "C26Simulation.h"
#include "C26Controls.h"
#include "C26Delivery.h"
DEFINE_LOG_CATEGORY(LogC26);

FC26Simulation::FC26Simulation()
{
    for(int I=0;I<128;++I) BoundaryPolygon.Add(C26Field::RopePoint(I*2.f*PI/128.f));
}
void FC26Simulation::Reset(){Ball={};CrossedContact=BounceEvent=StumpEvent=BoundaryEvent=false;ContactTime=0;ContactPosition=FVector::ZeroVector;BounceTime=-1.f;BouncePosition=FVector::ZeroVector;}
void FC26Simulation::Release(const FC26DeliveryPlan& InPlan,const FVector& Origin)
{
    Reset();Plan=InPlan;Ball.Position=Origin;Ball.Active=true;
    Plan.Speed=FMath::Clamp(Plan.Speed,1600.f,4600.f);
    Plan.Bounce=FMath::Clamp(Plan.Bounce,.2f,.85f);
    Ball.Spin=FVector(Plan.Speed/Tuning.BallRadius*.12f,0,Plan.Swing*.03f);
    const float Time=FMath::Max(.18f,(Plan.Length-Origin.Y)/Plan.Speed);
    // Reverse swing's onset is authored as a FRACTION of the flight, so it stays
    // late whatever the pace or the length. Resolve it to seconds here, where
    // the flight time is actually known, and only in this simulation's own copy
    // of the plan - the caller's intent is never rewritten.
    if(Plan.SwingOnset>0.f)
        Plan.SwingOnset=FMath::Clamp(Plan.SwingOnset,0.f,.9f)*Time;
    // Trim the launch by exactly the lateral displacement this movement is about
    // to produce, so the ball touches down on the aim point whether the movement
    // runs the whole flight (conventional swing) or only its last part (reverse).
    const float Deflect=C26Delivery::SwingDeflection(Plan.Swing,Time,Plan.SwingOnset);
    Ball.Velocity=FVector((Plan.Line-Origin.X)/Time-Deflect/Time,Plan.Speed,
        (Tuning.BallRadius-Origin.Z+.5f*Tuning.Gravity*Time*Time)/Time);
    FC26BallState P=Ball;
    bool bBounceFound=false;
    for(int I=0;I<720;++I)
    {
        const auto Prev=P;Integrate(P,1.f/240.f);
        if(!bBounceFound && !Prev.Bounced && P.Bounced)
        {
            BounceTime=P.Age;
            BouncePosition=FVector(P.Position.X,P.Position.Y,Tuning.BallRadius);
            bBounceFound=true;
        }
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
            S.Position+=S.Velocity*MoveTime;
            const float NewSpeed=FMath::Max(0.f,Speed-Decel*MoveTime);
            S.Velocity=Speed>SMALL_NUMBER?(S.Velocity/Speed)*NewSpeed:FVector::ZeroVector;
            // Roll angular velocity matches turf forward speed.
            S.Spin=FVector(0,NewSpeed/Radius,0);
            Remaining-=MoveTime;
            if(NewSpeed<=0)break;
            continue;
        }
        const float H=Remaining;
        const FVector P0=S.Position,V0=S.Velocity;
        const float Height=FMath::Max(0.f,float(S.Position.Z)-Radius);
        const float ImpactTime=(S.Velocity.Z+FMath::Sqrt(S.Velocity.Z*S.Velocity.Z+2.f*G*Height))/G;
        const bool HitsGround=ImpactTime>=0&&ImpactTime<=Remaining;
        const float T=HitsGround?ImpactTime:H;
        // Lateral acceleration is split by phase of flight, which is what makes
        // the delivery variations genuinely different balls rather than one
        // effect with several names:
        //   Swing     - in the air, BEFORE the bounce. Conventional swing works
        //               from the moment of release; reverse swing only starts
        //               after SwingOnset, so it moves late and gives the batter
        //               far less time to adjust.
        //   Deviation - off the pitch, AFTER the bounce. This is a cutter or a
        //               spinner: nothing happens in the air, then the ball
        //               changes direction on landing and keeps deviating.
        float SwingAcc=0.f;
        if(!S.Bounced&&!S.Struck&&S.Age>=Plan.SwingOnset)
        {
            SwingAcc=Plan.Swing;
            // Reverse swing ramps in rather than switching on, so the arc stays
            // smooth while still being unmistakably late. The ramp length is a
            // shared constant: the launch trim in Release() integrates the same
            // curve, and the two must agree or the ball misses its target.
            if(Plan.SwingOnset>0.f)
                SwingAcc*=FMath::Clamp((S.Age-Plan.SwingOnset)/C26Delivery::ReverseRampSeconds,0.f,1.f);
        }
        else if(S.Bounced&&!S.Struck)
        {
            SwingAcc=Plan.Deviation*0.32f; // continued run-on after the seam grips
        }
        S.Position.X+=V0.X*T+.5f*SwingAcc*T*T;
        S.Position.Y+=V0.Y*T;
        S.Position.Z+=V0.Z*T-.5f*G*T*T;
        S.Velocity.X+=SwingAcc*T;
        S.Velocity.Z-=G*T;
        Remaining-=T;
        if(!HitsGround)break;
        S.Position.Z=Radius;
        if(!S.Struck&&!S.Bounced)
        {
            S.Velocity.Z=FMath::Abs(S.Velocity.Z)*Plan.Bounce;
            S.Velocity.Y*=.9f;
            // The seam bites here: a cutter or a spinner leaves the pitch on a
            // different line than it arrived on. Faster balls deviate less
            // because there is less time on the surface for the seam to work.
            const float Grip=FMath::Clamp(3600.f/FMath::Max(1200.f,Plan.Speed),0.55f,1.35f);
            S.Velocity.X=S.Velocity.X*.92f+Plan.Seam+Plan.Deviation*0.085f*Grip;
        }
        else
        {
            S.Velocity.Z=FMath::Abs(S.Velocity.Z)*.55f;
            S.Velocity.X*=.82f;S.Velocity.Y*=.82f;
            if(S.Struck)S.PostHitBounce=true;
        }
        S.Bounced=true;
    }
}
void FC26Simulation::Step(float Dt)
{
    if(!Ball.Active||Dt<=0)return;
    const auto Prev=Ball;
    Integrate(Ball,Dt);
    if(Prev.Position.Y<C26Field::ContactY&&Ball.Position.Y>=C26Field::ContactY)CrossedContact=true;
    if(!Prev.Bounced&&Ball.Bounced)BounceEvent=true;
    if(FMath::Abs(Ball.Position.Y-C26Field::WicketY)<18.f&&Ball.Position.Z<C26Field::StumpHeight&&FMath::Abs(Ball.Position.X)<C26Field::WicketWidth*.5f)
        StumpEvent=true;
    if(CrossesRope(Prev.Position,Ball.Position))BoundaryEvent=true;
}
FC26BallState FC26Simulation::Predict(float Seconds) const
{
    FC26BallState S=Ball;
    for(float T=0;T<Seconds;T+=1.f/240.f)Integrate(S,1.f/240.f);
    return S;
}
void FC26Simulation::Forecast(TArray<FC26BallState>& Out,float Horizon,float Interval) const
{
    Out.Reset(FMath::CeilToInt(Horizon/Interval)+1);
    FC26BallState S=Ball;Out.Add(S);
    float Time=0;
    while(Time<Horizon)
    {
        const float Duration=FMath::Min(Interval,Horizon-Time);
        const float StepSize=1.f/240.f;
        for(float Elapsed=0;Elapsed<Duration;Elapsed+=StepSize)
        {
            const float Slice=FMath::Min(StepSize,Duration-Elapsed);
            Integrate(S,Slice);
        }
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
bool FC26Simulation::GetBouncePrediction(FVector& OutPos, float& OutTime) const
{
    if(BounceTime > 0.f)
    {
        OutPos = BouncePosition;
        OutTime = BounceTime;
        return true;
    }
    return false;
}
FC26Contact FC26Simulation::Hit(const FC26ShotIntent& Intent,float Error,int Difficulty,FRandomStream& Random)
{
    (void)Random; // The contact result is entirely geometric; randomness belongs to AI intent.
    FC26Contact C;C.ContactPoint=Ball.Position;
    C.TimingDeltaMs=Error*1000.f;
    if(!Ball.Active||Ball.Struck||!FMath::IsFinite(Error))return C;
    const float DifficultyScale=C26Controls::TimingWindowScale(Difficulty);
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
    // Shot family comes from the SHARED classifier, so the stroke named here is
    // the same one the gesture preview showed the player while they were pulling.
    C.Shot=C26Controls::ShotFamily(ShotAngle,Plan.Length,Height,Stride,Offset,Intent.Loft,Intent.Defend);
    // Suitability and the working face angle still belong to the simulation.
    if(Intent.Defend)
    {
        ShotAngle=FMath::Clamp(ShotAngle,-45.f,45.f);
    }
    else if(Short)
    {
        if(FMath::Abs(ShotAngle)<38.f){C.Suitability-=.26f;ShotAngle=Offset>20.f?72.f:-68.f;}
    }
    else if(Yorker)
    {
        if(FMath::Abs(ShotAngle)>60.f)C.Suitability-=.32f;
        if(Intent.Loft)C.Suitability-=.25f;
        ShotAngle=FMath::Clamp(ShotAngle,-55.f,55.f);
    }
    else if(ShotAngle>78.f&&Height<45.f)C.Suitability-=.22f;
    // Risk/reward: a maximum-commitment swing has less control, so it mistimes
    // harder, edges more readily and goes up more often. Timing is untouched.
    if(!Intent.Defend)C.Suitability-=C26Controls::ControlPenaltyFromPower(Power);
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
    const float ElevationRad=FMath::DegreesToRadians(Elevation);
    const float AngleRad=FMath::DegreesToRadians(C.FaceAngle);
    // Exit direction: straight (0deg) travels back down the ground toward the
    // bowler (-Y); positive face angles open toward the off side (+X, cover).
    C.Velocity=FVector(FMath::Sin(AngleRad)*FMath::Cos(ElevationRad),
        -FMath::Cos(AngleRad)*FMath::Cos(ElevationRad),
        FMath::Sin(ElevationRad))*Speed;
    Ball.Struck=true;Ball.PostHitBounce=false;Ball.Velocity=C.Velocity;
    Ball.Spin=FVector(-C.Velocity.Y,C.Velocity.X,0).GetSafeNormal()*(Intent.Loft?-35.f:55.f)+FVector(0,0,Error*100.f);
    return C;
}
bool FC26Simulation::CrossesRope(const FVector& From,const FVector& To) const
{
    const bool InsideFrom=C26Field::Inside(From),InsideTo=C26Field::Inside(To);
    return InsideFrom!=InsideTo;
}
FC26DeliveryPlan FC26AI::Bowl(const C26::Match& Rules,int Difficulty)
{
    FC26DeliveryPlan P;
    const int B=Rules.Now().LegalBalls;const float R=Random.FRand();
    if(B>=4&&R<.52f)P.Type=EC26Delivery::Yorker;
    else if(History.Boundaries>0&&R<.45f)P.Type=EC26Delivery::Slower;
    else if(B>0&&R>.8f)P.Type=EC26Delivery::Bouncer;
    else P.Type=R<.3f?EC26Delivery::Inswing:R<.6f?EC26Delivery::Outswing:EC26Delivery::Pace;
    if(P.Type==History.LastDelivery&&++History.RepeatedDelivery>1)
        P.Type=P.Type==EC26Delivery::Outswing?EC26Delivery::Slower:EC26Delivery::Outswing;
    History.LastDelivery=P.Type;
    P.Speed=P.Type==EC26Delivery::Slower?2420:3480;
    P.Length=P.Type==EC26Delivery::Yorker?810:P.Type==EC26Delivery::Bouncer?40:430;
    if(P.Type==EC26Delivery::Slower){P.Speed=2450;P.Length=570;}
    P.Bounce=P.Type==EC26Delivery::Bouncer?.72f:.55f;
    P.Line=Difficulty==0?Random.FRandRange(-20,20):Random.FRandRange(-55,55);
    if(History.OffsideBias>1.2f)P.Line=FMath::Clamp(P.Line+35.f,-70.f,70.f);
    P.Swing=P.Type==EC26Delivery::Outswing?180:P.Type==EC26Delivery::Inswing?-180:0;
    P.Seam=Random.FRandRange(-12,12);
    return P;
}
FC26ShotIntent FC26AI::Bat(const FC26DeliveryPlan& VisibleDelivery,const C26::Match& Rules,int Difficulty)
{
    FC26ShotIntent I;
    const bool Short=VisibleDelivery.Length<180;
    const bool Yorker=VisibleDelivery.Length>720;
    I.Stride=Short?-.85f:Yorker?.85f:.2f;
    I.Footwork=FMath::Clamp(VisibleDelivery.Line/45.f,-1.f,1.f);
    I.Power=Difficulty==0?.65f:Difficulty==2?.95f:.82f;
    I.Loft=Rules.Target()>0&&(Rules.Target()-Rules.Now().Runs)>12;
    if(Short)I.Angle=Random.FRand()>.5f?65.f:-65.f;
    else if(Yorker){I.Angle=Random.FRandRange(-20,20);I.Loft=false;}
    else I.Angle=Random.FRandRange(-45,45);
    History.OffsideBias=I.Angle>0?History.OffsideBias+.3f:History.OffsideBias-.2f;
    History.LastPower=I.Power;
    return I;
}
float FC26AI::TimingError(int Difficulty)
{
    const float Spread=Difficulty==0?.085f:Difficulty==2?.022f:.045f;
    return Random.FRandRange(-Spread,Spread);
}
TArray<FVector> FC26AI::Field(bool ProtectOffside)
{
    TArray<FVector> Out;
    // Indices 0/1 are the bowler's mark and the keeper's stance: the match
    // mode addresses all eleven athletes positionally, and the keeper-take
    // prediction reads FieldPositions[1].
    Out.Add(FVector(-20,-2700,5));
    Out.Add(FVector(0,2450,5));
    const TCHAR* Standard[]={
        TEXT("MidOff"),TEXT("MidOn"),TEXT("Cover"),TEXT("Point"),TEXT("FineLeg"),TEXT("SquareLeg"),TEXT("MidWicket"),TEXT("ThirdMan"),TEXT("LongOff")
    };
    const TCHAR* Offside[]={
        TEXT("DeepPoint"),TEXT("Cover"),TEXT("ExtraCover"),TEXT("MidOff"),TEXT("FineLeg"),TEXT("SquareLeg"),TEXT("MidWicket"),TEXT("ThirdMan"),TEXT("DeepCover")
    };
    for(int I=0;I<9;++I) Out.Add(FieldPosition(ProtectOffside?Offside[I]:Standard[I]));
    return Out;
}
FVector FC26AI::FieldPosition(FName Name)
{
    static const TMap<FName,FVector> Map={
        {TEXT("MidOff"),FVector(680,-1200,5)},
        {TEXT("MidOn"),FVector(-680,-1200,5)},
        {TEXT("Cover"),FVector(1680,320,5)},
        {TEXT("Point"),FVector(2350,860,5)},
        {TEXT("FineLeg"),FVector(-2050,1580,5)},
        {TEXT("SquareLeg"),FVector(-2350,860,5)},
        {TEXT("MidWicket"),FVector(-1750,-240,5)},
        {TEXT("ThirdMan"),FVector(2050,1580,5)},
        {TEXT("LongOff"),FVector(1250,-3450,5)},
        {TEXT("LongOn"),FVector(-1250,-3450,5)},
        {TEXT("DeepCover"),FVector(3600,450,5)},
        {TEXT("DeepPoint"),FVector(3800,1050,5)},
        {TEXT("ExtraCover"),FVector(2250,-420,5)}
    };
    const auto* Found=Map.Find(Name);
    return Found?*Found:FVector(1400,0,5);
}
