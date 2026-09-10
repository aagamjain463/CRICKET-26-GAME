#include "Misc/AutomationTest.h"
#include "../C26Simulation.h"
#include "../C26Delivery.h"
#include "../C26Motion.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26DeliveryPlanTest,"Cricket26.Production.DeliveryPlan",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26DeliveryPlanTest::RunTest(const FString&)
{
    FC26DeliveryPlan Selected;Selected.Line=58;Selected.Length=805;
    for(EC26Delivery Type:{EC26Delivery::Pace,EC26Delivery::Outswing,EC26Delivery::Inswing,EC26Delivery::Slower})
    {
        Selected.Type=Type;C26Delivery::Shape(Selected);
        TestEqual(TEXT("Variation preserves line"),Selected.Line,58.f);
        TestEqual(TEXT("Variation preserves yorker target"),Selected.Length,805.f);
        const auto Locked=Selected,Perfect=C26Delivery::Execute(Locked,0),Late=C26Delivery::Execute(Locked,.9f);
        TestEqual(TEXT("Perfect release preserves line"),Perfect.Line,Locked.Line);
        TestTrue(TEXT("Mistimed release changes accuracy and pace"),Late.Line>Perfect.Line&&Late.Speed<Perfect.Speed);
        TestEqual(TEXT("Execution leaves locked plan immutable"),Locked.Length,805.f);
    }
    Selected.Type=EC26Delivery::Pace;Selected.Length=70;C26Delivery::Shape(Selected);const float ShortBounce=Selected.Bounce;
    Selected.Length=805;C26Delivery::Shape(Selected);TestTrue(TEXT("Short length and yorker bounce differ"),ShortBounce>Selected.Bounce);
    const auto Pose=C26Motion::Pace(C26Field::ReleasePoseTime,12);
    TestTrue(TEXT("Hero action plants the front foot at release"),FMath::IsNearlyEqual(Pose.LeftFoot.Z,12.f));
    TestTrue(TEXT("Bowling hand is above head at release"),Pose.RightHand.Z>205.f);
    TestFalse(TEXT("Cannot go from result directly to delivery"),C26ValidTransition(EC26Phase::Result,EC26Phase::Delivery));
    TestFalse(TEXT("Cannot skip release"),C26ValidTransition(EC26Phase::RunUp,EC26Phase::InPlay));
    TestTrue(TEXT("Replay can return controls for next ball"),C26ValidTransition(EC26Phase::Replay,EC26Phase::Ready));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CadenceTest,"Cricket26.Production.FrameRateAndShots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26CadenceTest::RunTest(const FString&)
{
    for(float Length:{70.f,420.f,630.f,805.f})
    {
        FC26DeliveryPlan P;P.Line=12;P.Length=Length;C26Delivery::Shape(P);
        FC26Simulation A,B;A.Release(P,FVector(-20,-980,208));B.Release(P,FVector(-20,-980,208));
        for(int I=0;I<18;++I)A.Step(1.f/30.f);
        for(int I=0;I<72;++I)B.Step(1.f/120.f);
        TestTrue(TEXT("Ball flight is stable from 30 to 120 FPS"),A.Ball.Position.Equals(B.Ball.Position,.3f));
        TestFalse(TEXT("Length trajectory finite"),A.ContactPosition.ContainsNaN());
    }
    FRandomStream R(26);
    auto Strike=[&](float Error,bool Loft,float Angle,float Power,float Length)
    {
        FC26Simulation S;FC26DeliveryPlan P;P.Line=8;P.Length=Length;C26Delivery::Shape(P);S.Release(P,FVector(-20,-980,208));S.Step(S.ContactTime);
        FC26ShotIntent I;I.Power=Power;I.Angle=Angle;I.Loft=Loft;I.Stride=Length<180?-.8f:.75f;
        const auto Contact=S.Hit(I,Error,1,R);
        return Contact;
    };
    const auto Perfect=Strike(0,false,35,1,630),Poor=Strike(.16,false,35,1,630),Miss=Strike(.5,false,35,1,630);
    TestTrue(TEXT("Timing changes contact quality"),Perfect.Quality>Poor.Quality);
    TestTrue(TEXT("Grossly early/late input misses"),Miss.Timing==EC26Timing::Miss);
    TestTrue(TEXT("Full off ball produces a cover drive"),Perfect.Shot==TEXT("COVER DRIVE"));
    TestTrue(TEXT("Short ball produces a pull"),Strike(0,false,-70,.8f,70).Shot==TEXT("PULL"));
    TestTrue(TEXT("Loft changes flight rather than assigning runs"),Strike(0,true,0,1,630).Velocity.Z>Strike(0,false,0,1,630).Velocity.Z);
    FC26AI AI;AI.Reset(2626);C26::Match M;M.Reset();TSet<int> Variations;
    for(int I=0;I<30;++I)Variations.Add(int(AI.Bowl(M,1).Type));
    TestTrue(TEXT("AI uses varied deliveries"),Variations.Num()>=3);
    return true;
}
#endif
