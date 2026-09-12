#include "Misc/AutomationTest.h"
#include "../C26Simulation.h"
#include "../C26Delivery.h"
#include "../C26Motion.h"
#include "../C26Stadium.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26BroadcastVisualsTest,"Cricket26.Presentation.BroadcastVisuals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26BroadcastVisualsTest::RunTest(const FString&)
{
    // Crowd visual states: bigger moments must target strictly more rise, so a six visibly lifts
    // the ground harder than applause and calm always settles back near still.
    TestEqual(TEXT("Six lifts the ground fully"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Six),1.f);
    TestEqual(TEXT("Calm settles near still"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Calm),.06f);
    TestTrue(TEXT("Wicket outrises boundary"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Wicket)>AC26Stadium::CrowdTargetForState(EC26CrowdState::Boundary));
    TestTrue(TEXT("Boundary outrises excited"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Boundary)>AC26Stadium::CrowdTargetForState(EC26CrowdState::Excited));
    TestTrue(TEXT("Applause outrises anticipation"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Excited)>AC26Stadium::CrowdTargetForState(EC26CrowdState::Anticipation));
    TestTrue(TEXT("Anticipation outrises calm"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Anticipation)>AC26Stadium::CrowdTargetForState(EC26CrowdState::Calm));
    TestTrue(TEXT("Tension lingers above calm"),AC26Stadium::CrowdTargetForState(EC26CrowdState::Tense)>AC26Stadium::CrowdTargetForState(EC26CrowdState::Calm));
    for(auto S:{EC26CrowdState::Calm,EC26CrowdState::Anticipation,EC26CrowdState::Excited,EC26CrowdState::Boundary,
        EC26CrowdState::Six,EC26CrowdState::Wicket,EC26CrowdState::Tense,EC26CrowdState::Win})
        TestTrue(TEXT("Every crowd state eases at a positive rate"),AC26Stadium::CrowdRateForState(S)>0.f);
    // Environment grade: day is the brightest session, night the darkest; the shipped night look
    // is pinned so a future tweak cannot silently re-expose the prototype grade.
    TestEqual(TEXT("Night exposure pinned"),AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile::Night),-.45f);
    TestTrue(TEXT("Day brighter than afternoon"),AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile::ClearDay)>AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile::LateAfternoon));
    TestTrue(TEXT("Afternoon brighter than night"),AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile::LateAfternoon)>AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile::Night));
    // Pitch conditions: used is neutral, fresh skews green, dry/worn skew pale.
    const auto Used=AC26Stadium::PitchTintForCondition(EC26PitchCondition::Used);
    TestTrue(TEXT("Used condition is neutral"),Used.Equals(FLinearColor(1,1,1,1)));
    TestTrue(TEXT("Fresh condition skews green"),AC26Stadium::PitchTintForCondition(EC26PitchCondition::Fresh).G>=AC26Stadium::PitchTintForCondition(EC26PitchCondition::Fresh).R);
    TestTrue(TEXT("Dry condition skews pale"),AC26Stadium::PitchTintForCondition(EC26PitchCondition::Dry).R>=1.f);
    TestTrue(TEXT("Worn condition skews pale"),AC26Stadium::PitchTintForCondition(EC26PitchCondition::Worn).R>=1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26AuthoredClipsTest,"Cricket26.Anim.AuthoredClips",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26AuthoredClipsTest::RunTest(const FString&)
{
    // The authored clips drive the striker and the bowler in-match now (AC26Athlete::
    // ApplyAuthoredClip), so their geometry is production behaviour, not decoration. This
    // samples the IMPORTED assets -- after the FBX importer has done its own conversion --
    // and re-asserts the same facts Tools/correct_authored_anim.py verifies offline on the
    // curves. If it fails with an asset error, run Tools/ImportAnimations.py; if it fails
    // a geometry check, the pipeline in Docs/AUTHORED_ANIMATION.md was bypassed.
    constexpr float Fps=24.f;
    UAnimSequence* Batting=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Animations/A_C26_BattingDrive.A_C26_BattingDrive"));
    UAnimSequence* Bowling=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Animations/A_C26_BowlingPace.A_C26_BowlingPace"));
    if(!Batting||!Bowling)
    {
        AddError(TEXT("Authored clips missing from /Game/Cricket26/Animations -- run Tools/ImportAnimations.py (imports ArtSource/Exports/Animations/Corrected)"));
        return false;
    }
    // Skeleton-space transform of one bone at a time: GetBoneTransform returns LOCALS
    // seeded from the ref pose, so the chain is composed parent-first like the athlete does.
    auto Bone=[&](UAnimSequence* Clip,float Time,std::initializer_list<FName> Chain)->FVector
    {
        const FReferenceSkeleton& Ref=Clip->GetSkeleton()->GetReferenceSkeleton();
        const FAnimExtractContext Ctx(Time,false);
        FTransform Acc=FTransform::Identity;
        for(FName B:Chain)
        {
            const int32 Idx=Ref.FindBoneIndex(B);
            if(Idx<0){AddError(FString::Printf(TEXT("bone %s missing on clip skeleton"),*B.ToString()));return FVector(ForceInit);}
            FTransform Local=Ref.GetRefBonePose()[Idx];
            Clip->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Idx),Ctx,false);
            Acc=Acc*Local;
        }
        return Acc.GetTranslation();
    };
    const FName Hips(TEXT("Hips")),Spine(TEXT("Spine")),S1(TEXT("Spine1")),S2(TEXT("Spine2")),Neck(TEXT("Neck")),Head(TEXT("Head"));
    const FName LSh(TEXT("LeftShoulder")),LArm(TEXT("LeftArm")),LFore(TEXT("LeftForeArm")),LHand(TEXT("LeftHand"));
    const FName RSh(TEXT("RightShoulder")),RArm(TEXT("RightArm")),RFore(TEXT("RightForeArm")),RHand(TEXT("RightHand"));
    const FName LUp(TEXT("LeftUpLeg")),LLeg(TEXT("LeftLeg")),LFoot(TEXT("LeftFoot")),LToe(TEXT("LeftToeBase"));
    auto HandMid=[&](UAnimSequence* C,float T){return (Bone(C,T,{Hips,Spine,S1,S2,LSh,LArm,LFore,LHand})+Bone(C,T,{Hips,Spine,S1,S2,RSh,RArm,RFore,RHand}))/2.f;};
    // Forward on the horizontal plane, measured from the posed foot like the offline verifier.
    auto Forward=[&](UAnimSequence* C,float T)
    {
        const FVector Toe=Bone(C,T,{Hips,LUp,LLeg,LFoot,LToe}),Foot=Bone(C,T,{Hips,LUp,LLeg,LFoot});
        FVector F=Toe-Foot;F.Z=0;return F.GetSafeNormal();
    };
    auto HandsForward=[&](UAnimSequence* C,float T)
    {
        const FVector HipsP=Bone(C,T,{Hips});
        return FVector::DotProduct(HandMid(C,T)-HipsP,Forward(C,T));
    };
    // BATTING: the drive goes through the line, not through the body.
    const float Span=FVector::Distance(Bone(Batting,23.f/Fps,{Hips,Spine,S1,S2,LSh,LArm,LFore,LHand}),Bone(Batting,23.f/Fps,{Hips,Spine,S1,S2,RSh,RArm,RFore,RHand}));
    TestTrue(TEXT("Batting contact: hands together on the handle"),Span<25.f);
    TestTrue(TEXT("Batting contact: hands in FRONT of the body"),HandsForward(Batting,23.f/Fps)>10.f);
    TestTrue(TEXT("Batting backlift: hands behind the body"),HandsForward(Batting,13.f/Fps)<-10.f);
    TestTrue(TEXT("Batting stance: hands on the handle"),FMath::Abs(HandsForward(Batting,1.f/Fps))<40.f);
    // BOWLING: overhead release, ball in both hands at the mark, travel down the pitch.
    const FVector RH=Bone(Bowling,31.f/Fps,{Hips,Spine,S1,S2,RSh,RArm,RFore,RHand}),LH=Bone(Bowling,31.f/Fps,{Hips,Spine,S1,S2,LSh,LArm,LFore,LHand});
    const FVector HeadP=Bone(Bowling,31.f/Fps,{Hips,Spine,S1,S2,Neck,Head});
    TestTrue(TEXT("Bowling release: arm above the head"),FMath::Max(RH.Z,LH.Z)>HeadP.Z+40.f);
    const float MarkSpan=FVector::Distance(Bone(Bowling,0.f,{Hips,Spine,S1,S2,LSh,LArm,LFore,LHand}),Bone(Bowling,0.f,{Hips,Spine,S1,S2,RSh,RArm,RFore,RHand}));
    TestTrue(TEXT("Bowling mark: ball held in both hands"),MarkSpan<25.f);
    const FVector Travel=Bone(Bowling,38.f/Fps,{Hips})-Bone(Bowling,0.f,{Hips});
    TestTrue(TEXT("Bowling: travels down the pitch"),FVector::DotProduct(Travel,Forward(Bowling,0.f))>20.f);
    return true;
}
#endif
