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
#endif
