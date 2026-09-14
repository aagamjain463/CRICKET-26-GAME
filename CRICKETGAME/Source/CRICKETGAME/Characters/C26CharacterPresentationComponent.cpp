#include "C26CharacterPresentationComponent.h"
#include "C26CricketerAnimInstance.h"
#include "SuperOver/C26Athlete.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> C26CharacterDebug(TEXT("c26.Character.Debug"),0,TEXT("1: role, state, speed, LOD, sockets and equipment"));
static TAutoConsoleVariable<int32> C26CharacterRole(TEXT("c26.Character.Role"),-1,TEXT("Visual role 0..5, -1 match role; affects selected number only"));
static TAutoConsoleVariable<int32> C26CharacterNumber(TEXT("c26.Character.Number"),7,TEXT("Squad number for development role preview"));
#endif
void FC26LocomotionSample::Reset(const FTransform& Transform)
{
    *this=FC26LocomotionSample();Initialized=true;Position=Transform.GetLocation();Yaw=Transform.Rotator().Yaw;
}
void FC26LocomotionSample::Update(const FTransform& Transform,float Dt)
{
    if(!Initialized){Reset(Transform);return;}if(Dt<=UE_SMALL_NUMBER)return;
    const FVector Next=Transform.GetLocation();const FVector Delta=Next-Position;
    Teleported=Delta.Size()>FMath::Max(120.f,1800.f*Dt);
    if(Teleported){Reset(Transform);Teleported=true;return;}
    const FVector Old=Velocity;Velocity=FVector(Delta.X,Delta.Y,0)/Dt;GroundSpeed=Velocity.Size();
    Acceleration=(Velocity-Old)/Dt;
    const FVector Local=Transform.InverseTransformVectorNoScale(Velocity);
    Direction=GroundSpeed>1.f?FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)):0.f;
    const float NewYaw=Transform.Rotator().Yaw;TurnRate=FMath::FindDeltaAngleDegrees(Yaw,NewYaw)/Dt;
    Distance+=Delta.Size2D();Position=Next;Yaw=NewYaw;
}
UC26CharacterPresentationComponent::UC26CharacterPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
bool UC26CharacterPresentationComponent::TryActivate(AC26Athlete* Athlete)
{
    if(bActive)return true;if(!Athlete)return false;
    FString Path=TEXT("/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer.DA_C26_DefaultPlayer");
    bool Slice=false;
    EC26VisualRole Role=EC26VisualRole::Fielder;
    switch(Athlete->Role)
    {
    case EC26Role::Batter:Role=Athlete->NonStriker?EC26VisualRole::NonStriker:EC26VisualRole::Batter;break;
    case EC26Role::Bowler:Role=EC26VisualRole::Bowler;break;
    case EC26Role::Keeper:Role=EC26VisualRole::Keeper;break;
    case EC26Role::Umpire:Role=EC26VisualRole::Umpire;break;
    default:break;
    }
#if !UE_BUILD_SHIPPING
    Slice=FParse::Param(FCommandLine::Get(),TEXT("C26CharacterSlice"));
    FParse::Value(FCommandLine::Get(),TEXT("C26CharacterProfile="),Path);
    if(Slice)
    {
        int32 ReviewRole=-1,ReviewNumber=-1;
        FParse::Value(FCommandLine::Get(),TEXT("C26CharacterSliceRole="),ReviewRole);
        FParse::Value(FCommandLine::Get(),TEXT("C26CharacterSliceNumber="),ReviewNumber);
        if(ReviewRole>=0&&ReviewRole!=int32(Role))return false;
        if(ReviewNumber>=0&&ReviewNumber!=Athlete->SquadNumber)return false;
        const bool Representative=Role==EC26VisualRole::Bowler||Role==EC26VisualRole::Keeper||Role==EC26VisualRole::Umpire
            ||(Role==EC26VisualRole::Fielder&&(ReviewNumber<0||Athlete->SquadNumber==ReviewNumber))
            ||(Role==EC26VisualRole::Batter&&Athlete->SquadNumber==7);
        if(!Representative)return false;
    }
#endif
    static TSet<FString> Rejected;
    const FString GateKey=Path+FString::Printf(TEXT(":%d:%d"),int32(Role),Slice);
    if(Rejected.Contains(GateKey))return false;
    Profile=LoadObject<UC26CharacterProfile>(nullptr,*Path);
    TArray<FString> Errors;
    if(!Profile)Errors.Add(TEXT("No complete approved character profile at ")+Path);
    else if(Slice)Errors=Profile->InspectRole(Role);
    else Profile->Validate(Errors,true);
    if(!Errors.IsEmpty())
    {
        if(!Rejected.Contains(GateKey))
        {
            Rejected.Add(GateKey);
            UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_MIGRATION_BLOCKED %s (%d errors). Existing match preserved."),*Path,Errors.Num());
            for(const FString& Error:Errors)UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_ASSET_GATE %s"),*Error);
        }
        return false;
    }
    Body=NewObject<USkeletalMeshComponent>(Athlete,TEXT("PremiumCricketerBody"));
    Athlete->AddInstanceComponent(Body);Body->SetupAttachment(Athlete->GetRootComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCastShadow(true);
    Body->SetVisibility(false);Body->SetHiddenInGame(true);
    Body->SetRelativeRotation(Profile->MeshToGameplayRotation);
    Body->SetSkeletalMesh(Profile->Body);Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());
    Body->bEnableUpdateRateOptimizations=false;
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Body->SetBoundsScale(1.5f);Body->RegisterComponent();Body->SetComponentTickEnabled(false);
    for(const auto& Item:Profile->Equipment)
    {
        auto* Part=NewObject<UStaticMeshComponent>(Athlete);
        Athlete->AddInstanceComponent(Part);Part->SetStaticMesh(Item.Mesh);
        Part->SetupAttachment(Body,Item.Socket);Part->SetRelativeTransform(Item.Offset);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetVisibility(false);Part->SetHiddenInGame(true);
        Part->RegisterComponent();Equipment.Add(Item.Slot,Part);
    }
    bActive=true;Configure(Athlete);ResetMotion();UpdateFromMatch(Athlete,0.f);
    if(!ValidateRuntime())
    {
        bActive=false;for(auto& Item:Equipment)Item.Value->DestroyComponent();Equipment.Empty();Body->DestroyComponent();Body=nullptr;return false;
    }
    HideLegacy(Athlete);Body->SetHiddenInGame(false);Body->SetVisibility(true);
#if !UE_BUILD_SHIPPING
    SetComponentTickEnabled(Slice&&FParse::Param(FCommandLine::Get(),TEXT("C26CharacterCapture")));
#endif
    UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_ACTIVE id=%s role=%d body=%s skeleton=%s anim=%s"),
        *Appearance.PlayerID.ToString(),int32(VisualRole),*Profile->Body->GetName(),*GetNameSafe(Profile->Skeleton),*GetNameSafe(Body->GetAnimClass()));
    return true;
}
void UC26CharacterPresentationComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(Dt,TickType,ThisTickFunction);
#if !UE_BUILD_SHIPPING
    if(!bActive||!Body)return;
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
    if(!ReviewCamera)ReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    if(!ReviewCamera)return;
    const FVector At=GetOwner()->GetActorLocation();
    const FVector Eye=At+GetOwner()->GetActorForwardVector()*430+GetOwner()->GetActorRightVector()*220+FVector(0,0,150);
    ReviewCamera->SetActorLocation(Eye);ReviewCamera->SetActorRotation((At+FVector(0,0,95)-Eye).Rotation());
    ReviewCamera->GetCameraComponent()->SetFieldOfView(37);
    PC->SetViewTarget(ReviewCamera);Body->SetForcedLOD(1);Body->UpdateLODStatus();
    ReviewTime+=Dt;
    int32& Count=ReviewSamples.FindOrAdd(CurrentState);
    if(ReviewTime>1&&ReviewTime-ReviewLastCapture>.16f&&Count<6)
    {
        FString Dir=FPaths::ProjectDir()/TEXT("Artifacts/CharacterAudit/MatchReview");
        FParse::Value(FCommandLine::Get(),TEXT("C26CharacterCaptureDir="),Dir);
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%s_%s_%d.png"),*Appearance.PlayerID.ToString(),*CurrentState.ToString(),Count),false,false);
        UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_MATCH_FRAME id=%s state=%s time=%.3f speed=%.1f left=%s right=%s lod=%d gear=%d"),
            *Appearance.PlayerID.ToString(),*CurrentState.ToString(),Clock,Locomotion.GroundSpeed,
            *Body->GetBoneLocation(Profile->LeftFootBone).ToString(),*Body->GetBoneLocation(Profile->RightFootBone).ToString(),Body->GetPredictedLODLevel(),Equipment.Num());
        ++Count;ReviewLastCapture=ReviewTime;
    }
#endif
}
void UC26CharacterPresentationComponent::HideLegacy(AC26Athlete* Athlete)
{
    const TArray<USceneComponent*> Parts={Athlete->Mesh,Athlete->HeroMesh,Athlete->Bat,Athlete->Headwear,Athlete->Grill,
        Athlete->PadL,Athlete->PadR,Athlete->GloveL,Athlete->GloveR,Athlete->ShoeL,Athlete->ShoeR,Athlete->Uniform,Athlete->Shade,Athlete->ShirtNumber};
    for(auto* Part:Parts)if(Part){Part->SetVisibility(false);Part->SetHiddenInGame(true);Part->SetComponentTickEnabled(false);}
}
void UC26CharacterPresentationComponent::Configure(AC26Athlete* Athlete)
{
    if(!bActive)return;
    const FName NewID=FName(*FString::Printf(TEXT("Team%d_Player%d"),Athlete->TeamId,Athlete->SquadNumber));
    Appearance.PlayerID=NewID;
    Appearance.JerseyNumber=Athlete->SquadNumber;
    Appearance.LeftHandedBat=Athlete->LeftHandedBat;
    Appearance.LeftArmBowl=Athlete->LeftArmBowl;
    EC26VisualRole Role=EC26VisualRole::Fielder;
    switch(Athlete->Role)
    {
    case EC26Role::Batter:Role=Athlete->NonStriker?EC26VisualRole::NonStriker:EC26VisualRole::Batter;break;
    case EC26Role::Bowler:Role=EC26VisualRole::Bowler;break;
    case EC26Role::Keeper:Role=EC26VisualRole::Keeper;break;
    case EC26Role::Umpire:Role=EC26VisualRole::Umpire;break;
    default:break;
    }
    ApplyVisualRole(Role);
    WarpPrevTime=0.f;
    if(Role!=EC26VisualRole::Umpire&&Profile->TeamMaterials.IsValidIndex(Athlete->TeamId))
    {
        const int32 Slot=Body->GetMaterialIndex(Profile->JerseyMaterialSlot);
        if(Slot>=0)Body->SetMaterial(Slot,Profile->TeamMaterials[Athlete->TeamId]);
    }
    DressEquipment(Athlete->TeamId);
}
int32 UC26CharacterPresentationComponent::Dress(UStaticMeshComponent* Part,const TCHAR* Key,UMaterialInstanceDynamic* M)
{
    int32 Count=0;
    if(!Part||!Part->GetStaticMesh()||!M)return Count;
    const TArray<FStaticMaterial>& Slots=Part->GetStaticMesh()->GetStaticMaterials();
    for(int I=0;I<Slots.Num();++I)
        if(Slots[I].MaterialSlotName.ToString().Contains(Key)){Part->SetMaterial(I,M);++Count;}
    if(Count==0)UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_DRESS_MISS part=%s key=%s slots=%d"),
        *GetNameSafe(Part->GetStaticMesh()),Key,Slots.Num());
    return Count;
}
void UC26CharacterPresentationComponent::DressEquipment(int32 TeamId)
{
    if(!bActive)return;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Surface.M_Surface"));
    if(!Base)return;
    auto* ClothMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Cloth.M_C26_Cloth"));
    auto* GearMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Gear.M_C26_Gear"));
    auto* ShellMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Shell.M_C26_Shell"));
    auto* Willow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Willow.M_Willow"));
    auto Make=[&](UMaterialInterface* From,FLinearColor C,float Rough)
    {
        auto* M=UMaterialInstanceDynamic::Create(From?From:Base,GetOwner());
        M->SetVectorParameterValue(TEXT("Tint"),C);
        M->SetScalarParameterValue(TEXT("Roughness"),Rough);
        M->SetScalarParameterValue(TEXT("Glow"),0.f);return M;
    };
    // Same slot keys and tones as the legacy kit dressing: equipment meshes are
    // shared originals, so a slot named Willow takes willow here exactly as there.
    const FLinearColor Kit=TeamId==0?FLinearColor(.030,.345,.395):FLinearColor(.660,.100,.058);
    auto Find=[this](EC26EquipmentSlot Slot){auto* P=Equipment.Find(Slot);return P?*P:nullptr;};
    if(auto Bat=Find(EC26EquipmentSlot::Bat))
    {
        Dress(Bat,TEXT("Willow"),Make(Willow,FLinearColor(.402,.330,.196),.42f));
        Dress(Bat,TEXT("Grip"),Make(GearMat,FLinearColor(.016,.018,.022),.88f));
        Dress(Bat,TEXT("Cane"),Make(GearMat,FLinearColor(.300,.222,.118),.56f));
        Dress(Bat,TEXT("Twine"),Make(GearMat,FLinearColor(.052,.046,.040),.80f));
        Dress(Bat,TEXT("Label"),Make(Base,Kit*1.15f+FLinearColor(.03,.03,.03),.34f));
    }
    if(auto Helmet=Find(EC26EquipmentSlot::Helmet))
    {
        Dress(Helmet,TEXT("Shell"),Make(ShellMat,Kit*.92f,.24f));
        Dress(Helmet,TEXT("Crown"),Make(ClothMat,Kit*.92f,.86f));
        Dress(Helmet,TEXT("Peak"),Make(ShellMat,Kit*.66f,.28f));
        Dress(Helmet,TEXT("Trim"),Make(GearMat,FLinearColor(.022,.024,.029),.62f));
        Dress(Helmet,TEXT("Pad"),Make(GearMat,FLinearColor(.036,.034,.032),.93f));
    }
    auto* PadFace=Make(GearMat,FLinearColor(.700,.712,.686),.78f);
    auto* PadRoll=Make(GearMat,FLinearColor(.612,.624,.600),.84f);
    auto* Strap=Make(GearMat,FLinearColor(.028,.030,.036),.70f);
    auto* Buckle=Make(ShellMat,FLinearColor(.330,.342,.362),.26f);
    for(auto Slot:{EC26EquipmentSlot::BattingPadL,EC26EquipmentSlot::BattingPadR,
        EC26EquipmentSlot::KeeperPadL,EC26EquipmentSlot::KeeperPadR})if(auto P=Find(Slot))
    {
        Dress(P,TEXT("PadFace"),PadFace);Dress(P,TEXT("PadRoll"),PadRoll);
        Dress(P,TEXT("PadStrap"),Strap);Dress(P,TEXT("PadBuckle"),Buckle);
    }
    auto* GlovePalm=Make(GearMat,FLinearColor(.212,.150,.098),.52f);
    auto* GlovePad=Make(GearMat,FLinearColor(.732,.744,.716),.76f);
    for(auto Slot:{EC26EquipmentSlot::BattingGloveL,EC26EquipmentSlot::BattingGloveR,
        EC26EquipmentSlot::KeeperGloveL,EC26EquipmentSlot::KeeperGloveR})if(auto P=Find(Slot))
    {
        Dress(P,TEXT("GlovePalm"),GlovePalm);Dress(P,TEXT("GlovePad"),GlovePad);
        Dress(P,TEXT("GloveCuff"),PadRoll);Dress(P,TEXT("PadStrap"),Strap);
    }
}
void UC26CharacterPresentationComponent::ApplyVisualRole(EC26VisualRole Role)
{
    if(!bActive||uint8(Role)>uint8(EC26VisualRole::Umpire))return;
    if(Role!=VisualRole)
    {
        const auto Errors=Profile->InspectRole(Role);
        if(!Errors.IsEmpty())
        {
            UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_ROLE_BLOCKED role=%d %s"),int32(Role),*Errors[0]);return;
        }
    }
    VisualRole=Role;
    USkeletalMesh* Model=Role==EC26VisualRole::Umpire?Profile->UmpireBody:Profile->Body;
    if(Role!=EC26VisualRole::Umpire)if(auto* Variant=Profile->BodyPresets.Find(Appearance.BodyPreset))Model=*Variant;
    if(Body->GetSkeletalMeshAsset()!=Model){Body->EmptyOverrideMaterials();Body->SetSkeletalMesh(Model);}
    // Explicitly set BOTH visible and hidden flags on every role change, regardless of LOD.
    for(const auto& Pair:Equipment)
    {
        const bool Visible=C26Character::Allows(Role,Pair.Key);
        Pair.Value->SetVisibility(Visible);Pair.Value->SetHiddenInGame(!Visible);
    }
    for(const auto& Item:Profile->Equipment)if(auto* Part=Equipment.Find(Item.Slot))
    {
        const bool Left=Appearance.LeftHandedBat&&!Item.LeftHandedSocket.IsNone();
        (*Part)->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,Left?Item.LeftHandedSocket:Item.Socket);
        (*Part)->SetRelativeTransform(Left?Item.LeftHandedOffset:Item.Offset);
    }
    CurrentClip=nullptr;CurrentState=NAME_None;Transition=NAME_None;TransitionAge=0;
}
FName UC26CharacterPresentationComponent::ReadyKey() const
{
    switch(VisualRole)
    {
    case EC26VisualRole::Batter:case EC26VisualRole::NonStriker:return Appearance.LeftHandedBat?TEXT("BatterReady_L"):TEXT("BatterReady_R");
    case EC26VisualRole::Bowler:return TEXT("BowlerReady");
    case EC26VisualRole::Keeper:return TEXT("KeeperReady");
    case EC26VisualRole::Umpire:return TEXT("UmpireReady");
    default:return TEXT("FielderReady");
    }
}
bool C26Presentation::WantsMove(bool bWasMoving,float GroundSpeed)
{
    return bWasMoving?GroundSpeed>StopSpeed:GroundSpeed>StartSpeed;
}
bool C26Presentation::TransitionSpent(FName Transition,float GroundSpeed,float TransitionAge)
{
    // Leave a transition as soon as the body has actually finished doing it. Holding a Start clip
    // while already at full pace, or a Stop clip after the athlete is stationary, is a stride the
    // ground no longer justifies: the planted foot skates for the remainder. Turn clips are short
    // and read correctly played to the end, so they are never cut.
    if(Transition==TEXT("Start"))return GroundSpeed>220.f&&TransitionAge>.22f;
    if(Transition==TEXT("Stop"))return GroundSpeed<5.f&&TransitionAge>.25f;
    return false;
}
float C26Presentation::StepMeshYawOffset(float Offset,float AuthoritativeYawDelta,float Dt)
{
    // A turn the athlete could plausibly have run through is left to the animation; only
    // discontinuities (a re-aim of more than ~15 degrees inside one frame) are absorbed. The lag
    // is then unwound, so the mesh rejoins the actor instead of staying permanently mis-aimed.
    if(Dt>0.f&&FMath::Abs(AuthoritativeYawDelta)>OrientationSnapDegrees)
        Offset=FMath::Clamp(Offset-AuthoritativeYawDelta,-MaxMeshYawLag,MaxMeshYawLag);
    return FMath::Abs(Offset)>.05f?FMath::FInterpTo(Offset,0.f,Dt,14.f):0.f;
}
float C26Presentation::AdvanceOutgoingPose(float PreviousTime,float PreviousLength,float Dt)
{
    // Wrapped, so a clip that reaches its end mid-blend restarts rather than clamping at the last
    // frame -- clamping is the same freeze this exists to remove, just one clip-length later.
    if(PreviousLength<=.01f)return PreviousTime;
    return FMath::Fmod(PreviousTime+FMath::Max(0.f,Dt),PreviousLength);
}
float C26Presentation::BlendWeight(float BlendClock,float BlendSeconds)
{
    return FMath::SmoothStep(0.f,FMath::Max(MinBlendSeconds,BlendSeconds),BlendClock);
}
bool UC26CharacterPresentationComponent::StateAllowsFootLock(FName State)
{
    // Only the shared locomotion and stance clips. Batting, bowling and fielding actions carry
    // authored footwork owned by other work; pinning a foot through a front-foot stride would
    // drag it. Those clips fall back to the animated pose untouched.
    static const TSet<FName> Allowed={TEXT("Walk"),TEXT("Run"),TEXT("Start"),TEXT("Stop"),
        TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("BatterRun_L"),TEXT("BatterRun_R"),
        TEXT("KeeperShuffle"),TEXT("UmpireWalk"),TEXT("FielderReady"),TEXT("KeeperReady"),
        TEXT("UmpireReady"),TEXT("BowlerReady"),TEXT("NonStrikerReady")};
    return Allowed.Contains(State);
}
FName UC26CharacterPresentationComponent::SelectState(const AC26Athlete* Athlete,float Dt)
{
    const bool Batter=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker;
    switch(Athlete->Action)
    {
    case EC26Action::Batting:
        // An empty label means no stroke was selected; holding the ready stance is
        // the only cricket-correct answer, never a "_R" lookup that warns and falls
        // back through the generic defence chain.
        if(Athlete->ShotLabel.IsEmpty())return ReadyKey();
        return C26Character::ShotKey(Athlete->ShotLabel,Appearance.LeftHandedBat);
    case EC26Action::Bowling:return C26Character::BowlingKey(Athlete->DeliveryStyle,Appearance.LeftArmBowl);
    case EC26Action::Pickup:return TEXT("Pickup");
    case EC26Action::Throw:return TEXT("Throw");
    case EC26Action::Catch:return VisualRole==EC26VisualRole::Keeper?TEXT("KeeperReceive"):TEXT("Catch");
    case EC26Action::Celebrate:return Batter?(Appearance.LeftHandedBat?TEXT("BatterCelebrate_L"):TEXT("BatterCelebrate_R")):TEXT("Celebrate");
    case EC26Action::Disappointed:return TEXT("Disappointed");
    case EC26Action::SignalFour:return TEXT("SignalFour");
    case EC26Action::SignalSix:return TEXT("SignalSix");
    case EC26Action::SignalOut:return TEXT("SignalOut");
    case EC26Action::SignalWide:return TEXT("SignalWide");
    default:break;
    }
    // Hysteresis: a single threshold made an athlete hovering near it flap between idle and
    // locomotion, restarting Start/Stop every few frames, which is the loudest popping source.
    const bool Moving=C26Presentation::WantsMove(bWasMoving,Locomotion.GroundSpeed);
    if(Dt>0.f)
    {
        if(Moving!=bWasMoving){Transition=Moving?TEXT("Start"):TEXT("Stop");TransitionAge=0;}
        else if(!Moving&&FMath::Abs(Locomotion.TurnRate)>65.f&&Transition.IsNone())
        {Transition=Locomotion.TurnRate>0?TEXT("TurnRight"):TEXT("TurnLeft");TransitionAge=0;}
        bWasMoving=Moving;TransitionAge+=Dt;
    }
    if(!Transition.IsNone())
    {
        const auto* Clip=Profile->FindClip(Transition);
        // Leave a transition as soon as the body has actually finished doing it. Holding a Start
        // clip while already at full pace, or a Stop clip after the athlete is stationary, is a
        // stride the ground no longer justifies: the planted foot skates for the remainder.
        const bool Spent=C26Presentation::TransitionSpent(Transition,Locomotion.GroundSpeed,TransitionAge);
        if(Clip&&!Spent&&TransitionAge<Clip->Sequence->GetPlayLength())return Transition;
        Transition=NAME_None;
    }
    if(Moving)
    {
        if(Batter)return Appearance.LeftHandedBat?TEXT("BatterRun_L"):TEXT("BatterRun_R");
        if(VisualRole==EC26VisualRole::Keeper&&Profile->FindClip(TEXT("KeeperShuffle")))return TEXT("KeeperShuffle");
        if(VisualRole==EC26VisualRole::Umpire&&Profile->FindClip(TEXT("UmpireWalk")))return TEXT("UmpireWalk");
        return Locomotion.GroundSpeed<180.f?TEXT("Walk"):TEXT("Run");
    }
    return ReadyKey();
}
void UC26CharacterPresentationComponent::ResetMotion()
{
    Locomotion.Reset(GetOwner()->GetActorTransform());Clock=BlendClock=0;Transition=NAME_None;bWasMoving=false;
    CurrentClip=nullptr;CurrentState=NAME_None;FrozenSeconds=0;
    LeftFootLock=RightFootLock=FC26FootLockState();PelvisCompensationZ=0.f;
    MeshYawOffset=0.f;bInitializedYaw=false;LocomotionPhase=0.f;GroundProbeAge=1.f;
    if(Body)
    {
        Body->SetRelativeLocation(FVector::ZeroVector);
        // A teleport must not leave the mesh wearing the previous frame's yaw lag.
        if(Profile)Body->SetRelativeRotation(Profile->MeshToGameplayRotation);
    }
}
void UC26CharacterPresentationComponent::UpdateOrientationSmoothing(const AC26Athlete* Athlete,float Dt)
{
    // PART C. The simulation re-aims fielders and the striker in a single frame, so the mesh
    // used to rotate instantaneously under a pose whose feet were planted. This carries a
    // mesh-only yaw offset that absorbs the snap and unwinds it over ~0.3s at 60Hz (under a tenth
    // of the snap is left after ~145ms). The actor's rotation, and therefore every gameplay query
    // that reads it, is never touched.
    if(!Body||!Profile)return;
    const float Yaw=Athlete->GetActorRotation().Yaw;
    if(!bInitializedYaw){LastAuthoritativeYaw=Yaw;bInitializedYaw=true;MeshYawOffset=0.f;return;}
    const float Delta=FMath::FindDeltaAngleDegrees(LastAuthoritativeYaw,Yaw);
    LastAuthoritativeYaw=Yaw;
    MeshYawOffset=C26Presentation::StepMeshYawOffset(MeshYawOffset,Delta,Dt);
    const FRotator Base=Profile->MeshToGameplayRotation;
    Body->SetRelativeRotation(FRotator(Base.Pitch,Base.Yaw+MeshYawOffset,Base.Roll));
}
void UC26CharacterPresentationComponent::UpdateFootStabilization(const AC26Athlete* Athlete,float Dt)
{
    // PART A. Planted-foot locking. Each frame the previous pose's ankle is compared against the
    // ground; an ankle that is at ground height and is meant to be carrying weight is pinned to
    // the world point where it landed, and the leg is solved back to that point until either the
    // stride lifts it or the hip has travelled far enough that holding on would straighten the
    // knee. The lock weight is a filtered ramp, so a foot is never teleported on or off a mark.
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body?Body->GetAnimInstance():nullptr);
    if(!Anim)return;
    // Performance: two-bone IK on a pair of legs is cheap, but it is not free across a full
    // fielding side. Athletes that are hidden, distant, or running at reduced quality skip it
    // outright; at that range a sliding foot is well under a pixel, so dropping it is invisible.
    if(!Profile||!Body->IsVisible()||Body->GetPredictedLODLevel()>1||Quality==EC26CharacterQuality::Low)
    {
        // Logged once per athlete: "why is the foot stabilizer not running" was otherwise only
        // answerable by reading live weights off a screenshot.
        if(!bLoggedFootSkip)
        {
            bLoggedFootSkip=true;
            UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_FOOT_SKIP id=%s profile=%d visible=%d lod=%d quality=%d"),
                *Appearance.PlayerID.ToString(),Profile?1:0,Body->IsVisible()?1:0,
                Body->GetPredictedLODLevel(),int32(Quality));
        }
        Anim->FootIKWeight=Anim->LeftFootLockAlpha=Anim->RightFootLockAlpha=Anim->PelvisOffsetZ=0.f;
        LeftFootLock=RightFootLock=FC26FootLockState();PelvisCompensationZ=0.f;
        return;
    }
    // Re-arm the one-shot above, so a stabilizer that starts being skipped LATER (an athlete
    // dropping past the LOD gate as it moves away) reports its reason too, instead of the log
    // being answered once by whatever happened on the first frame of the match.
    bLoggedFootSkip=false;
    // Entering an action the shared foundation does not own (a shot, a delivery, a dive) releases
    // any held mark, but releases it by fading the weight out. Cutting the weight to zero on the
    // frame the state changes would snap the ankle back to the authored pose, which is precisely
    // the popping this work exists to remove.
    const bool bAllowNewMarks=StateAllowsFootLock(CurrentState);

    // The height a planted ankle sits at is a property of the body, so it is measured from the
    // reference pose once rather than assumed. Guessing it high floats the foot above the pitch;
    // guessing it low presses it through.
    if(RefAnkleHeight<0.f)
    {
        RefAnkleHeight=0.f;
        if(const USkeletalMesh* Asset=Body->GetSkeletalMeshAsset())
        {
            const FReferenceSkeleton& Ref=Asset->GetRefSkeleton();
            auto RefAnkleZ=[&Ref](FName Bone)
            {
                const int32 Index=Ref.FindBoneIndex(Bone);
                if(Index==INDEX_NONE)return 0.f;
                FTransform Accumulated=FTransform::Identity;
                for(int32 I=Index;I!=INDEX_NONE;I=Ref.GetParentIndex(I))Accumulated*=Ref.GetRefBonePose()[I];
                return float(Accumulated.GetLocation().Z);
            };
            RefAnkleHeight=FMath::Min(RefAnkleZ(Profile->LeftFootBone),RefAnkleZ(Profile->RightFootBone));
        }
        UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_ANKLE_HEIGHT mesh=%s height=%.2fcm"),*GetNameSafe(Body->GetSkeletalMeshAsset()),RefAnkleHeight);
    }

    const FVector ActorLoc=Athlete->GetActorLocation();
    // The outfield is effectively a plane, so the trace is cached and refreshed only when the
    // athlete has actually moved or the cache has aged out.
    GroundProbeAge+=FMath::Max(0.f,Dt);
    if(GroundProbeAge>.25f||FVector::DistSquared2D(GroundProbePos,ActorLoc)>625.f)
    {
        GroundProbeAge=0.f;GroundProbePos=ActorLoc;GroundProbeZ=ActorLoc.Z;
        if(UWorld* World=Athlete->GetWorld())
        {
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(C26FootGround),false,Athlete);
            if(World->LineTraceSingleByChannel(Hit,ActorLoc+FVector(0,0,60.f),ActorLoc-FVector(0,0,120.f),ECC_WorldStatic,Params))
                GroundProbeZ=Hit.ImpactPoint.Z;
        }
    }
    const float PlantedAnkleZ=GroundProbeZ+RefAnkleHeight;

    const FTransform ToWorld=Body->GetComponentTransform();
    // Landing tolerance widens as the athlete slows: a decelerating or settling stride plants
    // heavily and the authored ankle sits further off the plane than it does at a steady jog.
    const float PlantTolerance=Locomotion.GroundSpeed<60.f?6.f:4.f;

    // Judge every plant and lift from the authored pose, never from the stabilized skeleton.
    // Feeding the solver its own output would make a held foot look permanently grounded, so it
    // would never be released and the leg would drag until the hip tore it off the mark.
    const bool bCleanFeet=Anim->bAnimatedFeetValid;
    auto Process=[&](FC26FootLockState& Lock,FName FootBone,FName ThighBone,const FVector& AnimatedCS,float& OutAlpha,FVector& OutTarget)
    {
        const FVector Foot=bCleanFeet?ToWorld.TransformPosition(AnimatedCS):Body->GetSocketLocation(FootBone);
        const FVector Hip=Body->GetSocketLocation(ThighBone);
        const float AboveGround=Foot.Z-PlantedAnkleZ;

        // Only take a fresh mark once the previous one has faded out, otherwise the solver would
        // jump the foot from one world point to another inside a single frame. Marking at ground
        // height rather than at the authored height is what conforms the foot to the pitch.
        if(bAllowNewMarks&&!Lock.bLocked&&Lock.LockAlpha<.25f&&FMath::Abs(AboveGround)<PlantTolerance)
        {
            Lock.bLocked=true;
            Lock.LockedWorldPos=FVector(Foot.X,Foot.Y,PlantedAnkleZ);
            // Under the debug CVar only: proof in a log that a mark was really taken in a match,
            // rather than an on-screen weight read off a screenshot.
            if(C26CharacterDebug.GetValueOnGameThread())
                UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_FOOT_LOCK id=%s foot=%s above=%.1fcm speed=%.0f state=%s"),
                    *Appearance.PlayerID.ToString(),*FootBone.ToString(),AboveGround,Locomotion.GroundSpeed,*CurrentState.ToString());
        }
        if(Lock.bLocked)
        {
            // Release before the knee has to lock out, and release as soon as the authored stride
            // genuinely lifts the foot. Turning on the spot and shuffling are near-zero ground
            // speed but do pick the feet up, so lift-off is checked at every speed. Both releases
            // fade the weight out rather than switching it.
            const float Reach=FVector::Dist(Hip,Lock.LockedWorldPos);
            if(!bAllowNewMarks)Lock.bLocked=false;
            else if(Reach>C26Presentation::LegReach*.94f)Lock.bLocked=false;
            else if(AboveGround>C26Presentation::LiftHeight)Lock.bLocked=false;
        }
        Lock.LockAlpha=FMath::FInterpTo(Lock.LockAlpha,Lock.bLocked?1.f:0.f,Dt,Lock.bLocked?18.f:24.f);
        if(Lock.LockAlpha<=.001f){Lock.LockAlpha=0.f;OutAlpha=0.f;OutTarget=ToWorld.InverseTransformPosition(Foot);return;}
        // The solver is given the mark itself; LockAlpha is the weight the solved leg is blended
        // in at, so partial weights read as the leg easing onto and off the mark.
        OutAlpha=Lock.LockAlpha;
        OutTarget=ToWorld.InverseTransformPosition(Lock.LockedWorldPos);
    };
    const FVector LeftAnimated=bCleanFeet?ToWorld.TransformPosition(Anim->AnimatedLeftFootCS):Body->GetSocketLocation(Profile->LeftFootBone);
    const FVector RightAnimated=bCleanFeet?ToWorld.TransformPosition(Anim->AnimatedRightFootCS):Body->GetSocketLocation(Profile->RightFootBone);
    Process(LeftFootLock,Profile->LeftFootBone,TEXT("thigh_l"),Anim->AnimatedLeftFootCS,Anim->LeftFootLockAlpha,Anim->LeftFootTargetCS);
    Process(RightFootLock,Profile->RightFootBone,TEXT("thigh_r"),Anim->AnimatedRightFootCS,Anim->RightFootLockAlpha,Anim->RightFootTargetCS);
    Anim->FootIKWeight=1.f;

    // Subtle pelvis compensation: when a held mark sits below the animated ankle the leg would
    // have to reach for it, so the hips drop by the deeper of the two gaps and the knees keep
    // their flex. Clamped to a few centimetres so it never reads as a crouch.
    //
    // Only the "mark below the ankle" case is compensated. That is the one direction that can
    // pull the leg straight, because the solver has to travel further down than the authored
    // pose did. A mark ABOVE the ankle needs nothing: the solver reaches it by bending, so
    // sinking the pelvis there would only add a crouch the stride never asked for.
    float Desired=0.f;
    const float LeftGap=-FMath::Max(0.f,float(LeftAnimated.Z)-PlantedAnkleZ);
    const float RightGap=-FMath::Max(0.f,float(RightAnimated.Z)-PlantedAnkleZ);
    if(LeftFootLock.LockAlpha>.1f&&RightFootLock.LockAlpha>.1f)Desired=FMath::Min(LeftGap,RightGap);
    else if(LeftFootLock.LockAlpha>.1f)Desired=LeftGap*.5f;
    else if(RightFootLock.LockAlpha>.1f)Desired=RightGap*.5f;
    PelvisCompensationZ=FMath::FInterpTo(PelvisCompensationZ,FMath::Clamp(Desired,-6.f,0.f),Dt,12.f);
    Anim->PelvisOffsetZ=PelvisCompensationZ;
}
void UC26CharacterPresentationComponent::UpdateFromMatch(AC26Athlete* Athlete,float Dt)
{
    if(!bActive||!Body)return;
#if !UE_BUILD_SHIPPING
    const int32 PreviewRole=C26CharacterRole.GetValueOnGameThread();
    if(Athlete->SquadNumber==C26CharacterNumber.GetValueOnGameThread()&&PreviewRole>=0&&PreviewRole<=5)
    {bDebugRole=true;if(VisualRole!=EC26VisualRole(PreviewRole))ApplyVisualRole(EC26VisualRole(PreviewRole));}
    else if(bDebugRole){bDebugRole=false;Configure(Athlete);}
#endif
    Locomotion.Update(Athlete->GetActorTransform(),Dt);
    if(Locomotion.Teleported)ResetMotion();
    Clock+=FMath::Max(0.f,Dt);
    FName State=SelectState(Athlete,Dt);const FC26CricketClip* Clip=Profile->FindClip(State);
    if(!Clip)
    {
        if(!ReportedMissing.Contains(State)){ReportedMissing.Add(State);UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_MISSING_CLIP %s"),*State.ToString());}
        // Only cricket-correct fallbacks. Bowling never crosses style families.
        FName Fallback=ReadyKey();
        if(Athlete->Action==EC26Action::Batting)
        {
            FString Name=State.ToString();const FString Hand=Appearance.LeftHandedBat?TEXT("_L"):TEXT("_R");
            Fallback=FName(*(Name.Contains(TEXT("COVER"))?TEXT("COVERDRIVE")+Hand:
                Name.Contains(TEXT("HOOK"))?TEXT("PULL")+Hand:
                Name.Contains(TEXT("GLANCE"))||Name.Contains(TEXT("FLICK"))?TEXT("LEGGLANCE")+Hand:
                Name.Contains(TEXT("SWEEP"))?TEXT("SWEEP")+Hand:TEXT("FRONTFOOTDEFENCE")+Hand));
        }
        State=Fallback;Clip=Profile->FindClip(State);
    }
    if(!Clip||!Clip->Sequence)return; // Activation validation guarantees all fallback sequences.
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());if(!Anim)return;
    if(CurrentClip!=Clip)
    {
        PreviousState=CurrentState.IsNone()?State:CurrentState;
        Anim->PreviousSequence=Anim->CurrentSequence?Anim->CurrentSequence:Clip->Sequence;
        Anim->PreviousTime=Anim->CurrentTime;CurrentClip=Clip;CurrentState=State;BlendClock=0;
        UpdateWarp(Athlete,Clip);
    }
    BlendClock+=FMath::Max(0.f,Dt);
    // The outgoing pose kept playing before the blend started; freezing it at the switch frame
    // reads as the body stalling for the blend duration. Keep advancing it until it is inaudible.
    if(Anim->PreviousSequence&&BlendClock<Clip->BlendSeconds)
        Anim->PreviousTime=C26Presentation::AdvanceOutgoingPose(Anim->PreviousTime,
            Anim->PreviousSequence->GetPlayLength(),Dt);
    float Time=Athlete->ActionTime;
    if(State==Transition)Time=TransitionAge;
    else if(Clip->Loop)
    {
        const bool Move=State==TEXT("Run")||State==TEXT("Walk")||State==TEXT("BatterRun_R")||State==TEXT("BatterRun_L")||State==TEXT("KeeperShuffle")||State==TEXT("UmpireWalk");
        const float Length=Clip->Sequence->GetPlayLength();
        if(Move)
        {
            // Still stride-matched to ground distance, but advanced incrementally. Reading the
            // phase straight off cumulative Distance made Walk->Run jump to an unrelated frame
            // (the two clips cover different ground per cycle), which popped both feet at once.
            LocomotionPhase+=Locomotion.GroundSpeed*FMath::Max(0.f,Dt)/FMath::Max(1.f,Clip->GroundSpeed);
            if(Length>.01f)LocomotionPhase=FMath::Fmod(LocomotionPhase,Length);
            Time=LocomotionPhase;
        }
        else Time=FMath::Fmod(Clock,Length);
    }
    else if(!Clip->Event.IsNone())
    {
        const float MatchEvent=Clip->Event==TEXT("BatContact")?C26Field::BatContactPoseTime:
            Clip->Event==TEXT("BallRelease")?C26Field::ReleasePoseTime:
            Clip->Event==TEXT("ThrowRelease")?.2f:.18f;
        Time=C26Character::MapEventTime(Athlete->ActionTime,MatchEvent,Clip->EventTime(),Clip->Sequence->GetPlayLength());
    }
    else Time=FMath::Min(Time,Clip->Sequence->GetPlayLength());
    Anim->CurrentSequence=Clip->Sequence;Anim->CurrentTime=Time;
    Anim->BlendAlpha=C26Presentation::BlendWeight(BlendClock,Clip->BlendSeconds);
    // An exact authoritative contact sample must render that event pose this frame.
    if(Dt==0.f&&!Clip->Event.IsNone()&&FMath::IsNearlyEqual(Time,Clip->EventTime(),.001f))Anim->BlendAlpha=1.f;
    Anim->GroundSpeed=Locomotion.GroundSpeed;Anim->MovementDirection=Locomotion.Direction;
    Anim->Acceleration=Locomotion.Acceleration;Anim->TurnRate=Locomotion.TurnRate;Anim->State=State;
    UpdateOrientationSmoothing(Athlete,Dt);
    UpdateFootStabilization(Athlete,Dt);
    Body->TickAnimation(FMath::Max(0.f,Dt),false);Body->RefreshBoneTransforms();
    LearnWarp(Athlete);
    Debug(Athlete,Dt);
}
void UC26CharacterPresentationComponent::LearnWarp(const AC26Athlete* Athlete)
{
    // Pure measurement (no visual shifting): at the contact frame, log where the
    // authored hands are versus the simulation contact point. The old blade check
    // sampled the ball a frame AFTER contact (already travelled ~1m), so it could
    // never pass for any system; this is the honest sync signal.
    if(!CurrentClip||CurrentClip->Event!=TEXT("BatContact"))return;
    const float Tc=C26Field::BatContactPoseTime;
    const bool Crossed=(WarpPrevTime<Tc&&Athlete->ActionTime>=Tc)
        ||(Athlete->ActionTime==Tc&&Tc>0.f);
    WarpPrevTime=Athlete->ActionTime;
    if(!Crossed||Athlete->ContactTarget.IsZero())return;
    const FVector Miss=Athlete->ContactTarget-ReceivePosition();
    UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_CONTACT clip=%s hands=%s simContact=%s miss=%s (|%.1fcm|)"),
        *CurrentState.ToString(),*ReceivePosition().ToString(),*Athlete->ContactTarget.ToString(),
        *Miss.ToString(),Miss.Size());
}
void UC26CharacterPresentationComponent::UpdateWarp(const AC26Athlete* Athlete,const FC26CricketClip* Clip)
{
    // No visual shifting: per-ball root warps were built for a broken metric
    // (the old blade check sampled the ball a frame after contact) and lurched
    // visibly. Contact sync is fixed structurally (guard mark + shot-matched
    // contact depth); this function now only resets the body offset.
    if(Body)Body->SetRelativeLocation(FVector::ZeroVector);
    WarpPrevTime=0.f;
}
FVector UC26CharacterPresentationComponent::BallHandPosition() const
{return Body->GetSocketLocation(Appearance.LeftArmBowl?Profile->LeftHandSocket:Profile->RightHandSocket);}
FVector UC26CharacterPresentationComponent::ReceivePosition() const
{return (Body->GetSocketLocation(Profile->LeftHandSocket)+Body->GetSocketLocation(Profile->RightHandSocket))*.5f;}
void UC26CharacterPresentationComponent::SetQualityForView(const FVector& ViewPoint)
{
    if(!bActive)return;
    const float Distance=FVector::Distance(ViewPoint,Body->GetComponentLocation());
    const bool Active=Locomotion.GroundSpeed>12.f||VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::Bowler||VisualRole==EC26VisualRole::Keeper;
    const int32 Count=Body->GetSkeletalMeshAsset()->GetLODNum();
    const int32 Tier=Distance>6000.f?3:Distance>3000.f?2:Distance>1400.f?1:0;
    const int32 Bias=Quality==EC26CharacterQuality::Low?1:0;
    Body->SetForcedLOD(FMath::Clamp(Tier+Bias,0,FMath::Max(0,Count-1))+1);
    Body->UpdateLODStatus();
    Body->SetCastShadow(Active||Distance<4000.f);
    // Quality NEVER changes role visibility or freezes active motion. Hair/face reduction belongs
    // in the imported LODs; no runtime strand-hair dependency is introduced.
}
bool UC26CharacterPresentationComponent::ValidateRuntime() const
{
    bool Good=Body&&Body->GetSkeletalMeshAsset()&&Body->GetAnimInstance()&&Body->GetAnimClass()&&Profile&&uint8(VisualRole)<=5;
    if(Body&&Profile)Good&=Body->GetSkeletalMeshAsset()->GetSkeleton()==Profile->Skeleton;
    for(uint8 S=0;S<=uint8(EC26EquipmentSlot::Accessory);++S)
    {
        const auto Slot=EC26EquipmentSlot(S);const auto* Part=Equipment.Find(Slot);
        if(C26Character::Requires(VisualRole,Slot))Good&=Part&&*Part&&(*Part)->IsVisible()&&!(*Part)->bHiddenInGame;
        if(Part&&*Part&&!C26Character::Allows(VisualRole,Slot))Good&=!(*Part)->IsVisible()&&(*Part)->bHiddenInGame;
    }
    if(!Good)UE_LOG(LogTemp,Error,TEXT("C26_CHARACTER_RUNTIME_INVALID id=%s role=%d"),*Appearance.PlayerID.ToString(),int32(VisualRole));
    return Good;
}
void UC26CharacterPresentationComponent::Debug(const AC26Athlete* Athlete,float Dt)
{
#if !UE_BUILD_SHIPPING
    const FVector L=Body->GetBoneLocation(Profile->LeftFootBone,EBoneSpaces::ComponentSpace);
    const FVector R=Body->GetBoneLocation(Profile->RightFootBone,EBoneSpaces::ComponentSpace);
    if(Dt>0.f)
    {
        const bool Frozen=Locomotion.GroundSpeed>100.f&&(L-LastLeftFoot).Size()+(R-LastRightFoot).Size()<.03f;
        FrozenSeconds=Frozen?FrozenSeconds+Dt:0.f;LastLeftFoot=L;LastRightFoot=R;
        if(FrozenSeconds>.5f){UE_LOG(LogTemp,Error,TEXT("C26_CHARACTER_FROZEN id=%s speed=%.1f state=%s"),*Appearance.PlayerID.ToString(),Locomotion.GroundSpeed,*CurrentState.ToString());FrozenSeconds=0;}
    }
    if(C26CharacterDebug.GetValueOnGameThread())
    {
        // RuntimeIK is no longer off, so report the live stabilizer weights instead of asserting
        // a constant: an in-match capture can then prove the solver engaged rather than trust it.
        const auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
        const FString Text=FString::Printf(TEXT("%s Role:%d %s\n%s LOD:%d Speed:%.0f Dir:%.0f\nAnim:%s Gear:%d RootMotion:off FootIK:%.2f Lock:L%.2f R%.2f Pelvis:%.1f"),
            *Appearance.PlayerID.ToString(),int32(VisualRole),*CurrentState.ToString(),*GetNameSafe(Body->GetSkeletalMeshAsset()),Body->GetPredictedLODLevel(),
            Locomotion.GroundSpeed,Locomotion.Direction,*GetNameSafe(Body->GetAnimClass()),Equipment.Num(),
            Anim?Anim->FootIKWeight:0.f,Anim?Anim->LeftFootLockAlpha:0.f,Anim?Anim->RightFootLockAlpha:0.f,Anim?Anim->PelvisOffsetZ:0.f);
        DrawDebugString(GetWorld(),Athlete->GetActorLocation()+FVector(0,0,210),Text,nullptr,FColor::White,0.f,true);
        DrawDebugSphere(GetWorld(),BallHandPosition(),3.f,8,FColor::Cyan,false,0.f);
        for(FName Foot:{Profile->LeftFootBone,Profile->RightFootBone})DrawDebugSphere(GetWorld(),Body->GetBoneLocation(Foot),4.f,8,FColor::Green,false,0.f);
    }
#endif
}

UStaticMeshComponent* UC26CharacterPresentationComponent::GetBat() const
{const auto* Part=Equipment.Find(EC26EquipmentSlot::Bat);return Part?Part->Get():nullptr;}
FC26CharacterPoseSample UC26CharacterPresentationComponent::CapturePose() const
{
    FC26CharacterPoseSample Out;
    if(const auto* Anim=Body?Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance()):nullptr)
    {
        Out.State=CurrentState;Out.PreviousState=PreviousState;Out.Time=Anim->CurrentTime;
        Out.PreviousTime=Anim->PreviousTime;Out.Alpha=Anim->BlendAlpha;
        Out.Distance=Locomotion.Distance;Out.Clock=Clock;Out.GroundSpeed=Locomotion.GroundSpeed;
    }
    return Out;
}
void UC26CharacterPresentationComponent::ApplyReplayPose(const FC26CharacterPoseSample& A,const FC26CharacterPoseSample& B,float Alpha)
{
    if(!bActive)return;
    const auto& Frame=Alpha<.5f?A:B;
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    const auto* Clip=Profile->FindClip(Frame.State);if(!Anim||!Clip)return;
    const auto* Prior=Profile->FindClip(Frame.PreviousState);
    Anim->CurrentSequence=Clip->Sequence;Anim->PreviousSequence=Prior?Prior->Sequence:Clip->Sequence;
    Anim->CurrentTime=A.State==B.State?FMath::Lerp(A.Time,B.Time,Alpha):Frame.Time;
    // A looping clip wraps to zero; interpolation across that boundary must not reverse a stride.
    if(Clip->Loop&&A.State==B.State&&B.Time<A.Time)
        Anim->CurrentTime=FMath::Fmod(FMath::Lerp(A.Time,B.Time+Clip->Sequence->GetPlayLength(),Alpha),Clip->Sequence->GetPlayLength());
    Anim->PreviousTime=Frame.PreviousTime;Anim->BlendAlpha=Frame.Alpha;
    CurrentState=Frame.State;PreviousState=Frame.PreviousState;CurrentClip=Clip;
    Clock=FMath::Lerp(A.Clock,B.Clock,Alpha);BlendClock=Frame.Alpha*Clip->BlendSeconds;
    Locomotion.Reset(GetOwner()->GetActorTransform());Locomotion.Distance=FMath::Lerp(A.Distance,B.Distance,Alpha);
    Locomotion.GroundSpeed=FMath::Lerp(A.GroundSpeed,B.GroundSpeed,Alpha);bWasMoving=Locomotion.GroundSpeed>12.f;
    Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
}
