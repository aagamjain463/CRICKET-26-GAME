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
    // ApplyAuthoredClip via SelectBattingClip/SelectBowlingClip), so their geometry is
    // production behaviour, not decoration. This samples the IMPORTED assets -- after
    // the FBX importer has done its own conversion -- and re-asserts the same facts
    // Tools/correct_authored_anim.py verifies offline on the curves. If it fails with
    // an asset error, run Tools/ImportAnimations.py; if it fails a geometry check,
    // the pipeline in Docs/AUTHORED_ANIMATION.md was bypassed.
    constexpr float Fps=24.f;
    struct FClipSpec{const TCHAR* Name;bool Batting;bool Umpire;};
    const FClipSpec Specs[]={
        {TEXT("A_C26_BattingDrive"),true,false},{TEXT("A_C26_BattingPull"),true,false},
        {TEXT("A_C26_BattingCut"),true,false},{TEXT("A_C26_BattingSweep"),true,false},
        {TEXT("A_C26_BattingDefence"),true,false},{TEXT("A_C26_BattingBackFootDefence"),true,false},
        {TEXT("A_C26_BattingUpperCut"),true,false},{TEXT("A_C26_BattingHook"),true,false},
        {TEXT("A_C26_BattingLoftedDrive"),true,false},{TEXT("A_C26_BattingGlance"),true,false},
        {TEXT("A_C26_UmpireSignalWide"),false,true},{TEXT("A_C26_UmpireSignalSix"),false,true},
        {TEXT("A_C26_UmpireSignalOut"),false,true},{TEXT("A_C26_UmpireSignalFour"),false,true},
        {TEXT("A_C26_BowlingPace"),false,false},{TEXT("A_C26_BowlingOffSpin"),false,false},
        {TEXT("A_C26_BowlingLegSpin"),false,false},
    };
    // Skeleton-space transform of one bone chain: GetBoneTransform returns LOCALS
    // seeded from the ref pose, so the chain composes parent-first like the athlete does.
    auto Bone=[](UAnimSequence* Clip,float Time,std::initializer_list<FName> Chain)->FVector
    {
        const FReferenceSkeleton& Ref=Clip->GetSkeleton()->GetReferenceSkeleton();
        const FAnimExtractContext Ctx(Time,false);
        FTransform Acc=FTransform::Identity;
        for(FName B:Chain)
        {
            const int32 Idx=Ref.FindBoneIndex(B);
            if(Idx<0){return FVector(ForceInit);}
            FTransform Local=Ref.GetRefBonePose()[Idx];
            Clip->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Idx),Ctx,false);
            Acc=Acc*Local;
        }
        return Acc.GetTranslation();
    };
    const FName Hips(TEXT("Hips")),Spine(TEXT("Spine")),S1(TEXT("Spine1")),S2(TEXT("Spine2")),Neck(TEXT("Neck")),Head(TEXT("Head"));
    const FName LSh(TEXT("LeftShoulder")),LArm(TEXT("LeftArm")),LFore(TEXT("LeftForeArm")),LHand(TEXT("LeftHand"));
    const FName RSh(TEXT("RightShoulder")),RArm(TEXT("RightArm")),RFore(TEXT("RightForeArm")),RHand(TEXT("RightHand"));
    const FName LHips(TEXT("Hips"));
    auto HandMid=[&](UAnimSequence* C,float T){return (Bone(C,T,{LHips,Spine,S1,S2,LSh,LArm,LFore,LHand})+Bone(C,T,{LHips,Spine,S1,S2,RSh,RArm,RFore,RHand}))/2.f;};
    auto LeftHand=[&](UAnimSequence* C,float T){return Bone(C,T,{LHips,Spine,S1,S2,LSh,LArm,LFore,LHand});};
    auto RightHand=[&](UAnimSequence* C,float T){return Bone(C,T,{LHips,Spine,S1,S2,RSh,RArm,RFore,RHand});};
    // The body frame from the SHOULDER line, not the toes: shots like the cut pivot
    // the front foot open, which rotates a toe-relative frame past 60 degrees.
    auto Forward=[&](UAnimSequence* C,float T)
    {
        const FVector LS=Bone(C,T,{LHips,Spine,S1,S2,LSh}),RS=Bone(C,T,{LHips,Spine,S1,S2,RSh});
        const FVector D=LS-RS;
        FVector F=FVector::CrossProduct(D,FVector::UpVector);
        F.Z=0;
        return F.GetSafeNormal();
    };
    const float Defining=FMath::Min(23.f/Fps,1.f);   // contact (batting) / release (bowling)
    for(const FClipSpec& Spec:Specs)
    {
        UAnimSequence* Clip=LoadObject<UAnimSequence>(nullptr,FString::Printf(TEXT("/Game/Cricket26/Animations/%s.%s"),Spec.Name,Spec.Name));
        if(!Clip)
        {
            AddError(FString::Printf(TEXT("Authored clip %s missing -- run Tools/ImportAnimations.py (imports ArtSource/Exports/Animations/Corrected)"),Spec.Name));
            continue;
        }
        const FString Tag=Spec.Name;
        // 1. Chest faces the bowler at the stance and the defining frame. This is the
        //    gate that a whole-body yaw bug cannot hide from (one did, historically).
        auto YawDeg=[&](float T){return FMath::RadiansToDegrees(FMath::Atan2(Forward(Clip,T).X,Forward(Clip,T).Y));};
        TestTrue(*FString::Printf(TEXT("%s: stance chest faces the bowler"),*Tag),FMath::Abs(YawDeg(0.f))<20.f);
        TestTrue(*FString::Printf(TEXT("%s: defining-frame chest faces the bowler"),*Tag),FMath::Abs(YawDeg(Defining))<35.f);
        // 2. Hands on the handle / ball at the stance, and together at the defining frame.
        if(!Spec.Umpire)  // an umpire's hands hang at his sides at the ready
        {
            TestTrue(*FString::Printf(TEXT("%s: hands together at stance"),*Tag),FVector::Distance(LeftHand(Clip,0.f),RightHand(Clip,0.f))<25.f);
            TestTrue(*FString::Printf(TEXT("%s: hands together at defining frame"),*Tag),FVector::Distance(LeftHand(Clip,Defining),RightHand(Clip,Defining))<25.f);
        }
        if(Spec.Umpire)
        {
            const float P14=14.f/Fps;
            const FVector P=Bone(Clip,P14,{Hips});
            const FVector OffDir=FVector::CrossProduct(FVector::UpVector,Forward(Clip,P14));
            const FString Sig=Spec.Name+6;   // skip "A_C26_" -> "UmpireSignalWide" etc
            const FVector R=RightHand(Clip,P14),L=LeftHand(Clip,P14);
            if(Sig==TEXT("UmpireSignalWide"))
            {
                TestTrue(*FString::Printf(TEXT("%s: arms straight out at shoulder height"),*Tag),
                         FVector::DotProduct(R-P,OffDir)>120.f&&FVector::DotProduct(L-P,OffDir)<-120.f&&R.Z<P.Z+145.f&&R.Z>P.Z+60.f);
                TestTrue(*FString::Printf(TEXT("%s: wide holds"),*Tag),
                         FVector::DotProduct(RightHand(Clip,36.f/Fps)-Bone(Clip,36.f/Fps,{Hips}),FVector::CrossProduct(FVector::UpVector,Forward(Clip,36.f/Fps)))>120.f);
            }
            if(Sig==TEXT("UmpireSignalSix"))
                TestTrue(*FString::Printf(TEXT("%s: both arms straight up"),*Tag),R.Z>340.f&&L.Z>340.f);
            if(Sig==TEXT("UmpireSignalOut"))
                TestTrue(*FString::Printf(TEXT("%s: one arm up, one down"),*Tag),R.Z>340.f&&L.Z<P.Z+70.f);
            if(Sig==TEXT("UmpireSignalFour"))
            {
                const FVector P24=Bone(Clip,24.f/Fps,{Hips});
                const FVector R24=RightHand(Clip,24.f/Fps),L24=LeftHand(Clip,24.f/Fps);
                const FVector Off24=FVector::CrossProduct(FVector::UpVector,Forward(Clip,24.f/Fps));
                TestTrue(*FString::Printf(TEXT("%s: finish arms out at waist height"),*Tag),
                         FVector::DotProduct(R24-P24,Off24)>100.f&&FVector::DotProduct(L24-P24,Off24)<-100.f&&R24.Z<P24.Z+100.f);
            }
        }
        if(Spec.Batting)
        {
            const FVector H=HandMid(Clip,Defining),P=Bone(Clip,Defining,{Hips}),F=Forward(Clip,Defining);
            const float Front=FVector::DotProduct(H-P,F);
            const FString Shot=Spec.Name+6;   // skip "A_C26_"
            if(Shot==TEXT("BattingDrive")||Shot==TEXT("BattingDefence"))
                TestTrue(*FString::Printf(TEXT("%s: contact hands in front of the body"),*Tag),Front>10.f);
            // Mesh +X is the character's LEFT, so the right-hander's OFF side
            // (his right) is up x forward, and the leg side is its negation.
            const FVector OffSide=FVector::CrossProduct(FVector::UpVector,Forward(Clip,Defining));
            if(Shot==TEXT("BattingPull"))
            {
                TestTrue(*FString::Printf(TEXT("%s: pull is a horizontal shot"),*Tag),Front>5.f&&H.Z>P.Z+30.f);
                TestTrue(*FString::Printf(TEXT("%s: pull follows through to the leg side"),*Tag),
                         FVector::DotProduct(HandMid(Clip,29.f/Fps)-P,OffSide)<-10.f);
            }
            if(Shot==TEXT("BattingCut"))
                TestTrue(*FString::Printf(TEXT("%s: cut slashes to the off side"),*Tag),
                         FVector::DotProduct(H-P,OffSide)>10.f);
            if(Shot==TEXT("BattingBackFootDefence"))
                // Blocked beside the body, not pressed forward: the whole point
                // of the back-foot defence (the forward one is 56 cm out front).
                TestTrue(*FString::Printf(TEXT("%s: blocked beside the body"),*Tag),Front<30.f&&Front>-10.f&&H.Z<P.Z+60.f);
            if(Shot==TEXT("BattingUpperCut"))
                // High off-side contact behind the body line, steered over the slips.
                TestTrue(*FString::Printf(TEXT("%s: upper cut high and behind the line"),*Tag),
                         FVector::DotProduct(H-P,OffSide)>10.f&&H.Z>P.Z+85.f&&Front<20.f);
            if(Shot==TEXT("BattingHook"))
                // Head-height contact is what makes a hook (the pull sits at chest
                // height): gate it above the pull's +30 band.
                TestTrue(*FString::Printf(TEXT("%s: hook meets the ball at head height"),*Tag),H.Z>P.Z+85.f);
            if(Shot==TEXT("BattingLoftedDrive"))
            {
                // The contact is a drive's own (low, in front); the loft reads in
                // the finish, which goes overhead -- well above the drive's own
                // shoulder-height follow-through.
                TestTrue(*FString::Printf(TEXT("%s: lofted drive met low and in front"),*Tag),Front>10.f&&H.Z<P.Z+60.f);
                TestTrue(*FString::Printf(TEXT("%s: lofted drive finishes overhead"),*Tag),
                         HandMid(Clip,29.f/Fps).Z-Bone(Clip,29.f/Fps,{Hips}).Z>150.f);
            }
            if(Shot==TEXT("BattingGlance"))
            {
                // Low contact off the hip, and a deflection rather than a swing:
                // the hands cross to fine leg and stay low.
                TestTrue(*FString::Printf(TEXT("%s: glance met low off the hip"),*Tag),H.Z<P.Z+45.f&&Front>5.f);
                const FVector G=HandMid(Clip,29.f/Fps),GP=Bone(Clip,29.f/Fps,{Hips});
                TestTrue(*FString::Printf(TEXT("%s: glance deflected soft to fine leg"),*Tag),
                         FVector::DotProduct(G-GP,OffSide)<-25.f&&G.Z<GP.Z+60.f);
            }
            if(Shot==TEXT("BattingSweep"))
                TestTrue(*FString::Printf(TEXT("%s: sweep is a deep crouch"),*Tag),P.Z<150.f&&Front>5.f);
        }
        else
        {
            const FVector HeadP=Bone(Clip,Defining,{Hips,Spine,S1,S2,Neck,Head});
            const float Hi=FMath::Max(LeftHand(Clip,Defining).Z,RightHand(Clip,Defining).Z);
            const FString Style=Spec.Name+6;
            // Finger spin releases AT head height; quicks and wrist spinners above it.
            const float Gate=Style==TEXT("BowlingOffSpin")?20.f:40.f;
            TestTrue(*FString::Printf(TEXT("%s: release arm over the shoulder"),*Tag),Hi>HeadP.Z+Gate);
            const FVector Stance=Bone(Clip,0.f,{Hips}),Late=Bone(Clip,38.f/Fps,{Hips});
            TestTrue(*FString::Printf(TEXT("%s: travels down the pitch"),*Tag),FVector::DotProduct(Late-Stance,Forward(Clip,0.f))>20.f);
        }
    }
    return true;
}
#endif