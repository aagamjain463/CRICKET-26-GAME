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
#include "Materials/Material.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FC26VisualConfigurationTest,"Cricket26.Characters.VisualConfiguration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FC26VisualConfigurationTest::RunTest(const FString& Parameters)
{
    auto* Profile=NewObject<UC26CharacterProfile>();
    Profile->Body=NewObject<USkeletalMesh>();Profile->UmpireBody=NewObject<USkeletalMesh>();
    auto* Variant=NewObject<USkeletalMesh>();Profile->BodyPresets.Add(TEXT("Tall"),Variant);
    TestTrue(TEXT("Unknown preset uses existing body"),Profile->ResolveBody(EC26VisualRole::Batter,TEXT("Unknown"))==Profile->Body);
    TestTrue(TEXT("Named preset selects only the visual body"),Profile->ResolveBody(EC26VisualRole::Fielder,TEXT("Tall"))==Variant);
    TestTrue(TEXT("Umpire body takes precedence over player preset"),Profile->ResolveBody(EC26VisualRole::Umpire,TEXT("Tall"))==Profile->UmpireBody);

    FC26EquipmentDefinition Item;Item.Socket=TEXT("Grip_R");Item.Offset.SetLocation(FVector(1,2,3));
    Item.LeftHandedOffset.SetLocation(FVector(4,5,6));
    TestEqual(TEXT("Missing left socket retains right socket"),Item.ResolveSocket(true),Item.Socket);
    TestTrue(TEXT("Missing left socket also retains right offset"),Item.ResolveOffset(true).Equals(Item.Offset));
    Item.LeftHandedSocket=TEXT("Grip_L");
    TestEqual(TEXT("Left socket selected"),Item.ResolveSocket(true),Item.LeftHandedSocket);
    TestTrue(TEXT("Left offset selected with left socket"),Item.ResolveOffset(true).Equals(Item.LeftHandedOffset));
    TestTrue(TEXT("Right hand retains original offset"),Item.ResolveOffset(false).Equals(Item.Offset));

    TArray<FString> Errors;
    TestTrue(TEXT("Empty material overrides preserve existing profiles"),Profile->ValidateMaterialOverrides(Errors));
    const FName Slot=TEXT("HeadSurface");
    auto* Material=UMaterial::GetDefaultMaterial(MD_Surface);
    for(auto* Model:{Profile->Body.Get(),Profile->UmpireBody.Get(),Variant})
        Model->GetMaterials().Add(FSkeletalMaterial(Material,true,false,Slot));
    Profile->MaterialOverrides.Add(Slot,Material);
    TestTrue(TEXT("Named head/body material slot accepted on every body"),Profile->ValidateMaterialOverrides(Errors));
    Variant->GetMaterials().Reset();
    TestFalse(TEXT("Preset missing named material slot rejected"),Profile->ValidateMaterialOverrides(Errors));
    Errors.Reset();Profile->MaterialOverrides[Slot]=nullptr;
    TestFalse(TEXT("Null override rejected"),Profile->ValidateMaterialOverrides(Errors));
    return true;
}

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
#endif
