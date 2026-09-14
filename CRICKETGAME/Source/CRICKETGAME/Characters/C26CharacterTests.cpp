#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "C26CharacterPresentationComponent.h"
#include "C26CricketerAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CharacterRoleTest,"Cricket26.Characters.RoleIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26CharacterRoleTest::RunTest(const FString& Parameters)
{
    for(const auto Role:{EC26VisualRole::Fielder,EC26VisualRole::Bowler,EC26VisualRole::Umpire})
    for(const auto Slot:{EC26EquipmentSlot::Bat,EC26EquipmentSlot::BattingPadL,EC26EquipmentSlot::BattingPadR,
        EC26EquipmentSlot::KeeperPadL,EC26EquipmentSlot::KeeperPadR,EC26EquipmentSlot::BattingGloveL,
        EC26EquipmentSlot::BattingGloveR,EC26EquipmentSlot::KeeperGloveL,EC26EquipmentSlot::KeeperGloveR,EC26EquipmentSlot::Helmet})
        TestFalse(TEXT("Fielding/umpire roles reject protected player gear"),C26Character::Allows(Role,Slot));
    TestTrue(TEXT("Non-striker remains equipped"),C26Character::Requires(EC26VisualRole::NonStriker,EC26EquipmentSlot::Bat));
    TestTrue(TEXT("Keeper-specific gloves mandatory"),C26Character::Requires(EC26VisualRole::Keeper,EC26EquipmentSlot::KeeperGloveR));
    TestFalse(TEXT("Keeper cannot carry bat"),C26Character::Allows(EC26VisualRole::Keeper,EC26EquipmentSlot::Bat));
    TestFalse(TEXT("Invalid role rejected"),C26Character::Allows(EC26VisualRole(255),EC26EquipmentSlot::Headwear));
    TestFalse(TEXT("No duplicate equipment ball"),C26Character::Allows(EC26VisualRole::Bowler,EC26EquipmentSlot::Ball));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CharacterMotionTest,"Cricket26.Characters.AuthoritativeMovement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26CharacterMotionTest::RunTest(const FString& Parameters)
{
    FC26LocomotionSample Sample;Sample.Reset(FTransform::Identity);
    Sample.Update(FTransform(FVector(50,0,0)),.1f);
    TestEqual(TEXT("Direct transform drives 5m/s locomotion"),Sample.GroundSpeed,500.f);
    Sample.Update(FTransform(FVector(500,0,0)),0.f);
    TestEqual(TEXT("Exact contact sample cannot advance distance"),Sample.Distance,50.f);
    Sample.Update(FTransform(FVector(50,0,0)),.1f);
    TestEqual(TEXT("Stop detected without action enum"),Sample.GroundSpeed,0.f);
    TestTrue(TEXT("Deceleration detected"),Sample.Acceleration.X<0);
    Sample.Update(FTransform(FVector(5000,0,0)),.1f);
    TestTrue(TEXT("Reset/teleport does not play extreme stride"),Sample.Teleported);
    TestEqual(TEXT("Teleport clears speed"),Sample.GroundSpeed,0.f);
    Sample.Reset(FTransform(FRotator(0,179,0)));
    Sample.Update(FTransform(FRotator(0,-179,0)),.1f);
    TestTrue(TEXT("Yaw wraps continuously"),FMath::IsNearlyEqual(Sample.TurnRate,20.f,.01f));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CharacterContractTest,"Cricket26.Characters.AssetAndEventGates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26CharacterContractTest::RunTest(const FString& Parameters)
{
    auto* Profile=NewObject<UC26CharacterProfile>();TArray<FString> Errors;
    TestFalse(TEXT("Empty/uncertified profile cannot migrate"),Profile->Validate(Errors));
    TestTrue(TEXT("Reports exact missing dependencies"),Errors.Num()>20);
    TestEqual(TEXT("Simulation event maps to authored contact"),C26Character::MapEventTime(.18f,.18f,.8f,1.5f),.8f);
    TestTrue(TEXT("Follow-through retains authored seconds"),FMath::IsNearlyEqual(C26Character::MapEventTime(.38f,.18f,.8f,1.5f),1.f));
    TestTrue(TEXT("Spin fallback does not become pace"),C26Character::BowlingKey(EC26Delivery::OffBreak,false)!=C26Character::BowlingKey(EC26Delivery::Pace,false));
    TestTrue(TEXT("Finger and wrist spin have distinct actions"),C26Character::BowlingKey(EC26Delivery::OffBreak,false)!=C26Character::BowlingKey(EC26Delivery::LegBreak,false));
    TestTrue(TEXT("Handed batting has distinct assets"),C26Character::ShotKey(TEXT("COVER DRIVE"),true)!=C26Character::ShotKey(TEXT("COVER DRIVE"),false));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26CharacterGraphTest,"Cricket26.Characters.NativeSkeletalGraph",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26CharacterGraphTest::RunTest(const FString& Parameters)
{
    // Existing run is a TEST FIXTURE for node evaluation, not an approved cricket replacement.
    auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Animations/A_Run.A_Run"));
    if(!TestNotNull(TEXT("Run fixture"),Clip))return false;
    USkeletalMesh* Mesh=Clip->GetSkeleton()->GetPreviewMesh(true);
    if(!TestNotNull(TEXT("Fixture skeleton preview mesh"),Mesh))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    AActor* Actor=World->SpawnActor<AActor>();
    auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);
    Body->SetSkeletalMesh(Mesh);Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->RegisterComponent();
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    bool Good=TestNotNull(TEXT("Native graph initialized"),Anim);
    if(Anim)
    {
        Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;
        Anim->CurrentTime=.05f;Body->TickAnimation(.016f,false);Body->RefreshBoneTransforms();
        const TArray<FTransform> Before=Body->GetComponentSpaceTransforms();
        Anim->CurrentTime=Clip->GetPlayLength()*.4f;Body->TickAnimation(.016f,false);Body->RefreshBoneTransforms();
        const auto& After=Body->GetComponentSpaceTransforms();int32 Changed=0;
        for(int32 I=0;I<FMath::Min(Before.Num(),After.Num());++I)if(!Before[I].Equals(After[I],.01f))++Changed;
        Good&=TestTrue(TEXT("Authored sequence moves multiple skeletal bones"),Changed>8);
        AddInfo(FString::Printf(TEXT("Native graph evaluated %d changing bones"),Changed));
    }
    Actor->Destroy();World->DestroyWorld(false);return Good;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26RetargetedRunTest,"Cricket26.Characters.RetargetedRun",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26RetargetedRunTest::RunTest(const FString& Parameters)
{
    auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run.C26_A_Run"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate.SK_C26_FullBody_Candidate"));
    if(!TestNotNull(TEXT("New canonical body"),Mesh)||!TestNotNull(TEXT("Retargeted run"),Clip))return false;
    TArray<FString> Errors;UC26CharacterProfile::AuditBody(Mesh,Clip->GetSkeleton(),Errors);
    for(const auto& Error:Errors)AddError(Error);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);AActor* Actor=World->SpawnActor<AActor>();
    auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);
    Body->SetSkeletalMesh(Mesh);Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->RegisterComponent();
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    bool Good=TestNotNull(TEXT("Canonical body uses real skeletal graph"),Anim);
    FVector LastL,LastR;float FootTravel=0,LowestFoot=1000;
    if(Anim)for(int32 I=0;I<12;++I)
    {
        Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;
        Anim->CurrentTime=Clip->GetPlayLength()*I/12.f;Body->TickAnimation(.016f,false);Body->RefreshBoneTransforms();
        const FVector Head=Body->GetBoneLocation(TEXT("head")),L=Body->GetBoneLocation(TEXT("foot_l")),R=Body->GetBoneLocation(TEXT("foot_r"));
        Good&=TestTrue(TEXT("Pelvis-root source cannot push human below floor"),Head.Z>130&&Head.Z<195);
        Good&=TestTrue(TEXT("In-place root cannot drift with source motion"),Body->GetBoneLocation(TEXT("root")).Size()<.1f);
        if(I)FootTravel+=(L-LastL).Size()+(R-LastR).Size();LastL=L;LastR=R;
        LowestFoot=FMath::Min3(LowestFoot,float(L.Z),float(R.Z));
    }
    Good&=TestTrue(TEXT("Both legs stride through the cycle"),FootTravel>200);
    Good&=TestTrue(TEXT("Planted ankle remains near ground"),LowestFoot>-4&&LowestFoot<20);
    AddInfo(FString::Printf(TEXT("New body: cumulative feet travel %.1fcm, lowest ankle %.1fcm; visual quality still requires review"),FootTravel,LowestFoot));
    Actor->Destroy();World->DestroyWorld(false);return Good&&Errors.IsEmpty();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26FootSolverTest,"Cricket26.Characters.FootSolver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26FootSolverTest::RunTest(const FString& Parameters)
{
    auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run.C26_A_Run"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate.SK_C26_FullBody_Candidate"));
    if(!TestNotNull(TEXT("Canonical body"),Mesh)||!TestNotNull(TEXT("Locomotion clip"),Clip))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);AActor* Actor=World->SpawnActor<AActor>();
    auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);
    Body->SetSkeletalMesh(Mesh);Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->RegisterComponent();
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    bool Good=TestNotNull(TEXT("AnimInstance active"),Anim);
    if(Anim)
    {
        auto Pose=[&](float Time)
        {
            Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;Anim->CurrentTime=Time;
            Body->TickAnimation(.016f,false);Body->RefreshBoneTransforms();
        };
        auto CS=[&](const TCHAR* Bone){return Body->GetBoneLocation(Bone,EBoneSpaces::ComponentSpace);};

        // Baseline: the solver must be completely inert while it carries no weight.
        Anim->FootIKWeight=Anim->LeftFootLockAlpha=Anim->RightFootLockAlpha=Anim->PelvisOffsetZ=0.f;
        Pose(.1f);
        const FVector RestFoot=CS(TEXT("foot_l")),RestPelvis=CS(TEXT("pelvis")),RestThigh=CS(TEXT("thigh_l"));
        const float ThighLen=(CS(TEXT("calf_l"))-RestThigh).Size(),CalfLen=(RestFoot-CS(TEXT("calf_l"))).Size();
        Anim->FootIKWeight=1.f;
        Pose(.1f);
        Good&=TestTrue(TEXT("Zero lock weight leaves the authored pose untouched"),CS(TEXT("foot_l")).Equals(RestFoot,.01f));

        // Pelvis compensation is a subtle downward adjustment, not a crouch.
        Anim->PelvisOffsetZ=-4.f;Pose(.1f);
        Good&=TestTrue(TEXT("Pelvis compensation lowers the hips by the requested amount"),
            FMath::IsNearlyEqual(float(CS(TEXT("pelvis")).Z),float(RestPelvis.Z)-4.f,.5f));
        Anim->PelvisOffsetZ=0.f;

        // A full-weight lock must actually reach a reachable mark.
        Anim->LeftFootLockAlpha=1.f;
        Anim->LeftFootTargetCS=RestFoot+FVector(4.f,0,-3.f);
        Pose(.1f);
        Good&=TestTrue(TEXT("Full lock places the ankle on its mark"),CS(TEXT("foot_l")).Equals(Anim->LeftFootTargetCS,1.5f));
        Good&=TestTrue(TEXT("Locking does not stretch the thigh"),FMath::IsNearlyEqual(float((CS(TEXT("calf_l"))-CS(TEXT("thigh_l"))).Size()),ThighLen,.5f));
        Good&=TestTrue(TEXT("Locking does not stretch the calf"),FMath::IsNearlyEqual(float((CS(TEXT("foot_l"))-CS(TEXT("calf_l"))).Size()),CalfLen,.5f));

        // An unreachable mark must fail by falling short, never by pulling the leg apart.
        Anim->LeftFootTargetCS=RestFoot+FVector(0,0,-400.f);
        Pose(.1f);
        const float Reach=(CS(TEXT("foot_l"))-CS(TEXT("thigh_l"))).Size();
        Good&=TestTrue(TEXT("Unreachable mark clamps to leg length instead of stretching"),Reach<=ThighLen+CalfLen+.5f);

        // Partial weight is a partial correction: this is what stops feet snapping on and off marks.
        Anim->LeftFootTargetCS=RestFoot+FVector(6.f,0,0);
        Anim->LeftFootLockAlpha=.5f;Pose(.1f);
        const float Half=(CS(TEXT("foot_l"))-RestFoot).Size();
        Anim->LeftFootLockAlpha=1.f;Pose(.1f);
        const float Full=(CS(TEXT("foot_l"))-RestFoot).Size();
        Good&=TestTrue(TEXT("Half weight moves the ankle roughly half as far"),Half>.5f&&Half<Full*.8f);
        AddInfo(FString::Printf(TEXT("Solver: half-weight %.2fcm vs full-weight %.2fcm; leg %.1f+%.1fcm"),Half,Full,ThighLen,CalfLen));
    }
    Actor->Destroy();World->DestroyWorld(false);return Good;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26FootSkateTest,"Cricket26.Characters.FootSkate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26FootSkateTest::RunTest(const FString& Parameters)
{
    // Measures the thing the work claims to fix: how far a foot that is carrying weight travels
    // across the ground while it is planted, and how large its worst single-frame step is. The
    // in-place run clip is played back while the actor is translated at the clip's own authored
    // stride speed, so any residual world-space ankle motion during stance is skate. BOTH feet are
    // measured: a lock that fixed one leg and snapped the other would pass a single-foot test.
    auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run.C26_A_Run"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate.SK_C26_FullBody_Candidate"));
    if(!TestNotNull(TEXT("Canonical body"),Mesh)||!TestNotNull(TEXT("Locomotion clip"),Clip))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);AActor* Actor=World->SpawnActor<AActor>();
    auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);
    Body->SetSkeletalMesh(Mesh);Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->RegisterComponent();
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    if(!TestNotNull(TEXT("AnimInstance active"),Anim)){Actor->Destroy();World->DestroyWorld(false);return false;}

    const int32 Steps=90;const float Length=Clip->GetPlayLength();const float Dt=Length/Steps;
    const TCHAR* Bones[]={TEXT("foot_l"),TEXT("foot_r")};
    auto Sample=[&](int32 Step,int32 Foot,bool bLock,const FVector& LockWorld,float Alpha)
    {
        Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;Anim->CurrentTime=Length*Step/Steps;
        Anim->FootIKWeight=bLock?1.f:0.f;Anim->PelvisOffsetZ=0.f;
        Anim->LeftFootLockAlpha=Foot==0&&bLock?Alpha:0.f;
        Anim->RightFootLockAlpha=Foot==1&&bLock?Alpha:0.f;
        if(bLock)
        {
            const FVector TargetCS=Body->GetComponentTransform().InverseTransformPosition(LockWorld);
            if(Foot==0)Anim->LeftFootTargetCS=TargetCS;else Anim->RightFootTargetCS=TargetCS;
        }
        Body->TickAnimation(Dt,false);Body->RefreshBoneTransforms();
    };

    // Pass 1: recover each foot's authored stance window and stride speed from its own
    // component-space ankle motion while the clip plays in place.
    struct FFootTrace{TArray<FVector> CS;float Ceiling=0.f;FVector PerFrame=FVector::ZeroVector;float Dragged=0.f;int32 StanceFrames=0;};
    FFootTrace Trace[2];
    Actor->SetActorLocation(FVector::ZeroVector);
    for(int32 Foot=0;Foot<2;++Foot)
    {
        float SoleZ=BIG_NUMBER;
        for(int32 Step=0;Step<=Steps;++Step)
        {
            Sample(Step,Foot,false,FVector::ZeroVector,0.f);
            const FVector A=Body->GetBoneLocation(Bones[Foot],EBoneSpaces::ComponentSpace);
            Trace[Foot].CS.Add(A);SoleZ=FMath::Min(SoleZ,float(A.Z));
        }
        Trace[Foot].Ceiling=SoleZ+3.f;
        FVector Carry=FVector::ZeroVector;
        for(int32 Step=1;Step<Trace[Foot].CS.Num();++Step)
            if(Trace[Foot].CS[Step].Z<Trace[Foot].Ceiling&&Trace[Foot].CS[Step-1].Z<Trace[Foot].Ceiling)
            {Carry+=Trace[Foot].CS[Step]-Trace[Foot].CS[Step-1];++Trace[Foot].StanceFrames;}
        Trace[Foot].Dragged=Carry.Size();
        Trace[Foot].PerFrame=-Carry/FMath::Max(1,Trace[Foot].StanceFrames);
    }
    bool Good=TestTrue(TEXT("Both feet have a measurable stance phase"),
        Trace[0].StanceFrames>4&&Trace[1].StanceFrames>4);
    // Report the raw measurement before judging it, so a failure here is diagnosable from the log
    // alone instead of requiring another instrumented build.
    const float SpeedL=Trace[0].PerFrame.Size()/Dt,SpeedR=Trace[1].PerFrame.Size()/Dt;
    for(int32 Foot=0;Foot<2;++Foot)
        AddInfo(FString::Printf(TEXT("Authored %s stance: %d frames, lowest ankle Z %.2fcm, dragged %.2fcm -> %.0f cm/s"),
            Foot==0?TEXT("left"):TEXT("right"),Trace[Foot].StanceFrames,
            double(Trace[Foot].Ceiling-3.f),double(Trace[Foot].Dragged),double(Foot==0?SpeedL:SpeedR)));
    // The invariant that makes this clip a usable run cycle is how far the ground moves under each
    // foot per stance, NOT the rate: a foot that is only briefly in contact is dragged the same
    // distance in fewer frames and so reports a higher cm/s. Asserting the rate would fail on a
    // perfectly good clip whose two contacts differ in sharpness, so the distance is asserted
    // tightly and the rate only for gross sanity.
    const float Short=FMath::Min(Trace[0].Dragged,Trace[1].Dragged);
    const float Long=FMath::Max(Trace[0].Dragged,Trace[1].Dragged);
    Good&=TestTrue(FString::Printf(TEXT("Both feet cover the same ground per stance (%.2f vs %.2fcm)"),
        double(Trace[0].Dragged),double(Trace[1].Dragged)),Short>.8f*Long);
    const float Slower=FMath::Min(SpeedL,SpeedR),Faster=FMath::Max(SpeedL,SpeedR);
    Good&=TestTrue(FString::Printf(TEXT("Both feet are dragged at a plausible stride rate (%.0f vs %.0f cm/s)"),SpeedL,SpeedR),
        Slower>150.f&&Faster<900.f&&Slower>.33f*Faster);
    if(!Good){Actor->Destroy();World->DestroyWorld(false);return false;}
    // Each foot is played back at its OWN authored stance speed. The body only travels at one
    // speed, so the two feet are measured independently and each is judged against the drag it
    // actually has; using a shared average would silently handicap the faster foot's test.

    struct FSkate{float Travel=0.f,MaxStep=0.f;int32 Frames=0;};
    auto Measure=[&](int32 Foot,bool bLock)
    {
        // The body travels opposite to the way the planted ankle is dragged in component space,
        // at the rate that foot's own stance phase says the ground is passing under it.
        const FVector ActorSpeed=Trace[Foot].PerFrame;
        FSkate Out;bool bWasStance=false;FVector Mark=FVector::ZeroVector,Last=FVector::ZeroVector;float Alpha=0.f;
        for(int32 Step=0;Step<=Steps;++Step)
        {
            // Position the body BEFORE taking the mark, so the mark really is the world point the
            // ankle lands on in the frame it lands.
            Actor->SetActorLocation(ActorSpeed*Step);
            // The lock mark and its weight are driven exactly as the presentation component drives
            // them: take a mark on touchdown, ramp the weight, release on lift-off.
            const bool bStance=Trace[Foot].CS[Step].Z<Trace[Foot].Ceiling;
            if(bStance&&!bWasStance){Mark=Body->GetComponentTransform().TransformPosition(Trace[Foot].CS[Step]);Alpha=0.f;}
            if(bStance)Alpha=FMath::FInterpTo(Alpha,1.f,Dt,18.f);else Alpha=FMath::FInterpTo(Alpha,0.f,Dt,24.f);
            Sample(Step,Foot,bLock,Mark,Alpha);
            const FVector Ankle=Body->GetBoneLocation(Bones[Foot]);
            if(bStance&&bWasStance)
            {
                // Only horizontal travel counts; a heel rolling up off the ground is correct.
                const float Step2D=FVector::Dist2D(Ankle,Last);
                Out.Travel+=Step2D;Out.MaxStep=FMath::Max(Out.MaxStep,Step2D);++Out.Frames;
            }
            Last=Ankle;bWasStance=bStance;
        }
        return Out;
    };
    for(int32 Foot=0;Foot<2;++Foot)
    {
        const FSkate Before=Measure(Foot,false);
        const FSkate After=Measure(Foot,true);
        const FString Name=Foot==0?TEXT("left"):TEXT("right");
        AddInfo(FString::Printf(TEXT("Planted %s foot per stride: %.2fcm -> %.2fcm locked; worst single frame %.2fcm -> %.2fcm (%d stance frames)"),
            *Name,Before.Travel,After.Travel,Before.MaxStep,After.MaxStep,Before.Frames));
        Good&=TestTrue(FString::Printf(TEXT("Foot locking reduces planted-%s-foot travel"),*Name),
            After.Travel<Before.Travel);
        Good&=TestTrue(FString::Printf(TEXT("Locked %s planted foot is close to stationary"),*Name),
            After.Travel<FMath::Max(2.f,Before.Travel*.5f));
        // The success criterion is "no new foot snapping": locking may not introduce a larger
        // single-frame step than the authored stride already has.
        Good&=TestTrue(FString::Printf(TEXT("Locking adds no new %s foot snapping"),*Name),
            After.MaxStep<=Before.MaxStep+.5f);
    }
    Actor->Destroy();World->DestroyWorld(false);return Good;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26LocomotionTransitionTest,"Cricket26.Characters.LocomotionTransitions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26LocomotionTransitionTest::RunTest(const FString& Parameters)
{
    // PART B. These rules live inside SelectState, which needs a live athlete, so they are asserted
    // through the same pure functions SelectState calls. This is the idle<->movement contract.
    TestFalse(TEXT("A stationary athlete does not start moving inside the band"),C26Presentation::WantsMove(false,15.f));
    TestTrue(TEXT("The enter threshold is crossed at a real walking speed"),C26Presentation::WantsMove(false,23.f));
    TestTrue(TEXT("An athlete already moving keeps moving inside the band"),C26Presentation::WantsMove(true,15.f));
    TestFalse(TEXT("A moving athlete stops only below the leave threshold"),C26Presentation::WantsMove(true,9.f));
    // The band is the whole point: no single speed may answer both ways, or the athlete flaps and
    // restarts Start/Stop every few frames.
    int32 Band=0;
    for(float Speed=C26Presentation::StopSpeed+.5f;Speed<C26Presentation::StartSpeed;Speed+=1.f)
    {
        ++Band;
        TestTrue(TEXT("Every speed inside the band holds the state the athlete is already in"),
            C26Presentation::WantsMove(false,Speed)!=C26Presentation::WantsMove(true,Speed));
    }
    TestTrue(TEXT("The hysteresis band is wide enough to be worth having"),Band>=8);

    // Scope boundary. Only the shared locomotion and stance clips may be stabilized; the batting,
    // bowling and fielding action clips another agent owns carry authored footwork, and pinning a
    // foot through a front-foot stride would drag it. This is the guarantee that the foundation
    // never fights their work.
    for(const TCHAR* Shared:{TEXT("Walk"),TEXT("Run"),TEXT("Start"),TEXT("Stop"),TEXT("TurnLeft"),
        TEXT("TurnRight"),TEXT("FielderReady"),TEXT("KeeperReady"),TEXT("BowlerReady"),
        TEXT("UmpireReady"),TEXT("NonStrikerReady"),TEXT("BatterRun_L"),TEXT("BatterRun_R"),
        TEXT("KeeperShuffle"),TEXT("UmpireWalk")})
        TestTrue(TEXT("Shared locomotion and stance clips may take a foot mark"),
            UC26CharacterPresentationComponent::StateAllowsFootLock(FName(Shared)));
    for(const TCHAR* Owned:{TEXT("Pickup"),TEXT("Throw"),TEXT("Catch"),TEXT("KeeperReceive"),
        TEXT("Celebrate"),TEXT("Disappointed"),TEXT("SignalFour"),TEXT("SignalSix"),
        TEXT("SignalOut"),TEXT("SignalWide"),TEXT("COVERDRIVE_R"),TEXT("PULL_L"),
        TEXT("FRONTFOOTDEFENCE_R"),TEXT("FastBowl_R"),TEXT("LegSpin_L"),TEXT("OffSpin_R")})
        TestFalse(TEXT("Action clips owned by other work are never foot-locked"),
            UC26CharacterPresentationComponent::StateAllowsFootLock(FName(Owned)));

    // Transition exits. A Start clip held at full pace, or a Stop clip held once stationary, is a
    // stride the ground no longer justifies.
    TestFalse(TEXT("Start is not cut before the body has begun to move"),
        C26Presentation::TransitionSpent(TEXT("Start"),300.f,.10f));
    TestFalse(TEXT("Start still plays while the athlete is not yet at pace"),
        C26Presentation::TransitionSpent(TEXT("Start"),100.f,.50f));
    TestTrue(TEXT("Start hands back once the athlete is at pace"),
        C26Presentation::TransitionSpent(TEXT("Start"),250.f,.30f));
    TestFalse(TEXT("Stop is not cut the instant the athlete halts"),
        C26Presentation::TransitionSpent(TEXT("Stop"),2.f,.10f));
    TestFalse(TEXT("Stop still plays while the athlete is still coasting"),
        C26Presentation::TransitionSpent(TEXT("Stop"),40.f,.50f));
    TestTrue(TEXT("Stop hands back once the athlete is settled"),
        C26Presentation::TransitionSpent(TEXT("Stop"),2.f,.30f));
    TestFalse(TEXT("Turn clips always play out"),C26Presentation::TransitionSpent(TEXT("TurnLeft"),300.f,.90f));
    TestFalse(TEXT("An ordinary locomotion clip is never treated as a transition"),
        C26Presentation::TransitionSpent(TEXT("Run"),300.f,.90f));

    // PART C. A re-aim inside one frame is a snap; a turn the athlete could have run through is not.
    TestTrue(TEXT("A plausible turn is left to the animation"),
        FMath::IsNearlyEqual(C26Presentation::StepMeshYawOffset(0.f,5.f,.016f),0.f,.001f));
    const float Snapped=C26Presentation::StepMeshYawOffset(0.f,90.f,.016f);
    // The call absorbs the snap and then takes the first unwind step, so it lands short of a full
    // 90. Asserting the FRACTION taken keeps this independent of the filter's exact step size.
    const float SnapTaken=FMath::Abs(Snapped)/90.f;
    TestTrue(TEXT("A 90 degree re-aim is mostly absorbed on the first frame"),SnapTaken>=.75f&&SnapTaken<=1.f);
    TestTrue(TEXT("The absorbed lag opposes the authoritative rotation, not doubles it"),Snapped<0.f);
    const float Flipped=C26Presentation::StepMeshYawOffset(0.f,180.f,.016f);
    TestTrue(TEXT("A 180 degree re-aim cannot spin the body past the lag clamp"),
        FMath::Abs(Flipped)<=C26Presentation::MaxMeshYawLag+.01f);
    // The lag must unwind, or the mesh would stay permanently mis-aimed against its own actor.
    float Lag=Snapped;int32 Frames=0;
    while(Frames<120&&FMath::Abs(Lag)>.5f){Lag=C26Presentation::StepMeshYawOffset(Lag,0.f,.016f);++Frames;}
    TestTrue(TEXT("Mesh yaw lag unwinds back onto the actor"),FMath::Abs(Lag)<=.5f);
    TestTrue(TEXT("The unwind is quick enough to read as a turn, not a drift"),Frames<60);
    AddInfo(FString::Printf(TEXT("Transitions: %d speeds inside the band; a 90deg re-aim absorbs to %.1fdeg (%.0f%%) in one frame and unwinds to <0.5deg in %d frames (%.0fms)"),
        Band,Snapped,SnapTaken*100.f,Frames,Frames*16.f));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26ActionRecoveryTest,"Cricket26.Characters.ActionRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26ActionRecoveryTest::RunTest(const FString& Parameters)
{
    // PART B, the third case the brief names: an action hands back to a ready stance. The failure
    // mode is not a blend that is too long, it is a FROZEN outgoing pose - the action stops dead
    // at the switch frame and the body visibly stalls for the whole blend. Both clips are real
    // assets from this worktree, so the play lengths and the wrap are the authored ones.
    auto* Action=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Cricket/A_C26_FastBowl_R.A_C26_FastBowl_R"));
    auto* Ready=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Cricket/A_C26_FielderReady.A_C26_FielderReady"));
    if(!TestNotNull(TEXT("Bowling action clip"),Action)||!TestNotNull(TEXT("Ready stance clip"),Ready))return false;
    const float ActionLength=Action->GetPlayLength();
    TestTrue(TEXT("The action clip has a usable length"),ActionLength>.05f);

    // Replay the hand-back the way the component drives it: the outgoing pose advanced every frame
    // while the blend runs. Starting two frames from the end of the action forces the clip to run
    // out mid-blend, which is the normal case for a delivery handing back to ready and the case a
    // naive "clamp at the last frame" implementation would freeze on.
    const float BlendSeconds=.12f,Dt=1.f/60.f;
    float PreviousTime=FMath::Max(0.f,ActionLength-Dt*2.f),BlendClock=0.f,PeakStep=0.f;
    int32 Frames=0,Advanced=0,Frozen=0,Wraps=0;
    while(BlendClock<BlendSeconds&&Frames<600)
    {
        const bool bWrapped=PreviousTime+Dt>=ActionLength;
        const float Next=C26Presentation::AdvanceOutgoingPose(PreviousTime,ActionLength,Dt);
        const float Step=FMath::Abs(Next-PreviousTime);
        if(bWrapped)++Wraps;else PeakStep=FMath::Max(PeakStep,Step);
        if(Step>.001f)++Advanced;else ++Frozen;
        TestTrue(TEXT("The outgoing pose stays inside the clip"),Next>=0.f&&Next<ActionLength);
        PreviousTime=Next;BlendClock+=Dt;++Frames;
    }
    // This is the regression guard for the defect: freezing PreviousTime made every frame of the
    // blend a repeat of the switch pose, which is the stall the brief calls a frozen pause.
    TestEqual(TEXT("The outgoing pose never repeats a frame during the blend"),Frozen,0);
    TestEqual(TEXT("Every frame of the blend advances the outgoing pose"),Advanced,Frames);
    TestTrue(TEXT("The blend outlasts the action clip, so the wrap case is genuinely covered"),Wraps>=1);
    TestTrue(TEXT("The blend completes inside its authored duration"),
        BlendClock>=BlendSeconds&&Frames<=FMath::CeilToInt(BlendSeconds/Dt)+1);
    TestTrue(TEXT("The outgoing pose advances by exactly one frame of real time"),PeakStep<=Dt+.001f);
    AddInfo(FString::Printf(TEXT("Action hand-back: %s (%.2fs, from %d frames before the end) -> ready over %.2fs; outgoing pose advanced on %d/%d frames, %d wrap, peak step %.3fs"),
        *Action->GetName(),double(ActionLength),2,double(BlendSeconds),Advanced,Frames,Wraps,double(PeakStep)));

    // The incoming clip must not be visible before the blend, must be fully in at the end, and
    // must leave and arrive with ~zero velocity. A linear ramp pops at both ends.
    TestTrue(TEXT("The incoming clip is not visible at the switch frame"),C26Presentation::BlendWeight(0.f,BlendSeconds)<.01f);
    TestTrue(TEXT("The incoming clip is fully in exactly at the end of the blend"),
        C26Presentation::BlendWeight(BlendSeconds,BlendSeconds)>=.999f);
    float Previous=C26Presentation::BlendWeight(0.f,BlendSeconds),FirstTenth=0.f,MidTenth=0.f;
    for(int32 Step=1;Step<=10;++Step)
    {
        const float Weight=C26Presentation::BlendWeight(BlendSeconds*Step/10.f,BlendSeconds);
        const float Delta=Weight-Previous;
        TestTrue(TEXT("Blend weight never runs backwards"),Delta>=-.0001f);
        TestTrue(TEXT("Blend weight stays inside 0..1"),Weight>=-.0001f&&Weight<=1.0001f);
        if(Step==1)FirstTenth=Delta;
        if(Step==6)MidTenth=Delta;
        Previous=Weight;
    }
    // Smoothstep: the first tenth of the blend covers far less ground than the middle, which is
    // exactly what makes the switch inaudible instead of a step.
    TestTrue(TEXT("The blend eases in rather than stepping"),FirstTenth<MidTenth*.6f);
    AddInfo(FString::Printf(TEXT("Blend weight: first tenth moves %.3f, middle tenth %.3f (ease-in ratio %.2f)"),
        double(FirstTenth),double(MidTenth),double(MidTenth>0.f?FirstTenth/MidTenth:0.f)));
    // A degenerate blend length must not divide by zero or hand back a NaN weight.
    TestTrue(TEXT("A zero-length blend still resolves to a usable weight"),
        FMath::IsFinite(C26Presentation::BlendWeight(0.f,0.f))&&FMath::IsFinite(C26Presentation::BlendWeight(.5f,0.f)));
    return true;
}
#endif
