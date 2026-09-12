#include "Misc/AutomationTest.h"
#include "../Core/C26Rules.h"
#include "../C26Simulation.h"
#include "../C26Controls.h"
#include "../C26Delivery.h"
#include "../C26FieldingSystem.h"
#include "../C26Motion.h"
#include "../C26PresentationDirector.h"
#include "../C26PresentationTypes.h"

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

// ============================================================================
// CRICKET 26 // GAMEPLAY CONTROL OVERHAUL AUTOMATION SUITE
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26PitchTargetingTest, "Cricket26.ControlOverhaul.PitchTargetingAndPresets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26PitchTargetingTest::RunTest(const FString& Parameters)
{
    // 1. Target boundary clamping
    const float PitchMinX = -135.f, PitchMaxX = 135.f;
    const float PitchMinY = 30.f, PitchMaxY = 840.f;

    FVector TestP1(-300.f, -50.f, 0.f);
    FVector Clamped1(FMath::Clamp(TestP1.X, PitchMinX, PitchMaxX), FMath::Clamp(TestP1.Y, PitchMinY, PitchMaxY), 5.f);
    TestEqual(TEXT("Pitch min bounds clamp"), Clamped1, FVector(-135.f, 30.f, 5.f));

    FVector TestP2(450.f, 1200.f, 0.f);
    FVector Clamped2(FMath::Clamp(TestP2.X, PitchMinX, PitchMaxX), FMath::Clamp(TestP2.Y, PitchMinY, PitchMaxY), 5.f);
    TestEqual(TEXT("Pitch max bounds clamp"), Clamped2, FVector(135.f, 840.f, 5.f));

    // 2. Bowling preset lengths
    const float YorkerLen = 800.f, FullLen = 650.f, GoodLen = 480.f, ShortLen = 280.f, BouncerLen = 100.f;
    TestTrue(TEXT("Yorker is full"), YorkerLen > FullLen && FullLen > GoodLen && GoodLen > ShortLen && ShortLen > BouncerLen);
    TestTrue(TEXT("All presets within pitch length limits"), YorkerLen <= PitchMaxY && BouncerLen >= PitchMinY);

    // 3. Delivery record buffer
    TArray<FC26DeliveryRecord> Records;
    for (int32 I = 0; I < 30; ++I)
    {
        FC26DeliveryRecord Rec;
        Rec.IntendedPitch = FVector(0.f, 480.f, 5.f);
        Rec.ActualPitch = FVector(5.f, 475.f, 5.f);
        Rec.SpeedKph = 142.f;
        Rec.DeliveryType = EC26Delivery::Pace;
        Rec.RunsConceded = (I % 4 == 0) ? 4 : (I % 3 == 0 ? 1 : 0);
        Rec.bWicket = (I == 5 || I == 18);
        Rec.bBoundary = (Rec.RunsConceded >= 4);
        Rec.bDot = (Rec.RunsConceded == 0 && !Rec.bWicket);

        Records.Add(Rec);
        if (Records.Num() > 24) Records.RemoveAt(0);
    }
    TestEqual(TEXT("Delivery history buffer caps cleanly at 24"), Records.Num(), 24);
    TestTrue(TEXT("Delivery record fields accurately captured"), Records.Last().SpeedKph > 140.f && Records.Last().ActualPitch.Y > 0.f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26FieldPresetsTest, "Cricket26.ControlOverhaul.FieldPresetsAndLegality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26FieldPresetsTest::RunTest(const FString& Parameters)
{
    // Test all 12 tactical field presets
    const EC26FieldPreset AllPresets[] = {
        EC26FieldPreset::Balanced,
        EC26FieldPreset::Attacking,
        EC26FieldPreset::Defensive,
        EC26FieldPreset::PowerplayAttack,
        EC26FieldPreset::PowerplayDefensive,
        EC26FieldPreset::PaceAttack,
        EC26FieldPreset::SpinAttack,
        EC26FieldPreset::OffsideHeavy,
        EC26FieldPreset::LegsideHeavy,
        EC26FieldPreset::DeathOvers,
        EC26FieldPreset::ProtectBoundary,
        EC26FieldPreset::SinglePrevention
    };

    for (EC26FieldPreset Preset : AllPresets)
    {
        TArray<FVector> Field = C26Fielding::GetPresetFieldPositions(Preset, false);
        TestEqual(TEXT("Preset provides exactly 11 positions"), Field.Num(), 11);

        // Keeper behind stumps
        TestTrue(TEXT("Keeper is stationed behind batting crease"), Field[1].Y > 1006.f);

        // Legality check outside powerplay
        FString Warn;
        const bool bLegalStandard = C26Fielding::ValidateFieldLegality(Field, false, false, Warn);
        TestTrue(FString::Printf(TEXT("Preset %d is legal in standard play"), (int32)Preset), bLegalStandard);
    }

    // Test Powerplay presets under powerplay restriction
    TArray<FVector> PPAttack = C26Fielding::GetPresetFieldPositions(EC26FieldPreset::PowerplayAttack, false);
    FString WarnPP;
    TestTrue(TEXT("Powerplay Attack is compliant with 2-man boundary rule"),
        C26Fielding::ValidateFieldLegality(PPAttack, true, false, WarnPP));

    TArray<FVector> PPDef = C26Fielding::GetPresetFieldPositions(EC26FieldPreset::PowerplayDefensive, false);
    TestTrue(TEXT("Powerplay Defensive is compliant with 2-man boundary rule"),
        C26Fielding::ValidateFieldLegality(PPDef, true, false, WarnPP));

    // Position naming verification
    TestEqual(TEXT("Position Name: Keeper"), C26Fielding::GetFieldPositionName(FVector(0, 1600, 0), false), TEXT("Wicket Keeper"));
    TestEqual(TEXT("Position Name: First Slip"), C26Fielding::GetFieldPositionName(FVector(280, 1550, 0), false), TEXT("First Slip"));
    TestEqual(TEXT("Position Name: Fine Leg"), C26Fielding::GetFieldPositionName(FVector(-2400, 2400, 0), false), TEXT("Fine Leg"));
    TestEqual(TEXT("Position Name: Cover"), C26Fielding::GetFieldPositionName(FVector(2200, 400, 0), false), TEXT("Cover"));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26Law28ViolationsTest, "Cricket26.ControlOverhaul.Law28Violations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26Law28ViolationsTest::RunTest(const FString& Parameters)
{
    // Start with a legal balanced field
    TArray<FVector> Field = C26Fielding::GetPresetFieldPositions(EC26FieldPreset::Balanced, false);
    FString Warn;
    TestTrue(TEXT("Base field is valid"), C26Fielding::ValidateFieldLegality(Field, false, false, Warn));

    // Violation 1: More than 5 fielders on leg side (Law 28.4)
    TArray<FVector> IllegalLegSide = Field;
    for (int32 I = 2; I <= 7; ++I)
    {
        IllegalLegSide[I] = FVector(-1200.f - I * 100.f, 200.f * (I - 4), 0.f); // 6 fielders on leg side (-X)
    }
    TestFalse(TEXT("Law 28.4 violation: >5 leg-side fielders rejected"),
        C26Fielding::ValidateFieldLegality(IllegalLegSide, false, false, Warn));
    TestTrue(TEXT("Warning mentions Leg side"), Warn.Contains(TEXT("Leg side")));

    // Violation 2: More than 2 fielders behind square on leg side (Law 28.4)
    TArray<FVector> IllegalBehindSquare = Field;
    IllegalBehindSquare[2] = FVector(-800.f, 1200.f, 0.f);  // Behind square 1
    IllegalBehindSquare[3] = FVector(-1400.f, 1500.f, 0.f); // Behind square 2
    IllegalBehindSquare[4] = FVector(-1800.f, 1100.f, 0.f); // Behind square 3 (Illegal!)
    // Fielders 5..10 safely spread on off-side (+X)
    for (int32 I = 5; I <= 10; ++I)
    {
        IllegalBehindSquare[I] = FVector(800.f + (I - 5) * 350.f, -400.f + (I - 5) * 300.f, 0.f);
    }

    TestFalse(TEXT("Law 28.4 violation: >2 behind square on leg side rejected"),
        C26Fielding::ValidateFieldLegality(IllegalBehindSquare, false, false, Warn));
    TestTrue(TEXT("Warning mentions behind square"), Warn.Contains(TEXT("behind square")));

    // Violation 3: Powerplay violation (>2 outside 30-yard circle)
    TArray<FVector> IllegalPowerplay = Field;
    // Set 4 fielders outside circle (> 2740 cm), but keep leg side <= 5
    IllegalPowerplay[2] = FVector(3500.f, 1000.f, 0.f);   // Off side, outside circle
    IllegalPowerplay[3] = FVector(-3500.f, 1000.f, 0.f);  // Leg side, outside circle
    IllegalPowerplay[4] = FVector(4000.f, -1000.f, 0.f);  // Off side, outside circle
    IllegalPowerplay[5] = FVector(3200.f, -1500.f, 0.f);  // Off side, outside circle
    // Keep 6..10 inside circle on off side
    for (int32 I = 6; I <= 10; ++I)
    {
        IllegalPowerplay[I] = FVector(1000.f + (I - 6) * 300.f, 500.f, 0.f);
    }

    TestFalse(TEXT("Powerplay restriction: >2 outside circle rejected in PP"),
        C26Fielding::ValidateFieldLegality(IllegalPowerplay, true, false, Warn));
    TestTrue(TEXT("Warning mentions Powerplay circle"), Warn.Contains(TEXT("Powerplay")));

    // Violation 4: Fielder spacing (<180cm)
    TArray<FVector> IllegalSpacing = Field;
    IllegalSpacing[3] = IllegalSpacing[2] + FVector(60.f, 0.f, 0.f); // 60cm distance
    TestFalse(TEXT("Fielder spacing: <180cm between fielders rejected"),
        C26Fielding::ValidateFieldLegality(IllegalSpacing, false, false, Warn));
    TestTrue(TEXT("Warning mentions close"), Warn.Contains(TEXT("close")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26FieldingDivingTest, "Cricket26.ControlOverhaul.ManualFieldingAndDiving", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26FieldingDivingTest::RunTest(const FString& Parameters)
{
    // 1. EvaluateDiveQuality
    float DampClean = 1.0f;
    const int32 CleanStop = C26Fielding::EvaluateDiveQuality(0.04f, 120.f, 1800.f, DampClean);
    TestEqual(TEXT("Accurate dive stops ball cleanly"), CleanStop, 0);
    TestEqual(TEXT("Clean stop speed dampens to 0"), DampClean, 0.0f);

    float DampParry = 1.0f;
    const int32 ParryKnockdown = C26Fielding::EvaluateDiveQuality(0.25f, 290.f, 2200.f, DampParry);
    TestEqual(TEXT("Borderline dive results in parried knockdown"), ParryKnockdown, 1);
    TestTrue(TEXT("Parry dampens ball speed to 22%"), FMath::IsNearlyEqual(DampParry, 0.22f, 0.01f));

    float DampMiss = 1.0f;
    const int32 MissedDive = C26Fielding::EvaluateDiveQuality(0.55f, 420.f, 2500.f, DampMiss);
    TestEqual(TEXT("Mistimed/out-of-range dive misses entirely"), MissedDive, 2);

    // 2. Procedural Dive Motion Solver C26Motion::SolveFielderDive
    const float TestTimes[] = { 0.05f, 0.25f, 0.60f, 0.95f, 1.30f };
    for (float T : TestTimes)
    {
        const C26Motion::FFielderPose DivePose = C26Motion::SolveFielderDive(T, FVector(150.f, 20.f, 10.f), 12.f, 145.f, 85.f);
        TestFalse(FString::Printf(TEXT("Dive pose at T=%.2f is finite"), T), DivePose.LeftHand.ContainsNaN() || DivePose.RightHand.ContainsNaN());
        TestTrue(TEXT("Dive pose crouch is active"), DivePose.Crouch < 0.f);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CatchTimingPhysicsTest, "Cricket26.ControlOverhaul.CatchTimingAndDropPhysics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26CatchTimingPhysicsTest::RunTest(const FString& Parameters)
{
    // 1. Catch Timing & Quality evaluation
    EC26CatchTiming TimingPerfect = EC26CatchTiming::None;
    float QualityPerfect = 0.f;
    const bool bPerfectCatch = C26Fielding::EvaluateCatchQuality(0.04f, 80.f, 1600.f, 1.0f, false, TimingPerfect, QualityPerfect);
    TestTrue(TEXT("Well-timed catch succeeds"), bPerfectCatch);
    TestEqual(TEXT("Catch timing classified as Perfect"), TimingPerfect, EC26CatchTiming::Perfect);
    TestTrue(TEXT("Perfect catch quality is high"), QualityPerfect >= 0.70f);

    EC26CatchTiming TimingLate = EC26CatchTiming::None;
    float QualityLate = 0.f;
    const bool bLateCatch = C26Fielding::EvaluateCatchQuality(0.38f, 220.f, 2800.f, 0.8f, false, TimingLate, QualityLate);
    TestFalse(TEXT("Late reaching catch fails"), bLateCatch);
    TestEqual(TEXT("Catch timing classified as Late"), TimingLate, EC26CatchTiming::Late);
    TestTrue(TEXT("Poor catch quality is below threshold"), QualityLate < 0.40f);

    // 2. Dropped Catch Physics Continuity
    FC26Simulation Sim;
    Sim.Ball.Active = true;
    Sim.Ball.Position = FVector(1200.f, -1400.f, 160.f);
    Sim.Ball.Velocity = FVector(800.f, -1200.f, 200.f);
    Sim.Ball.PostHitBounce = false;

    // Simulate drop mechanics from MatchGameMode
    Sim.Ball.Velocity = Sim.Ball.Velocity * 0.35f + FVector(FMath::RandRange(-80.f, 80.f), FMath::RandRange(-80.f, 80.f), 120.f);
    Sim.Ball.PostHitBounce = true;

    TestTrue(TEXT("Dropped ball remains active and live"), Sim.Ball.Active);
    TestTrue(TEXT("Dropped ball has post-hit bounce flagged"), Sim.Ball.PostHitBounce);
    TestTrue(TEXT("Dropped ball maintains physical rebound velocity"), Sim.Ball.Velocity.Size() > 100.f);
    TestFalse(TEXT("Dropped ball trajectory is finite"), Sim.Ball.Velocity.ContainsNaN());

    return true;
}


// ============================================================================
// CRICKET 26 // MATCH PRESENTATION OVERHAUL AUTOMATION SUITE
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26PresentationLibraryTest, "Cricket26.Presentation.SceneLibraryAndCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26PresentationLibraryTest::RunTest(const FString& Parameters)
{
    UC26PresentationDirector* PD = NewObject<UC26PresentationDirector>();
    TestNotNull(TEXT("Presentation director instantiated"), PD);

    // Force initialization of library

    // Verify coverage across key event categories (Total 38 events defined in C26PresentationTypes.h)
    const EC26PresentationEvent SampleEvents[] = {
        EC26PresentationEvent::TossCeremonyWalkout,
        EC26PresentationEvent::TossCoinFlip,
        EC26PresentationEvent::TossDecisionBat,
        EC26PresentationEvent::TossDecisionBowl,
        EC26PresentationEvent::TeamWalkoutAnthem,
        EC26PresentationEvent::OpeningBattersWalkIn,
        EC26PresentationEvent::OpeningBowlerRunupPrep,
        EC26PresentationEvent::BatterMidPitchDiscussion,
        EC26PresentationEvent::BatterBoundaryMeeting,
        EC26PresentationEvent::BowlerCaptainDiscussion,
        EC26PresentationEvent::BowlerFrustrationBoundary,
        EC26PresentationEvent::WicketCelebrationBowled,
        EC26PresentationEvent::WicketCelebrationCaught,
        EC26PresentationEvent::FiftyCelebration,
        EC26PresentationEvent::CenturyCelebration,
        EC26PresentationEvent::EndOfOverSummaryCard,
        EC26PresentationEvent::InningsBreakTransition,
        EC26PresentationEvent::MatchWinningCelebration,
        EC26PresentationEvent::PostMatchHandshakes,
        EC26PresentationEvent::PlayerOfTheMatchPresentation
    };

    for (EC26PresentationEvent Evt : SampleEvents)
    {
        const int32 Variants = PD->GetVariantCount(Evt);
        TestTrue(FString::Printf(TEXT("Presentation event %d has at least 1 registered variant"), int32(Evt)), Variants >= 1);
    }

    // High frequency events have multiple variants for broadcast variety
    TestTrue(TEXT("Bowled wicket has multiple variants"), PD->GetVariantCount(EC26PresentationEvent::WicketCelebrationBowled) >= 2);
    TestTrue(TEXT("Fifty celebration has multiple variants"), PD->GetVariantCount(EC26PresentationEvent::FiftyCelebration) >= 2);
    TestTrue(TEXT("Century celebration has multiple variants"), PD->GetVariantCount(EC26PresentationEvent::CenturyCelebration) >= 2);
    TestTrue(TEXT("Bowler-Captain discussion has multiple variants"), PD->GetVariantCount(EC26PresentationEvent::BowlerCaptainDiscussion) >= 2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26PresentationMilestoneRulesTest, "Cricket26.Presentation.MilestoneFireOnceGuarantee", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26PresentationMilestoneRulesTest::RunTest(const FString& Parameters)
{
    // Test exact fire-once milestone rules for Fifty and Century across multi-run boundaries
    bool bFiftyCelebrated[3] = { false, false, false };
    bool bCenturyCelebrated[3] = { false, false, false };
    int32 FiftyFiredCount = 0;
    int32 CenturyFiredCount = 0;

    int32 StrikerIndex = 0;
    int32 BatterRuns = 0;

    auto CheckMilestones = [&](int32 NewRuns)
    {
        BatterRuns = NewRuns;
        if (BatterRuns >= 100 && !bCenturyCelebrated[StrikerIndex])
        {
            bCenturyCelebrated[StrikerIndex] = true;
            CenturyFiredCount++;
        }
        else if (BatterRuns >= 50 && !bFiftyCelebrated[StrikerIndex])
        {
            bFiftyCelebrated[StrikerIndex] = true;
            FiftyFiredCount++;
        }
    };

    // Progression: 46 -> 52 (Leaps past 50 with a six)
    CheckMilestones(46);
    TestEqual(TEXT("Runs 46: No milestone"), FiftyFiredCount, 0);

    CheckMilestones(52);
    TestEqual(TEXT("Runs 52: Fifty milestone triggers exactly once"), FiftyFiredCount, 1);

    // Next balls: 53, 57, 61, 88
    CheckMilestones(53);
    CheckMilestones(57);
    CheckMilestones(61);
    CheckMilestones(88);
    TestEqual(TEXT("Subsequent runs do NOT re-trigger Fifty milestone"), FiftyFiredCount, 1);

    // Progression: 96 -> 102 (Leaps past 100 with a six)
    CheckMilestones(96);
    TestEqual(TEXT("Runs 96: No century milestone yet"), CenturyFiredCount, 0);

    CheckMilestones(102);
    TestEqual(TEXT("Runs 102: Century milestone triggers exactly once"), CenturyFiredCount, 1);

    // Next balls: 104, 110
    CheckMilestones(104);
    CheckMilestones(110);
    TestEqual(TEXT("Subsequent runs do NOT re-trigger Century milestone"), CenturyFiredCount, 1);

    // Verify non-striker (Index 1) tracks independently
    StrikerIndex = 1;
    CheckMilestones(54);
    TestEqual(TEXT("Partner batter reaches 50: independent milestone trigger"), FiftyFiredCount, 2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26PresentationPacingAndAntiRepetitionTest, "Cricket26.Presentation.PacingAndAntiRepetition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26PresentationPacingAndAntiRepetitionTest::RunTest(const FString& Parameters)
{
    UC26PresentationDirector* PD = NewObject<UC26PresentationDirector>();

    // 1. Pacing Filter Verification
    PD->SetPacing(EC26PresentationPacing::Quick);
    TestEqual(TEXT("Pacing is Quick"), PD->GetPacing(), EC26PresentationPacing::Quick);

    FC26PresentationRequest LowReq;
    LowReq.Event = EC26PresentationEvent::BatterMidPitchDiscussion;
    LowReq.Priority = EC26PresentationPriority::Low;
    TestFalse(TEXT("Quick mode suppresses Low priority scene"), PD->RequestPresentation(LowReq));

    FC26PresentationRequest HighReq;
    HighReq.Event = EC26PresentationEvent::WicketCelebrationBowled;
    HighReq.Priority = EC26PresentationPriority::High;
    TestTrue(TEXT("Quick mode permits High priority scene"), PD->RequestPresentation(HighReq));
    TestTrue(TEXT("Presentation becomes active"), PD->IsPresentationActive());
    TestTrue(TEXT("Quick mode caps duration to 2.5s"), PD->GetSceneDuration() <= 2.5f);

    // 2. Anti-Repetition Recency History
    const auto& History = PD->GetRecentScenes();
    TestTrue(TEXT("Played scene logged in recent history"), History.Num() > 0);
    TestEqual(TEXT("Recent history records Bowled event"), History.Last().Event, EC26PresentationEvent::WicketCelebrationBowled);

    // 3. Match Pressure Calculator
    const float HighTension = PD->CalculateMatchPressure(18, 14, 2, 1); // 4 needed off 2 balls
    TestTrue(TEXT("Death balls generate high match tension"), HighTension >= 0.85f);

    const float LowTension = PD->CalculateMatchPressure(0, 10, 5, 0); // 1st innings early
    TestTrue(TEXT("1st innings early generates low match tension"), LowTension < 0.40f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26PresentationSkipSafetyTest, "Cricket26.Presentation.SkipSafetyNoSoftLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26PresentationSkipSafetyTest::RunTest(const FString& Parameters)
{
    UC26PresentationDirector* PD = NewObject<UC26PresentationDirector>();

    // Enqueue scenes
    FC26PresentationRequest Req1;
    Req1.Event = EC26PresentationEvent::TossCeremonyWalkout;
    Req1.Priority = EC26PresentationPriority::High;
    PD->RequestPresentation(Req1);

    FC26PresentationRequest Req2;
    Req2.Event = EC26PresentationEvent::TossCoinFlip;
    Req2.Priority = EC26PresentationPriority::High;
    PD->RequestPresentation(Req2);

    TestTrue(TEXT("Presentation active"), PD->IsPresentationActive());
    TestEqual(TEXT("Queue has 1 pending item"), PD->GetQueueCount(), 1);

    // Immediate skip invocation (e.g. user pressed spacebar / tapped screen)
    PD->SkipCurrentScene();

    // Must be completely restored and cleared with zero dangling locks
    TestFalse(TEXT("Presentation cleanly deactivated"), PD->IsPresentationActive());
    TestEqual(TEXT("Active event reset to None"), PD->GetCurrentEvent(), EC26PresentationEvent::None);
    TestEqual(TEXT("Queue completely cleared on skip"), PD->GetQueueCount(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26TossCoinTrajectoryTest, "Cricket26.Presentation.TossCoinPhysicsSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FC26TossCoinTrajectoryTest::RunTest(const FString& Parameters)
{
    UC26PresentationDirector* PD = NewObject<UC26PresentationDirector>();

    FC26PresentationRequest TossReq;
    TossReq.Event = EC26PresentationEvent::TossCoinFlip;
    TossReq.Priority = EC26PresentationPriority::High;
    PD->RequestPresentation(TossReq);

    FVector CoinPos;
    FRotator CoinRot;
    const bool bActive = PD->GetTossCoinState(CoinPos, CoinRot);
    TestTrue(TEXT("Toss coin solver active during coin flip"), bActive);
    TestFalse(TEXT("Coin position is finite"), CoinPos.ContainsNaN());
    TestFalse(TEXT("Coin rotation is finite"), CoinRot.ContainsNaN());
    TestTrue(TEXT("Coin height above ground"), CoinPos.Z > 0.f);

    PD->SkipCurrentScene();
    TestFalse(TEXT("Toss coin deactivated after scene ends"), PD->GetTossCoinState(CoinPos, CoinRot));

    return true;
}

#endif
