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
    // across the ground while it is planted. The in-place run clip is played back while the actor
    // is translated at the clip's own authored stride speed, so any residual world-space ankle
    // motion during stance is skate.
    auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Cricket26/Characters/Animations/Locomotion/C26_A_Run.C26_A_Run"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Cricket26/Characters/Bodies/SK_C26_FullBody_Candidate.SK_C26_FullBody_Candidate"));
    if(!TestNotNull(TEXT("Canonical body"),Mesh)||!TestNotNull(TEXT("Locomotion clip"),Clip))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);AActor* Actor=World->SpawnActor<AActor>();
    auto* Body=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Body);
    Body->SetSkeletalMesh(Mesh);Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());Body->RegisterComponent();
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    if(!TestNotNull(TEXT("AnimInstance active"),Anim)){Actor->Destroy();World->DestroyWorld(false);return false;}

    const int32 Steps=90;const float Length=Clip->GetPlayLength();const float Dt=Length/Steps;
    auto Sample=[&](int32 Step,const FVector& ActorPos,bool bLock,const FVector& LockWorld,float Alpha)
    {
        Actor->SetActorLocation(ActorPos);
        Anim->PreviousSequence=Anim->CurrentSequence=Clip;Anim->BlendAlpha=1;Anim->CurrentTime=Length*Step/Steps;
        Anim->FootIKWeight=bLock?1.f:0.f;
        Anim->LeftFootLockAlpha=bLock?Alpha:0.f;Anim->RightFootLockAlpha=0.f;Anim->PelvisOffsetZ=0.f;
        if(bLock)Anim->LeftFootTargetCS=Body->GetComponentTransform().InverseTransformPosition(LockWorld);
        Body->TickAnimation(Dt,false);Body->RefreshBoneTransforms();
    };

    // Pass 1: recover the clip's authored stride speed from the stance-phase ankle velocity,
    // then measure unassisted skate at that speed.
    float SoleZ=BIG_NUMBER;TArray<FVector> AnkleCS;
    for(int32 Step=0;Step<=Steps;++Step)
    {
        Sample(Step,FVector::ZeroVector,false,FVector::ZeroVector,0.f);
        const FVector A=Body->GetBoneLocation(TEXT("foot_l"),EBoneSpaces::ComponentSpace);
        AnkleCS.Add(A);SoleZ=FMath::Min(SoleZ,float(A.Z));
    }
    const float StanceCeiling=SoleZ+3.f;
    FVector Carry=FVector::ZeroVector;int32 StanceFrames=0;
    for(int32 Step=1;Step<AnkleCS.Num();++Step)
        if(AnkleCS[Step].Z<StanceCeiling&&AnkleCS[Step-1].Z<StanceCeiling)
        {Carry+=AnkleCS[Step]-AnkleCS[Step-1];++StanceFrames;}
    bool Good=TestTrue(TEXT("Clip has a measurable stance phase"),StanceFrames>4);
    if(!Good){Actor->Destroy();World->DestroyWorld(false);return false;}
    // The body travels opposite to the way the planted ankle is dragged in component space.
    const FVector PerFrame=-Carry/StanceFrames;
    AddInfo(FString::Printf(TEXT("Authored stride: %.0f cm/s over %d stance frames"),PerFrame.Size()/Dt,StanceFrames));

    auto MeasureSkate=[&](bool bLock)
    {
        float Skate=0.f;int32 Frames=0;bool bWasStance=false;
        FVector Mark=FVector::ZeroVector,LastAnkle=FVector::ZeroVector;float Alpha=0.f;
        for(int32 Step=0;Step<=Steps;++Step)
        {
            const FVector ActorPos=PerFrame*Step;
            // The lock mark and its weight are driven exactly as the presentation component
            // drives them: take a mark on touchdown, ramp the weight, release on lift-off.
            const bool bStance=AnkleCS[Step].Z<StanceCeiling;
            if(bStance&&!bWasStance){Mark=Body->GetComponentTransform().TransformPosition(AnkleCS[Step]);Alpha=0.f;}
            if(bStance)Alpha=FMath::FInterpTo(Alpha,1.f,Dt,18.f);else Alpha=FMath::FInterpTo(Alpha,0.f,Dt,24.f);
            Sample(Step,ActorPos,bLock,Mark,Alpha);
            const FVector Ankle=Body->GetBoneLocation(TEXT("foot_l"));
            if(bStance&&bWasStance)
            {
                // Only horizontal travel counts; a heel rolling up off the ground is correct.
                Skate+=FVector::Dist2D(Ankle,LastAnkle);++Frames;
            }
            LastAnkle=Ankle;bWasStance=bStance;
        }
        return TPair<float,int32>(Skate,Frames);
    };
    const auto Before=MeasureSkate(false);
    const auto After=MeasureSkate(true);
    AddInfo(FString::Printf(TEXT("Planted-foot travel over one stride: %.2fcm unassisted -> %.2fcm locked (%d stance frames)"),
        Before.Key,After.Key,Before.Value));
    Good&=TestTrue(TEXT("Foot locking reduces planted-foot travel"),After.Key<Before.Key);
    Good&=TestTrue(TEXT("Locked planted foot is close to stationary"),After.Key<FMath::Max(2.f,Before.Key*.5f));
    Actor->Destroy();World->DestroyWorld(false);return Good;
}
#endif
