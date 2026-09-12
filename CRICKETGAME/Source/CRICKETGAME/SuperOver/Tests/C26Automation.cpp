#include "Misc/AutomationTest.h"
#include "../Core/C26Rules.h"
#include "../C26Simulation.h"
#include "../C26Controls.h"
#include "../C26Delivery.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26RulesTest,"Cricket26.Rules.SuperOver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26RulesTest::RunTest(const FString& Parameters)
{
    using namespace C26;uint32 Id=0;
    auto Ball=[&](Match& M,int Runs=0){DeliveryOutcome O;O.Epoch=M.Epoch;O.Id=++Id;O.BatRuns=Runs;O.CompletedRuns=Runs<4?Runs:0;return O;};
    auto First=[&](Match& M,int Runs=0){for(int I=0;I<6;++I)M.Apply(Ball(M,I==0?Runs:0));};
    Match M;M.Reset();First(M);TestTrue(TEXT("1 Six legal dots complete innings"),M.Now().Closed&&M.Now().LegalBalls==6);
    M.Reset();auto O=Ball(M);O.WideRuns=1;M.Apply(O);TestTrue(TEXT("2 Wide preserves legal ball"),M.Now().Runs==1&&M.Now().LegalBalls==0);
    M.Reset();O=Ball(M,2);O.NoBall=true;M.Apply(O);TestTrue(TEXT("3 No-ball, runs and free hit"),M.Now().Runs==3&&M.Now().LegalBalls==0&&M.Now().FreeHit);
    M.Reset();for(int I=0;I<2;++I){O=Ball(M);O.Wicket=Dismissal::Bowled;M.Apply(O);}TestTrue(TEXT("4 Two wickets complete innings"),M.Now().Closed&&M.Now().Wickets==2);
    M.Reset();First(M,4);M.StartChase();M.Apply(Ball(M,6));TestTrue(TEXT("5 Chase ends immediately"),M.Winner==Result::SecondTeam&&M.Now().LegalBalls==1);
    M.Reset();First(M,6);M.StartChase();First(M,4);TestTrue(TEXT("6 Failed chase"),M.Winner==Result::FirstTeam);
    M.Reset();First(M,4);M.StartChase();First(M,4);TestTrue(TEXT("7 Tie"),M.Winner==Result::Tie);
    M.Reset();O=Ball(M,4);M.Apply(O);TestTrue(TEXT("8 Duplicate delivery rejected"),M.Apply(O)==Commit::Duplicate&&M.Now().Runs==4);
    for(int I=0;I<10;++I){M.Reset();First(M);M.StartChase();M.Apply(Ball(M,1));TestTrue(TEXT("9 Repeated complete rematch"),M.Winner==Result::SecondTeam);}
    O=Ball(M);M.Reset();TestTrue(TEXT("9 Stale outcome cannot mutate new match"),M.Apply(O)==Commit::WrongEpoch&&M.Now().Runs==0);
    O=Ball(M,1);O.NoBall=true;O.Wicket=Dismissal::RunOut;M.Apply(O);TestTrue(TEXT("10 Run out plus no-ball plus completed run"),M.Now().Runs==2&&M.Now().Wickets==1&&M.Now().LegalBalls==0);
    M.Reset();O=Ball(M);O.Rope=Boundary::Four;M.Apply(O);O=Ball(M);O.Rope=Boundary::Six;M.Apply(O);TestEqual(TEXT("11 Boundaries"),M.Now().Runs,10);
    M.Reset();M.Apply(Ball(M,1));M.Apply(Ball(M,2));TestEqual(TEXT("12 Strike rotation"),M.Now().Striker,1);
    M.Reset();TestFalse(TEXT("13 Premature innings switch rejected"),M.StartChase());First(M,6);TestTrue(TEXT("13 Innings switch"),M.StartChase()&&M.Current==1&&M.Target()==7);
    M.Apply(Ball(M,2));TestEqual(TEXT("14 Runs required"),M.RunsRequired(),5);TestEqual(TEXT("15 Balls remaining"),M.BallsRemaining(),5);
    M.Reset();O=Ball(M);O.NoBall=true;M.Apply(O);O=Ball(M);O.WideRuns=1;M.Apply(O);O=Ball(M);O.Wicket=Dismissal::Caught;M.Apply(O);TestTrue(TEXT("Free hit survives wide, suppresses caught, then clears"),M.Now().Wickets==0&&!M.Now().FreeHit);
    M.Reset();O=Ball(M);O.Byes=1;O.CompletedRuns=1;M.Apply(O);O=Ball(M);O.LegByes=2;O.CompletedRuns=2;M.Apply(O);TestTrue(TEXT("Byes and leg byes rotate strike but not batter score"),M.Now().Runs==3&&M.Now().Extras==3&&M.Now().Striker==1&&M.Now().BatterRuns[0]==0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26TrajectoryTest,"Cricket26.Simulation.Trajectories",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26TrajectoryTest::RunTest(const FString& Parameters)
{
    FC26Simulation Sim;FC26AI AI;AI.Reset(26);C26::Match M;M.Reset();
    TestEqual(TEXT("Eleven fielding players including bowler and keeper"),FC26AI::Field().Num(),11);
    TestTrue(TEXT("Rope segment detects fast crossing"),Sim.CrossesRope(FVector(6400,0,2),FVector(6800,0,2)));
    TestFalse(TEXT("Inside field remains in play"),Sim.CrossesRope(FVector(300,0,2),FVector(800,0,2)));
    for(int I=0;I<24;++I)
    {
        auto Plan=AI.Bowl(M,1);Sim.Release(Plan,FVector(-20,-910,213));
        TestTrue(TEXT("Contact time reachable"),Sim.ContactTime>.3f&&Sim.ContactTime<1.3f);
        TestTrue(TEXT("Contact height is physical"),Sim.ContactPosition.Z>=3.5f&&Sim.ContactPosition.Z<220);
        for(int Step=0;Step<240&&Sim.Ball.Active;++Step)Sim.Step(1.f/240.f);
        TestFalse(TEXT("Finite trajectory"),Sim.Ball.Position.ContainsNaN());
    }
    FC26DeliveryPlan P;Sim.Release(P,FVector(0,-910,213));Sim.Step(Sim.ContactTime);FRandomStream R(26);FC26ShotIntent Shot;Shot.Loft=true;
    auto Contact=Sim.Hit(Shot,0,1,R);TestTrue(TEXT("A centred timed shot produces a rebound"),Contact.Timing!=EC26Timing::Miss&&Sim.Ball.Struck&&Sim.Ball.Velocity.Y<0);
    TestFalse(TEXT("Rope six has no ground contact initially"),Sim.Ball.PostHitBounce);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26GoldenDeliveryTest,"Cricket26.Simulation.GoldenDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26GoldenDeliveryTest::RunTest(const FString& Parameters)
{
    // ONE golden delivery: genuine pace from the hand, one pitching bounce, one front-foot
    // straight drive timed dead centre. Locks the hand -> bounce -> contact -> redirect chain.
    FC26Simulation Sim;Sim.Tuning=FC26Tuning();
    FC26DeliveryPlan Pace; // Defaults: Pace, 460 length, 3250 speed.
    const FVector Hand(-20.f,-940.f,210.f);
    Sim.Release(Pace,Hand);
    TestTrue(TEXT("Golden: ball leaves the bowling hand"),Sim.Ball.Active&&Sim.Ball.Position.Equals(Hand,1.f));
    TestTrue(TEXT("Golden: delivery arrives on a readable timescale"),Sim.ContactTime>.3f&&Sim.ContactTime<1.3f);
    const float IncomingY=Sim.Ball.Velocity.Y;
    TestTrue(TEXT("Golden: delivery travels toward the striker"),IncomingY>0);
    Sim.Step(Sim.ContactTime);
    TestTrue(TEXT("Golden: ball pitched before reaching the bat"),Sim.Ball.Bounced);
    TestTrue(TEXT("Golden: contact happens at the striker's end"),FMath::Abs(Sim.Ball.Position.Y-C26Field::ContactY)<5.f);
    FC26ShotIntent Drive;Drive.Angle=0;Drive.Power=.8f;Drive.Stride=.75f;Drive.Footwork=0;Drive.Loft=false;Drive.Defend=false;
    FRandomStream R(260026);
    auto Contact=Sim.Hit(Drive,0,1,R);
    TestTrue(TEXT("Golden: centred straight drive is clean contact"),
        Contact.Timing==EC26Timing::Perfect||Contact.Timing==EC26Timing::Good);
    TestEqual(TEXT("Golden: the call is a straight drive"),Contact.Shot,TEXT("STRAIGHT DRIVE"));
    TestTrue(TEXT("Golden: contact quality is high"),Contact.Quality>.5f);
    TestTrue(TEXT("Golden: contact point is the ball, not a teleport"),
        Contact.ContactPoint.Equals(Sim.Ball.Position,1.f));
    TestTrue(TEXT("Golden: trajectory redirects back down the ground"),Sim.Ball.Velocity.Y<0&&FMath::Abs(Sim.Ball.Velocity.X)<FMath::Abs(Sim.Ball.Velocity.Y));
    TestTrue(TEXT("Golden: drive leaves at a grounded height"),Sim.Ball.Position.Z<200.f&&Sim.Ball.Velocity.Z<900.f);
    for(int Step=0;Step<480&&Sim.Ball.Active;++Step)Sim.Step(1.f/240.f);
    TestFalse(TEXT("Golden: post-contact trajectory stays finite"),Sim.Ball.Position.ContainsNaN());
    return true;
}

#include "../C26CommentaryTypes.h"
#include "../C26CommentaryLibrary.h"
#include "../C26CommentaryDirector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CommentaryDirectorTest, "Cricket26.Commentary.Director", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26CommentaryDirectorTest::RunTest(const FString& Parameters)
{
    UC26CommentaryDirector* Director = NewObject<UC26CommentaryDirector>();
    TestNotNull(TEXT("Director instantiated"), Director);

    // 1. Pressure Model Testing
    FC26CommentaryEvent E1;
    E1.InningsNumber = 0;
    E1.BallNumber = 1;
    const float LowPressure = Director->CalculatePressure(E1);
    TestTrue(TEXT("First innings early pressure is low"), LowPressure <= 0.35f);

    FC26CommentaryEvent E2;
    E2.InningsNumber = 1;
    E2.Target = 18;
    E2.RunsRequired = 6;
    E2.BallsRemaining = 1;
    E2.Wickets = 1;
    const float ExtremePressure = Director->CalculatePressure(E2);
    TestTrue(TEXT("Chase final ball pressure is extreme"), ExtremePressure >= 0.90f);

    // 2. Emotion Selection Testing
    float Intensity = 0.f;

    // Normal four
    FC26CommentaryEvent EFour;
    EFour.EventType = ECommentaryEventType::Four;
    EFour.bIsBoundary = true;
    ECommentaryEmotion EmFour = Director->SelectEmotion(EFour, 0.30f, Intensity);
    TestTrue(TEXT("Normal four is Appreciative or Excited"), EmFour == ECommentaryEmotion::Appreciative || EmFour == ECommentaryEmotion::Excited);

    // High pressure six
    FC26CommentaryEvent EHighSix;
    EHighSix.EventType = ECommentaryEventType::Six;
    EHighSix.bIsSix = true;
    EHighSix.bIsBoundary = true;
    ECommentaryEmotion EmHighSix = Director->SelectEmotion(EHighSix, 0.85f, Intensity);
    TestTrue(TEXT("High pressure six is VeryExcited"), EmHighSix == ECommentaryEmotion::VeryExcited && Intensity >= 0.90f);

    // Match winning six
    FC26CommentaryEvent EWinSix;
    EWinSix.EventType = ECommentaryEventType::Six;
    EWinSix.bIsSix = true;
    EWinSix.bIsMatchWinningEvent = true;
    ECommentaryEmotion EmWinSix = Director->SelectEmotion(EWinSix, 0.99f, Intensity);
    TestTrue(TEXT("Match winning six is Celebratory at max intensity"), EmWinSix == ECommentaryEmotion::Celebratory && Intensity >= 0.99f);

    // Wicket bowled
    FC26CommentaryEvent EBowled;
    EBowled.EventType = ECommentaryEventType::Bowled;
    EBowled.bIsWicket = true;
    EBowled.DismissalType = 1;
    ECommentaryEmotion EmBowled = Director->SelectEmotion(EBowled, 0.75f, Intensity);
    TestTrue(TEXT("Bowled wicket is Shocked or Dramatic"), EmBowled == ECommentaryEmotion::Shocked || EmBowled == ECommentaryEmotion::Dramatic);

    // 3. Commentary Library Query Verification
    TArray<const FC26CommentaryLineDef*> FourLines = FC26CommentaryLibrary::FindMatchingLines(
        FName(TEXT("FOUR")), ECommentatorRole::Lead, ECommentaryEmotion::Appreciative, 0.3f, 0.5f, false, false, false
    );
    TestTrue(TEXT("Library has lead four lines"), FourLines.Num() > 0);

    TArray<const FC26CommentaryLineDef*> SixLeadLines = FC26CommentaryLibrary::FindMatchingLines(
        FName(TEXT("SIX")), ECommentatorRole::Lead, ECommentaryEmotion::VeryExcited, 0.85f, 0.9f, false, false, false
    );
    TestTrue(TEXT("Library has high-pressure six lines"), SixLeadLines.Num() > 0);

    TArray<const FC26CommentaryLineDef*> SixAnalystLines = FC26CommentaryLibrary::FindMatchingLines(
        FName(TEXT("SIX")), ECommentatorRole::Analyst, ECommentaryEmotion::Analytical, 0.5f, 0.5f, false, false, true
    );
    TestTrue(TEXT("Library has analyst follow-up lines for sixes"), SixAnalystLines.Num() > 0);

    // 4. Test Scenario Method
    Director->TestScenario(ECommentaryScenario::RoutineDot);
    Director->TestScenario(ECommentaryScenario::NormalFour);
    Director->TestScenario(ECommentaryScenario::NormalSix);
    Director->TestScenario(ECommentaryScenario::HighPressureSix);
    Director->TestScenario(ECommentaryScenario::WicketBowled);
    Director->TestScenario(ECommentaryScenario::WicketCaught);
    Director->TestScenario(ECommentaryScenario::FinalBallTense);
    Director->TestScenario(ECommentaryScenario::MatchWinningSix);

    TestTrue(TEXT("Last played line recorded"), !Director->GetLastPlayedLineId().IsEmpty());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26GestureControlsTest, "Cricket26.Controls.GesturePro", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26GestureControlsTest::RunTest(const FString& Parameters)
{
    using namespace C26Controls;
    // Pull vector is resolution-independent design space.
    TestTrue(TEXT("Pull vector"), PullVector(FVector2D(100, 100), FVector2D(160, 40)) == FVector2D(60, -60));
    TestEqual(TEXT("Dead zone swallows micro pulls"), PullMagnitude01(FVector2D(5, 5), 12.f, 220.f), 0.f);
    TestTrue(TEXT("Full pull saturates"), PullMagnitude01(FVector2D(400, 0), 12.f, 220.f) >= 0.999f);
    // Aim follows the SCREEN: the batting camera is behind the bowler, so a
    // right-hander's off side is on the left of the screen. Drag right = leg
    // side (-), drag left = off side (+), up straightens.
    TestTrue(TEXT("Leg-side pull aims leg"), AimAngleFromPull(FVector2D(120, 0), 12.f, 1.f, false) < -40.f);
    TestTrue(TEXT("Off-side pull aims off"), AimAngleFromPull(FVector2D(-120, 0), 12.f, 1.f, false) > 40.f);
    TestTrue(TEXT("Straight pull stays central"), FMath::Abs(AimAngleFromPull(FVector2D(0, -150), 12.f, 1.f, false)) < 8.f);
    TestTrue(TEXT("Left-handed batter mirrors"), AimAngleFromPull(FVector2D(120, 0), 12.f, 1.f, true) > 40.f);
    // Power curve is bounded and monotonic.
    const float P0 = PowerFromPull(0.f, 0.35f, 1.f), P1 = PowerFromPull(1.f, 0.35f, 1.f);
    TestTrue(TEXT("Power bounded"), P0 >= 0.349f && P1 <= 1.001f && P1 > PowerFromPull(0.5f, 0.35f, 1.f));
    // Timing classification from real millisecond deltas.
    float Q = 0.f;
    TestTrue(TEXT("Perfect on time"), TimingLabelFromDelta(8.f, 25.f, 83.f, 190.f, Q) == EC26Timing::Perfect && Q > 0.9f);
    TestTrue(TEXT("Good slightly off"), TimingLabelFromDelta(60.f, 25.f, 83.f, 190.f, Q) == EC26Timing::Good);
    TestTrue(TEXT("Early well off"), TimingLabelFromDelta(-120.f, 25.f, 83.f, 190.f, Q) == EC26Timing::Early);
    TestTrue(TEXT("Late well off"), TimingLabelFromDelta(150.f, 25.f, 83.f, 190.f, Q) == EC26Timing::Late);
    TestTrue(TEXT("Way off is a miss"), TimingLabelFromDelta(400.f, 25.f, 83.f, 190.f, Q) == EC26Timing::Miss && Q <= 0.001f);
    // Line/length recognition across the full matrix.
    TestTrue(TEXT("Yorker"), ClassifyLength(800.f) == EC26DeliveryLength::Yorker);
    TestTrue(TEXT("Full"), ClassifyLength(600.f) == EC26DeliveryLength::Full);
    TestTrue(TEXT("Good"), ClassifyLength(400.f) == EC26DeliveryLength::GoodLength);
    TestTrue(TEXT("Short"), ClassifyLength(120.f) == EC26DeliveryLength::Short);
    TestTrue(TEXT("Bouncer"), ClassifyLength(20.f) == EC26DeliveryLength::Bouncer);
    TestTrue(TEXT("Wide"), ClassifyLine(70.f) == EC26DeliveryLine::WideOff);
    TestTrue(TEXT("Middle"), ClassifyLine(0.f) == EC26DeliveryLine::MiddleStump);
    TestTrue(TEXT("Down leg"), ClassifyLine(-40.f) == EC26DeliveryLine::DownLeg);
    // Cricket knowledge: wrong shots punished, right shots rewarded.
    TestTrue(TEXT("Cover drive to half-volley ideal"),
        ShotSuitability(30.f, 600.f, 15.f, false, false) > 0.85f);
    TestTrue(TEXT("Straight drive to short ball punished"),
        ShotSuitability(0.f, 120.f, 0.f, false, false) < 0.75f);
    TestTrue(TEXT("Pull to yorker punished"),
        ShotSuitability(-70.f, 800.f, 0.f, false, false) < 0.7f);
    TestTrue(TEXT("Pull to short ball rewarded"),
        ShotSuitability(-65.f, 120.f, 5.f, false, false) > 0.85f);
    TestTrue(TEXT("Lofted yorker punished"),
        ShotSuitability(0.f, 800.f, 0.f, true, false) < 0.75f);
    // Footwork follows length.
    TestTrue(TEXT("Front foot to full"), IdealStrideForLength(700.f) > 0.5f);
    TestTrue(TEXT("Back foot to short"), IdealStrideForLength(100.f) < -0.5f);
    // Effort model: pace up with pull, penalty only at max exertion.
    TestTrue(TEXT("Pace scales with effort"), PaceMultiplier(1.f) > PaceMultiplier(0.f));
    TestTrue(TEXT("No penalty at control pace"), EffortErrorPenalty(0.5f) <= 0.001f);
    TestTrue(TEXT("Penalty at max effort"), EffortErrorPenalty(1.f) > 0.05f);

    // ---- rebuilt pull-and-release loop ------------------------------------
    // Sensitivity scales finger travel, not the clamp: magnitude stays <= 1.
    TestTrue(TEXT("Sensitivity shortens the required drag"),
        PullMagnitude01(FVector2D(0, -90), 16.f, 210.f, 2.f) > PullMagnitude01(FVector2D(0, -90), 16.f, 210.f, 1.f));
    TestTrue(TEXT("Magnitude never exceeds one"),
        PullMagnitude01(FVector2D(0, -4000), 16.f, 210.f, 3.f) <= 1.0001f);
    // Aggression curve: monotonic, bounded, and deliberately non-linear so the
    // first quarter of the pull cannot produce a heave.
    TestTrue(TEXT("Curve starts controlled"), DefaultMagnitudeCurve(0.20f) < 0.22f);
    TestTrue(TEXT("Curve tops out at one"), FMath::IsNearlyEqual(DefaultMagnitudeCurve(1.f), 1.f, 0.001f));
    TestTrue(TEXT("Curve is monotonic"),
        DefaultMagnitudeCurve(0.3f) > DefaultMagnitudeCurve(0.1f) && DefaultMagnitudeCurve(0.9f) > DefaultMagnitudeCurve(0.7f));
    TestTrue(TEXT("Curve is not linear"), FMath::Abs(DefaultMagnitudeCurve(0.5f) - 0.5f) > 0.05f);
    TestTrue(TEXT("Band names track aggression"),
        FString(AggressionBandName(0.10f)) == TEXT("CONTROLLED") && FString(AggressionBandName(0.95f)) == TEXT("MAXIMUM"));
    // Risk rides with aggression: a maximum pull costs control.
    TestTrue(TEXT("No control cost below attack"), ControlPenaltyFromAggression(0.4f) <= 0.001f);
    TestTrue(TEXT("Maximum pull costs control"), ControlPenaltyFromAggression(1.f) > 0.2f);
    // Six-band release timing from one millisecond delta.
    TestTrue(TEXT("Release perfect"), ReleaseTimingFromDelta(10.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::Perfect);
    TestTrue(TEXT("Release good"), ReleaseTimingFromDelta(-60.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::Good);
    TestTrue(TEXT("Release early"), ReleaseTimingFromDelta(-130.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::Early);
    TestTrue(TEXT("Release late"), ReleaseTimingFromDelta(130.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::Late);
    TestTrue(TEXT("Release very early"), ReleaseTimingFromDelta(-230.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::VeryEarly);
    TestTrue(TEXT("Release very late"), ReleaseTimingFromDelta(230.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::VeryLate);
    TestTrue(TEXT("Release out of reach"), ReleaseTimingFromDelta(900.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::NoShot);
    TestTrue(TEXT("A run-up release reads very early"),
        ReleaseTimingFromDelta(-3200.f, 25.f, 83.f, 190.f, 250.f) == EC26ReleaseTiming::NoShot);
    // Direction naming is batter-relative and mirrors cleanly. A right-hander
    // drags LEFT to reach cover; a left-hander reaches the same zone by dragging
    // RIGHT, because their off side is the other way round on screen.
    TestTrue(TEXT("Cover named"), FString(DirectionZoneName(AimAngleFromPull(FVector2D(-90, -90), 16.f, 1.f, false))) == TEXT("COVER"));
    TestTrue(TEXT("Midwicket named"), FString(DirectionZoneName(AimAngleFromPull(FVector2D(90, -90), 16.f, 1.f, false))) == TEXT("MIDWICKET"));
    TestTrue(TEXT("Left-hander sees the same zone for the same intent"),
        FString(DirectionZoneName(AimAngleFromPull(FVector2D(90, -90), 16.f, 1.f, true)))
            == FString(DirectionZoneName(AimAngleFromPull(FVector2D(-90, -90), 16.f, 1.f, false))));
    // Shot family: the SAME drag direction produces different strokes by length.
    TestTrue(TEXT("Off-side drag to a full ball drives"),
        FString(ShotFamily(35.f, 620.f, 42.f, .75f, -20.f, false, false)) == TEXT("COVER DRIVE"));
    TestTrue(TEXT("Off-side drag to a short ball cuts"),
        FString(ShotFamily(35.f, 120.f, 130.f, -.8f, 25.f, false, false)) == TEXT("SQUARE CUT"));
    TestTrue(TEXT("Leg-side drag to a short ball pulls"),
        FString(ShotFamily(-60.f, 120.f, 120.f, -.8f, 0.f, false, false)) == TEXT("PULL"));
    TestTrue(TEXT("Leg-side drag to a bouncer hooks"),
        FString(ShotFamily(-60.f, 40.f, 160.f, -.8f, 0.f, false, false)) == TEXT("HOOK"));
    TestTrue(TEXT("Straight drag to a full ball drives straight"),
        FString(ShotFamily(0.f, 620.f, 40.f, .75f, 0.f, false, false)) == TEXT("STRAIGHT DRIVE"));
    TestTrue(TEXT("Leg-side drag to a full ball flicks"),
        FString(ShotFamily(-45.f, 620.f, 40.f, .75f, 10.f, false, false)) == TEXT("FLICK"));
    TestTrue(TEXT("A tiny pull defends"),
        FString(ShotFamily(0.f, 430.f, 70.f, .25f, 0.f, false, true)) == TEXT("DEFENSIVE PUSH"));
    TestTrue(TEXT("Straight drag to a short ball still becomes a cross-bat shot"),
        FString(ShotFamily(0.f, 120.f, 130.f, -.8f, 30.f, false, false)) == TEXT("SQUARE CUT"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26BouncePredictionTest, "Cricket26.Controls.BouncePrediction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26BouncePredictionTest::RunTest(const FString& Parameters)
{
    // The marker must correspond to REAL trajectory data: predicted bounce vs
    // the bounce the integrator actually produces, across the test matrix.
    FC26Simulation Sim; Sim.Tuning = FC26Tuning();
    const FVector Hand(-20.f, -940.f, 210.f);
    const float Lengths[5] = { 810.f, 600.f, 420.f, 120.f, 40.f };
    const float Lines[3] = { -45.f, 0.f, 30.f };
    for (float L : Lengths)
    {
        for (float X : Lines)
        {
            FC26DeliveryPlan P; P.Length = L; P.Line = X; P.Speed = 3480.f; P.Swing = 0.f; P.Seam = 5.f; P.Bounce = 0.55f;
            Sim.Release(P, Hand);
            FVector Pred; float PredT = -1.f;
            const bool bHas = Sim.GetBouncePrediction(Pred, PredT);
            // Integrate to the real bounce.
            FVector Actual = FVector::ZeroVector; bool bFound = false;
            FC26BallState S = Sim.Ball;
            for (int I = 0; I < 1440; ++I)
            {
                const bool Was = S.Bounced;
                // Step manually via public API mirror: use a scratch sim.
                Sim.Ball = S; Sim.Step(1.f / 240.f); S = Sim.Ball;
                if (!Was && S.Bounced) { Actual = S.Position; bFound = true; break; }
                if (S.Position.Y > C26Field::ContactY + 50.f) break;
            }
            Sim.Release(P, Hand); // restore canonical state
            if (bHas)
            {
                TestTrue(TEXT("Bounce predicted"), bFound);
                if (bFound)
                    TestTrue(TEXT("Prediction matches physics (5cm)"),
                        FVector::Dist2D(Pred, Actual) < 5.f);
            }
        }
    }
    // Perfect release is (near-)identity: intended == actual pitch point.
    FC26DeliveryPlan Locked; Locked.Line = -12.f; Locked.Length = 430.f;
    FC26DeliveryPlan Actual = C26Delivery::Execute(Locked, 0.f);
    TestTrue(TEXT("Perfect release holds line/length"),
        FMath::Abs(Actual.Line - Locked.Line) < 1.f && FMath::Abs(Actual.Length - Locked.Length) < 1.f);
    FC26DeliveryPlan Poor = C26Delivery::Execute(Locked, 1.4f);
    TestTrue(TEXT("Poor release deviates continuously"),
        FMath::Abs(Poor.Line - Locked.Line) > 10.f && Poor.NoBall);
    return true;
}
#endif
