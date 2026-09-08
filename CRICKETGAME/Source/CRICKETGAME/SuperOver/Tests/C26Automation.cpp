#include "Misc/AutomationTest.h"
#include "../Core/C26Rules.h"
#include "../C26Simulation.h"

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
#endif
