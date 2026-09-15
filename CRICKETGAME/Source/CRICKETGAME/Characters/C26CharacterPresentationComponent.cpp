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
    ProfileAsset=TSoftObjectPtr<UC26CharacterProfile>(FSoftObjectPath(TEXT("/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer.DA_C26_DefaultPlayer")));
}
bool UC26CharacterPresentationComponent::TryActivate(AC26Athlete* Athlete)
{
    if(bActive)return true;if(!Athlete)return false;
    FString Path=ProfileAsset.ToSoftObjectPath().ToString();
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
            ||(Role==EC26VisualRole::Batter&&(ReviewNumber<0||Athlete->SquadNumber==ReviewNumber||Athlete->SquadNumber==7||Athlete->SquadNumber==18))
            ||(Role==EC26VisualRole::NonStriker&&(ReviewNumber<0||Athlete->SquadNumber==ReviewNumber||Athlete->SquadNumber==18||Athlete->SquadNumber==7));
        if(!Representative)return false;
    }
#endif
    static TSet<FString> Rejected;
    const FString GateKey=Path+FString::Printf(TEXT(":%d:%d"),int32(Role),Slice);
    if(Rejected.Contains(GateKey))return false;
    Profile=Path.IsEmpty()?nullptr:LoadObject<UC26CharacterProfile>(nullptr,*Path);
    // One shipped cast: a missing or mistyped override falls back to the approved default player, never to
    // the unapproved review candidates the rounds were built on.
    if(!Profile)Profile=LoadObject<UC26CharacterProfile>(nullptr,TEXT("/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer.DA_C26_DefaultPlayer"));
    TArray<FString> Errors;
    if(!Profile)Errors.Add(TEXT("No complete approved character profile at ")+Path);
    else Errors=Profile->InspectRole(Role);
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
    AssignBodyMesh(Profile->ResolveBody(Role,Appearance.BodyPreset));Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Body->SetAnimInstanceClass(UC26CricketerAnimInstance::StaticClass());
    Body->bEnableUpdateRateOptimizations=false;
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Body->SetBoundsScale(1.5f);Body->RegisterComponent();Body->SetComponentTickEnabled(false);
    for(const auto& Item:Profile->Equipment)
    {
        auto* Part=NewObject<UStaticMeshComponent>(Athlete);
        Athlete->AddInstanceComponent(Part);Part->SetStaticMesh(Item.Mesh);
        Part->SetupAttachment(Body,Item.ResolveSocket(Athlete->LeftHandedBat));Part->SetRelativeTransform(Item.ResolveOffset(Athlete->LeftHandedBat));
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
        *Appearance.PlayerID.ToString(),int32(VisualRole),*Profile->ResolveBody(VisualRole,Appearance.BodyPreset)->GetName(),*GetNameSafe(Profile->Skeleton),*GetNameSafe(Body->GetAnimClass()));
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
    // Temperament rides on the profile's existing AnimationStyle. An explicitly authored style is kept;
    // the default "Professional" is resolved per squad slot so eleven players never react in unison.
    static const FName DefaultStyle=TEXT("Professional");
    FName Style=Appearance.AnimationStyle;
    if(Style.IsNone()||Style==DefaultStyle||Style==TEXT("Calm")||Style==TEXT("Aggressive")||Style==TEXT("Energetic"))
        Style=Appearance.AnimationStyle=C26Character::Temperament(Athlete->TeamId,Athlete->SquadNumber);
    const uint32 Seed=FCrc::StrCrc32(*NewID.ToString());
    const float Jitter=float(Seed%1000)/1000.f;              // 0..1, stable per player
    const bool Calm=Style==TEXT("Calm"),Energetic=Style==TEXT("Energetic");
    StyleRate=(Calm?.94f:Energetic?1.08f:1.04f)*(.98f+.04f*Jitter);
    StyleDelay=Calm?.10f:Energetic?0.f:.03f;
    StyleLife=Calm?.8f:Energetic?1.15f:1.f;
    StyleLookSpeed=Calm?2.6f:Energetic?5.5f:4.f;
    BreathRate=(Energetic?.36f:.28f)*(.92f+.16f*Jitter);    // breaths per second at rest
    BreathPhase=Jitter*UE_TWO_PI;SwayPhase=Jitter*4.1f;EvaluationCounter=Seed%6;
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
    ApplyBodyMaterials(Athlete->TeamId);
    DressEquipment(Athlete->TeamId);
}
void UC26CharacterPresentationComponent::AssignBodyMesh(USkeletalMesh* Model)
{
    if(Body->GetSkeletalMeshAsset()==Model)return;
    // Mesh assignment must clear the previous mesh's slot-index overrides. The existing native
    // animation instance remains the pose driver; mesh selection does not select gameplay state.
    Body->EmptyOverrideMaterials();Body->SetSkeletalMesh(Model);
    ApplyMaterialOverrides();
}
void UC26CharacterPresentationComponent::ApplyMaterialOverrides()
{
    for(const auto& Override:Profile->MaterialOverrides)
    {
        const int32 Slot=Body->GetMaterialIndex(Override.Key);
        if(Slot>=0&&Override.Value)Body->SetMaterial(Slot,Override.Value);
    }
}
void UC26CharacterPresentationComponent::ApplyBodyMaterials(int32 Team)
{
    ApplyMaterialOverrides();
    const int32 Slot=Body->GetMaterialIndex(Profile->JerseyMaterialSlot);
    if(Slot<0)return;
    if(VisualRole!=EC26VisualRole::Umpire)
    {
        if(Profile->TeamMaterials.IsValidIndex(Team))Body->SetMaterial(Slot,Profile->TeamMaterials[Team]);
    }
    else if(!Profile->TeamMaterials.IsEmpty()&&Profile->TeamMaterials[0])
    {
        // The umpire shares the athlete body, and its base shirt read as a third member of the batting side.
        // Officials wear a neutral charcoal shirt that neither team's kit can be confused with.
        auto* Official=UMaterialInstanceDynamic::Create(Profile->TeamMaterials[0],GetOwner());
        Official->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.018f,.020f,.024f));
        Body->SetMaterial(Slot,Official);
    }
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
    AssignBodyMesh(Profile->ResolveBody(Role,Appearance.BodyPreset));
    // Explicitly set BOTH visible and hidden flags on every role change, regardless of LOD.
    for(const auto& Pair:Equipment)
    {
        const bool Visible=C26Character::Allows(Role,Pair.Key);
        Pair.Value->SetVisibility(Visible);Pair.Value->SetHiddenInGame(!Visible);
    }
    RefreshEquipmentAttachments();
    CurrentClip=nullptr;CurrentState=NAME_None;Transition=NAME_None;TransitionAge=0;
}
void UC26CharacterPresentationComponent::RefreshEquipmentAttachments()
{
    for(const auto& Item:Profile->Equipment)if(auto* Part=Equipment.Find(Item.Slot))
    {
        (*Part)->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,Item.ResolveSocket(Appearance.LeftHandedBat));
        (*Part)->SetRelativeTransform(EquipmentOffset(Item));
    }
}
FTransform UC26CharacterPresentationComponent::PosedSocket(UAnimSequence* Sequence,float Time,FName Socket)
{
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
    if(!Anim||!Sequence)return FTransform::Identity;
    const TObjectPtr<UAnimSequence> SavedPrevious=Anim->PreviousSequence,SavedCurrent=Anim->CurrentSequence;
    const float SavedPreviousTime=Anim->PreviousTime,SavedTime=Anim->CurrentTime,SavedAlpha=Anim->BlendAlpha;
    const FC26SecondaryMotion SavedLife=Anim->Life;const FC26BatControl SavedControl=Anim->BatControl;
    Anim->PreviousSequence=Anim->CurrentSequence=Sequence;Anim->PreviousTime=Anim->CurrentTime=Time;
    Anim->BlendAlpha=1;Anim->Life=FC26SecondaryMotion();Anim->BatControl=FC26BatControl();
    Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
    const FTransform Result=Body->GetSocketTransform(Socket,RTS_Component);
    Anim->PreviousSequence=SavedPrevious;Anim->CurrentSequence=SavedCurrent;Anim->PreviousTime=SavedPreviousTime;
    Anim->CurrentTime=SavedTime;Anim->BlendAlpha=SavedAlpha;Anim->Life=SavedLife;Anim->BatControl=SavedControl;
    return Result;
}
FTransform UC26CharacterPresentationComponent::EquipmentOffset(const FC26EquipmentDefinition& Item)
{
    const bool Left=Appearance.LeftHandedBat;
    if(Item.Slot!=EC26EquipmentSlot::Bat||!Left||!Item.Mesh||Item.LeftHandedSocket.IsNone()||Item.LeftHandedSocket==Item.Socket
        ||!Item.LeftHandedOffset.Equals(Item.Offset,1e-3f))return Item.ResolveOffset(Left);
    // The profile carries no authored left-handed bat offset (it repeats the right-handed one, which put the
    // left-hander's top hand below his bottom hand for the whole innings). The left-handed clips are the
    // sagittal mirror of the right-handed ones, so the left-handed bat is the mirror of the right-handed bat:
    // reflect the right-handed stance bat across the mesh's sagittal plane (X; the mesh faces +Y), flip the
    // bat's own face axis to keep it a proper rotation, and express that in the left-handed grip socket.
    if(!LeftBatOffset.IsSet())
    {
        const auto* Right=Profile->FindClip(TEXT("BatterReady_R"));const auto* Mirror=Profile->FindClip(TEXT("BatterReady_L"));
        if(!Right||!Mirror||!Right->Sequence||!Mirror->Sequence)return Item.ResolveOffset(Left);
        const FTransform RightBat=Item.Offset*PosedSocket(Right->Sequence,0.f,Item.Socket);
        const FTransform MirrorGrip=PosedSocket(Mirror->Sequence,0.f,Item.LeftHandedSocket);
        const FVector Extent=Item.Mesh->GetBoundingBox().GetExtent();
        const FMatrix Face=FScaleMatrix(Extent.X>=Extent.Y?FVector(-1,1,1):FVector(1,-1,1));
        const FMatrix Reflected=Face*RightBat.ToMatrixWithScale()*FScaleMatrix(FVector(-1,1,1));
        LeftBatOffset=FTransform(Reflected).GetRelativeTransform(MirrorGrip);
    }
    return LeftBatOffset.GetValue();
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
FName UC26CharacterPresentationComponent::Variant(const AC26Athlete* Athlete)
{
    if(!LatchedKey.IsNone())return LatchedKey;
    const bool Keeper=VisualRole==EC26VisualRole::Keeper;
    const auto Pick=[this](const TCHAR* Want,const TCHAR* Base){return Profile->FindClip(Want)?FName(Want):FName(Base);};
    const FVector Local=Athlete->GetActorTransform().InverseTransformPosition(Athlete->ContactTarget);
    // A variant keyed on where the ball is only means something once the ball is within reach.
    const bool InReach=!Athlete->ContactTarget.IsZero()&&FVector2D(Local.X,Local.Y).Size()<260.f;
    const bool Left=Local.Y<0.f;   // UE +Y is the athlete's right
    FName Key;bool Latch=true;
    switch(Athlete->Action)
    {
    case EC26Action::Bowling:
    {
        Key=C26Character::BowlingKey(Athlete->DeliveryStyle,Athlete->LeftArmBowl);
        const bool Pace=Key.ToString().StartsWith(TEXT("FastBowl"));
        const bool Medium=Athlete->BowlerKind==EC26BowlerKind::FastMedium||Athlete->BowlerKind==EC26BowlerKind::Medium;
        if(Pace&&Medium)Key=Pick(Athlete->LeftArmBowl?TEXT("FastMedium_L"):TEXT("FastMedium_R"),*Key.ToString());
        break;
    }
    case EC26Action::Pickup:Key=EntrySpeed>150.f?Pick(TEXT("PickupRunning"),TEXT("Pickup")):FName(TEXT("Pickup"));break;
    case EC26Action::Throw:
    {
        // An infield return close to either set of stumps is snapped flat, not crow-hopped.
        const FVector P=Athlete->GetActorLocation();
        const float ToStumps=FMath::Min(FVector::Dist2D(P,FVector(0,C26Field::WicketY,0)),FVector::Dist2D(P,FVector(0,-C26Field::WicketY,0)));
        Key=ToStumps<1800.f?Pick(TEXT("ThrowQuick"),TEXT("Throw")):FName(TEXT("Throw"));
        break;
    }
    case EC26Action::Slide:Key=Pick(TEXT("SlideSave"),TEXT("Pickup"));break;
    case EC26Action::Dive:
        Key=Local.Z>40.f?Pick(Left?TEXT("DiveCatch_L"):TEXT("DiveCatch_R"),TEXT("Catch"))
                        :Pick(Left?TEXT("DiveStop_L"):TEXT("DiveStop_R"),TEXT("Pickup"));
        Latch=InReach;
        break;
    case EC26Action::Catch:
        Latch=InReach;
        if(Keeper)
        {
            Key=TEXT("KeeperReceive");
            if(!InReach)break;
            if(FMath::Abs(Local.Y)>70.f)Key=Pick(Left?TEXT("KeeperDive_L"):TEXT("KeeperDive_R"),TEXT("KeeperReceive"));
            else if(Local.Z<35.f)Key=Pick(TEXT("KeeperTakeLow"),TEXT("KeeperReceive"));
            break;
        }
        Key=TEXT("Catch");
        if(!InReach)break;
        if(FMath::Abs(Local.Y)>85.f)Key=Pick(Left?TEXT("DiveCatch_L"):TEXT("DiveCatch_R"),TEXT("Catch"));
        else if(Local.Z>165.f)Key=Pick(TEXT("CatchHigh"),TEXT("Catch"));
        else if(Local.Z<70.f)Key=Pick(TEXT("CatchLow"),TEXT("Catch"));
        break;
    default:Key=ReadyKey();Latch=false;break;
    }
    if(Latch)LatchedKey=Key;
    return Key;
}
FName UC26CharacterPresentationComponent::SelectState(const AC26Athlete* Athlete,float Dt)
{
    ReactionClock=-1.f;
    if(bRecovered)return IdleState(Athlete,Dt);
    switch(Athlete->Action)
    {
    case EC26Action::Batting:
        // An empty label means no stroke was selected; holding the ready stance is
        // the only cricket-correct answer, never a "_R" lookup that warns and falls
        // back through the generic defence chain.
        if(Athlete->ShotLabel.IsEmpty())return ReadyKey();
        return C26Character::ShotKey(Athlete->ShotLabel,Appearance.LeftHandedBat);
    case EC26Action::Bowling:case EC26Action::Pickup:case EC26Action::Throw:case EC26Action::Catch:
    case EC26Action::Dive:case EC26Action::Slide:return Variant(Athlete);
    case EC26Action::Ready:case EC26Action::Celebrate:case EC26Action::Disappointed:case EC26Action::BatRaise:
    case EC26Action::GloveTap:case EC26Action::FistPump:case EC26Action::Handshake:case EC26Action::Discuss:
    {
        const FName Key=ReactionState(Athlete);
        if(!Key.IsNone())return Key;
        break;
    }
    case EC26Action::SignalFour:return TEXT("SignalFour");
    case EC26Action::SignalSix:return TEXT("SignalSix");
    case EC26Action::SignalOut:return TEXT("SignalOut");
    case EC26Action::SignalWide:return TEXT("SignalWide");
    default:break;
    }
    return IdleState(Athlete,Dt);
}
FName UC26CharacterPresentationComponent::ReactionState(const AC26Athlete* Athlete)
{
    const bool Batter=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker;
    FName Cue=Athlete->Reaction;
    FName Key=C26Character::ReactionKey(Cue,VisualRole,Appearance.AnimationStyle,Appearance.LeftHandedBat);
    if(Cue.IsNone())
    {
        // Uncued base actions keep their pre-Round 7 clips; the ceremonial ones map onto the reaction set.
        switch(Athlete->Action)
        {
        case EC26Action::Celebrate:Key=Batter?(Appearance.LeftHandedBat?TEXT("BatterCelebrate_L"):TEXT("BatterCelebrate_R")):TEXT("Celebrate");break;
        case EC26Action::Disappointed:Key=TEXT("Disappointed");break;
        case EC26Action::BatRaise:Key=C26Character::ReactionKey(TEXT("Milestone"),VisualRole,Appearance.AnimationStyle,Appearance.LeftHandedBat);break;
        case EC26Action::FistPump:Key=Batter?NAME_None:FName(TEXT("CelebrateEnergetic"));break;
        case EC26Action::GloveTap:Key=C26Character::ReactionKey(TEXT("Support"),VisualRole,Appearance.AnimationStyle,Appearance.LeftHandedBat);break;
        default:break;
        }
    }
    else if(!Key.IsNone()&&!Profile->FindClip(Key))
    {
        if(!ReportedMissing.Contains(Key)){ReportedMissing.Add(Key);UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_MISSING_CLIP %s"),*Key.ToString());}
        Key=NAME_None;
    }
    if(Key.IsNone()||!Profile->FindClip(Key))return NAME_None;
    // Each athlete takes his own beat before reacting (distance to the event plus temperament), and plays
    // the clip at his own pace; until then he holds his ready state.
    const float T=(Athlete->ActionTime-Athlete->ReactionDelay-(Cue.IsNone()?0.f:StyleDelay))*(Cue.IsNone()?1.f:StyleRate);
    if(T<0.f)return NAME_None;
    ReactionClock=T;
    return Key;
}
FName UC26CharacterPresentationComponent::IdleState(const AC26Athlete* Athlete,float Dt)
{
    const bool Batter=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker;
    const bool Moving=Locomotion.GroundSpeed>12.f;
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
        if(Clip&&TransitionAge<Clip->Sequence->GetPlayLength())return Transition;
        Transition=NAME_None;
    }
    if(Moving)
    {
        if(Batter)return Appearance.LeftHandedBat?TEXT("BatterRun_L"):TEXT("BatterRun_R");
        if(VisualRole==EC26VisualRole::Keeper)
        {
            // Square-on lateral shuffle: the athlete's left is -Right. Running up to the stumps for a return is
            // forward travel and uses the run cycle; a sideways shuffle there skates.
            const float Side=FVector::DotProduct(Locomotion.Velocity,Athlete->GetActorRightVector());
            const FName Shuffle=Side<0?TEXT("KeeperShuffle_L"):TEXT("KeeperShuffle_R");
            if(FMath::Abs(Side)>FMath::Abs(FVector::DotProduct(Locomotion.Velocity,Athlete->GetActorForwardVector()))&&Profile->FindClip(Shuffle))return Shuffle;
        }
        if(VisualRole==EC26VisualRole::Umpire&&Profile->FindClip(TEXT("UmpireWalk")))return TEXT("UmpireWalk");
        return Locomotion.GroundSpeed<180.f?TEXT("Walk"):TEXT("Run");
    }
    return ReadyKey();
}
void UC26CharacterPresentationComponent::ResetMotion()
{
    Locomotion.Reset(GetOwner()->GetActorTransform());Clock=BlendClock=0;Transition=NAME_None;bWasMoving=false;
    CurrentClip=nullptr;CurrentState=NAME_None;FrozenSeconds=0;LatchedKey=NAME_None;
    bRecovered=false;ReactionClock=-1.f;Lean=LeanVelocity=FVector2D::ZeroVector;SmoothedAcceleration=FVector::ZeroVector;
    if(Body)Body->SetRelativeLocation(FVector::ZeroVector);
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
    const float PreviousSpeed=Locomotion.GroundSpeed;
    Locomotion.Update(Athlete->GetActorTransform(),Dt);
    // The match stops the athlete on the frame an action starts, so the approach speed is the
    // speed measured before that frame.
    if(Athlete->Action!=LatchedAction)
    {
        // A dive that ends in the gather is one motion: the match switches Dive -> Pickup/Catch on the
        // take, and the dive clip already carries that Pickup/Catch event at the moment the hands arrive.
        const bool DiveTake=LatchedAction==EC26Action::Dive&&LatchedKey.ToString().StartsWith(TEXT("Dive"))
            &&(Athlete->Action==EC26Action::Pickup||Athlete->Action==EC26Action::Catch);
        LatchedAction=Athlete->Action;EntrySpeed=PreviousSpeed;
        if(!DiveTake)LatchedKey=NAME_None;
        bRecovered=false;
    }
    // The same action restarted (a fresh stroke, a new reaction) is a new one-shot too.
    if(Athlete->ActionTime+1e-3f<PreviousActionTime)bRecovered=false;
    PreviousActionTime=Athlete->ActionTime;
    Appearance.LeftArmBowl=Athlete->LeftArmBowl;
    // The batting hand can be switched between balls: re-seat the bat on the other grip and re-measure reach.
    if(Appearance.LeftHandedBat!=Athlete->LeftHandedBat)
    {
        Appearance.LeftHandedBat=Athlete->LeftHandedBat;RefreshEquipmentAttachments();ReachClip=nullptr;CurrentClip=nullptr;
    }
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
            // Edge labels (TOP EDGE etc.) have no stroke of their own: a block is the honest pose.
            Fallback=FName(*(TEXT("FRONTFOOTDEFENCE")+Hand));
        }
        State=Fallback;Clip=Profile->FindClip(State);
    }
    if(!Clip||!Clip->Sequence)return; // Activation validation guarantees all fallback sequences.
    // Played-out one-shots recover to the ready loop: strokes and deliveries (after their authored
    // follow-through and recovery), reactions and umpire signals. Gathers, throws and dives stay under the
    // match's own clock. A dismissed batter keeps his final pose.
    const EC26Action Act=Athlete->Action;
    const bool Recoverable=Act==EC26Action::Batting||Act==EC26Action::Bowling||ReactionClock>=0.f
        ||Act==EC26Action::SignalFour||Act==EC26Action::SignalSix||Act==EC26Action::SignalOut||Act==EC26Action::SignalWide;
    if(!bRecovered&&Recoverable&&!Clip->Loop&&State!=Transition&&!C26Character::HoldsFinalPose(State))
    {
        const float Length=Clip->Sequence->GetPlayLength();
        float Raw=ReactionClock>=0.f?ReactionClock:Athlete->ActionTime;
        if(ReactionClock<0.f&&!Clip->Event.IsNone())
        {
            const float MatchEvent=Clip->Event==TEXT("BatContact")?C26Field::BatContactPoseTime:
                Clip->Event==TEXT("BallRelease")?C26Field::ReleasePoseTime:Clip->Event==TEXT("ThrowRelease")?.2f:.18f;
            const float ClipEvent=Clip->EventTime();
            if(MatchEvent>0.f&&ClipEvent>=0.f)Raw=Athlete->ActionTime<=MatchEvent?Athlete->ActionTime/MatchEvent*ClipEvent:ClipEvent+Athlete->ActionTime-MatchEvent;
        }
        if(Raw>=Length&&Dt>0.f)
        {
            bRecovered=true;
            const FName Idle=IdleState(Athlete,0.f);
            if(const auto* IdleClip=Profile->FindClip(Idle)){State=Idle;Clip=IdleClip;ReactionClock=-1.f;}
        }
    }
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());if(!Anim)return;
    if(CurrentClip!=Clip)
    {
        PreviousState=CurrentState.IsNone()?State:CurrentState;
        Anim->PreviousSequence=Anim->CurrentSequence?Anim->CurrentSequence:Clip->Sequence;
        Anim->PreviousTime=Anim->CurrentTime;CurrentClip=Clip;CurrentState=State;BlendClock=0;
        // Entering or leaving a reaction, or settling out of a played-out one-shot, is a body shift rather
        // than a technique change: it gets a longer, softer blend than a stroke or gather entry.
        ActiveBlend=(ReactionClock>=0.f||bRecovered)?.32f:Clip->BlendSeconds;
        UpdateWarp(Athlete,Clip);
    }
    // The match poses event-driven athletes (pickup, catch, throw, keeper take) with Dt==0 and drives
    // ActionTime itself, so the blend must advance on action time too. Advancing on Dt alone left the
    // blend at zero for the whole gather: the previous run pose was rendered and the ball met it.
    BlendClock+=Dt>0.f?Dt:FMath::Clamp(Athlete->ActionTime-LastActionTime,0.f,.1f);
    LastActionTime=Athlete->ActionTime;
    float Time=Athlete->ActionTime;
    if(ReactionClock>=0.f)Time=FMath::Min(ReactionClock,Clip->Sequence->GetPlayLength());
    else if(State==Transition)Time=TransitionAge;
    else if(Clip->Loop)
    {
        const bool Move=State==TEXT("Run")||State==TEXT("Walk")||State==TEXT("BatterRun_R")||State==TEXT("BatterRun_L")||State==TEXT("KeeperShuffle_L")||State==TEXT("KeeperShuffle_R")||State==TEXT("UmpireWalk");
        Time=Move?Locomotion.Distance/FMath::Max(1.f,Clip->GroundSpeed):Clock;
        Time=FMath::Fmod(Time,Clip->Sequence->GetPlayLength());
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
    Anim->BlendAlpha=FMath::SmoothStep(0.f,FMath::Max(.02f,ActiveBlend),BlendClock);
    // An exact authoritative contact sample must render that event pose this frame.
    if(Dt==0.f&&!Clip->Event.IsNone()&&FMath::Abs(Time-Clip->EventTime())<=1.f/30.f)Anim->BlendAlpha=1.f;
    Anim->GroundSpeed=Locomotion.GroundSpeed;Anim->MovementDirection=Locomotion.Direction;
    Anim->Acceleration=Locomotion.Acceleration;Anim->TurnRate=Locomotion.TurnRate;Anim->State=State;
    UpdateBatControl(Athlete);
    UpdateLife(Athlete,Dt);
    // Pose evaluation is the per-athlete cost. A distant athlete holding a ready loop or running (LOD 2+, i.e.
    // beyond 30 m of the lens) is re-posed every second or third frame, staggered across the squad; the clip
    // time is still advanced every frame, so nothing drifts. Principals, one-shots, reactions, transitions and
    // every event pose (Dt==0) evaluate every frame.
    const bool Principal=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::Bowler||VisualRole==EC26VisualRole::Keeper;
    const int32 Interval=(Dt>0.f&&!Principal&&Clip->Loop&&ReactionClock<0.f&&State!=Transition&&BlendClock>=ActiveBlend)?(QualityTier>=3?3:QualityTier>=2?2:1):1;
    if(Interval>1&&(++EvaluationCounter%Interval)!=0)return;
    Body->TickAnimation(FMath::Max(0.f,Dt),false);Body->RefreshBoneTransforms();
    LearnWarp(Athlete);
    Debug(Athlete,Dt);
}
void UC26CharacterPresentationComponent::UpdateLife(const AC26Athlete* Athlete,float Dt)
{
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());if(!Anim)return;
    const EC26Action Act=Athlete->Action;
    // Gameplay-owned poses carry the contact, release, gather and throw events: no procedural layer at all,
    // so hand, bat and ball alignment and foot locking are exactly what the authored clip delivers.
    const bool Gameplay=!bRecovered&&(Act==EC26Action::Batting||Act==EC26Action::Bowling||Act==EC26Action::Pickup
        ||Act==EC26Action::Throw||Act==EC26Action::Catch||Act==EC26Action::Dive||Act==EC26Action::Slide);
    const bool Batter=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker;
    const bool Moving=Locomotion.GroundSpeed>12.f;
    const bool Reacting=ReactionClock>=0.f;
    float Breath=1,Sway=1,LookW=1,LeanW=1;
    if(Gameplay)Breath=Sway=LookW=LeanW=0;
    else if(Reacting){Breath=.5f;Sway=0;LookW=0;LeanW=.5f;}
    else if(Moving){Breath=.35f;Sway=0;LookW=.45f;LeanW=1;}
    // The striker's head is already authored onto the bowler; the keeper watches from his crouch.
    if(VisualRole==EC26VisualRole::Batter&&!Reacting&&!Moving)LookW=0;
    if(VisualRole==EC26VisualRole::Keeper)LookW*=.5f;
    if(VisualRole==EC26VisualRole::Umpire)Sway*=.5f;
    const float Step=Dt>0.f?1.f-FMath::Exp(-Dt*(Gameplay?18.f:3.f)):0.f;
    // The exact event frames are posed with Dt==0: the layer must already be gone there.
    const bool Hard=Gameplay&&Dt<=0.f;
    const auto Approach=[&](float& W,float Target){W=Hard?0.f:W+(Target-W)*Step;};
    Approach(BreathWeight,Breath*StyleLife);Approach(SwayWeight,Sway*StyleLife);Approach(LookWeight,LookW);Approach(LeanWeight,LeanW);
    if(Dt>0.f)
    {
        // Breathing deepens and quickens after a sprint and settles back over a few seconds.
        const float Exertion=FMath::Clamp(Locomotion.GroundSpeed/450.f,0.f,1.f);
        BreathPhase=FMath::Fmod(BreathPhase+Dt*UE_TWO_PI*BreathRate*(1.f+.8f*Exertion),UE_TWO_PI);
        SwayPhase=FMath::Fmod(SwayPhase+Dt*UE_TWO_PI*.11f,UE_TWO_PI);
        // Head and eyes to the match's look target, limited to what a neck does without turning the chest.
        FVector2D Want=FVector2D::ZeroVector;
        if(!Athlete->LookAt.IsZero())
        {
            const FVector Local=Athlete->GetActorTransform().InverseTransformPosition(Athlete->LookAt);
            Want.X=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)),-50.f,50.f);
            Want.Y=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Local.Z-160.f,FVector2D(Local.X,Local.Y).Size())),-14.f,12.f);
            if(FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)))>120.f)Want=FVector2D::ZeroVector; // behind: don't owl
        }
        Look+=(Want-Look)*(1.f-FMath::Exp(-Dt*StyleLookSpeed));
        // Follow-through inertia: the trunk lags a change of speed and leans into a turn, on a lightly
        // under-damped spring so a stop settles instead of snapping upright.
        SmoothedAcceleration+=(Locomotion.Acceleration-SmoothedAcceleration)*(1.f-FMath::Exp(-Dt*8.f));
        const FVector Fwd=Athlete->GetActorForwardVector();
        const float Along=FVector::DotProduct(SmoothedAcceleration,Fwd);
        const float Centripetal=Locomotion.GroundSpeed*FMath::DegreesToRadians(Locomotion.TurnRate);
        const FVector2D Target(FMath::Clamp(-Along*.004f,-4.f,5.f),FMath::Clamp(Centripetal*.0025f,-4.f,4.f));
        const float Stiff=190.f,Damp=15.f,H=FMath::Min(Dt,.05f);
        LeanVelocity+=((Target-Lean)*Stiff-LeanVelocity*Damp)*H;Lean+=LeanVelocity*H;
    }
    FC26SecondaryMotion& Life=Anim->Life;
    Life.Breath=BreathWeight;Life.Sway=SwayWeight;Life.BreathPhase=BreathPhase;Life.SwayPhase=SwayPhase+(Batter?1.3f:0.f);
    Life.BreathDegrees=Batter?.6f:.9f;Life.SwayDegrees=Batter?.4f:.7f;
    Life.LookYaw=Look.X*LookWeight;Life.LookPitch=Look.Y*LookWeight;
    Life.LeanPitch=Lean.X*LeanWeight;Life.LeanRoll=Lean.Y*LeanWeight;
    if(Hard||(Life.Breath<1e-3f&&Life.Sway<1e-3f&&LookWeight<1e-3f&&LeanWeight<1e-3f))Life=FC26SecondaryMotion();
}
void UC26CharacterPresentationComponent::LearnWarp(const AC26Athlete* Athlete)
{
    // Pure measurement (no visual shifting): on the frame the match crosses a clip's event, log the
    // rendered hands against the simulation so hand/ball sync and handedness are read, not assumed.
    if(!CurrentClip||CurrentClip->Event.IsNone())return;
    const FName Event=CurrentClip->Event;
    const float Tc=Event==TEXT("BatContact")?C26Field::BatContactPoseTime:Event==TEXT("BallRelease")?C26Field::ReleasePoseTime:
        Event==TEXT("ThrowRelease")?.2f:.18f;
    const bool Crossed=(WarpPrevTime<Tc&&Athlete->ActionTime>=Tc)||(Athlete->ActionTime==Tc&&Tc>0.f);
    WarpPrevTime=Athlete->ActionTime;
    if(!Crossed)return;
    if(Event==TEXT("BallRelease")||Event==TEXT("ThrowRelease"))
    {
        const bool Left=Event==TEXT("BallRelease")&&Appearance.LeftArmBowl;
        const FVector Ball=Body->GetSocketLocation(Left?Profile->LeftHandSocket:Profile->RightHandSocket);
        const FVector Other=Body->GetSocketLocation(Left?Profile->RightHandSocket:Profile->LeftHandSocket);
        const FVector Head=Body->GetBoneLocation(TEXT("head"));
        UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_RELEASE clip=%s arm=%s ballHandAboveHead=%.1fcm ballHandAboveOther=%.1fcm hand=%s"),
            *CurrentState.ToString(),Left?TEXT("L"):TEXT("R"),Ball.Z-Head.Z,Ball.Z-Other.Z,*Ball.ToString());
        return;
    }
    if(Athlete->ContactTarget.IsZero())return;
    const FVector Miss=Athlete->ContactTarget-ReceivePosition();
    UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_CONTACT clip=%s event=%s hands=%s simContact=%s miss=%s (|%.1fcm|)"),
        *CurrentState.ToString(),*Event.ToString(),*ReceivePosition().ToString(),*Athlete->ContactTarget.ToString(),
        *Miss.ToString(),Miss.Size());
}
void UC26CharacterPresentationComponent::UpdateBatControl(const AC26Athlete* Athlete)
{
    auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());if(!Anim)return;
    FC26BatControl& Control=Anim->BatControl;
    const UStaticMeshComponent* Bat=GetBat();
    bool Enabled=Bat&&Bat->IsVisible()&&Bat->GetStaticMesh();
#if !UE_BUILD_SHIPPING
    Enabled&=!FParse::Param(FCommandLine::Get(),TEXT("C26NoBatControl"));
#endif
    Control.bGripLock=Enabled;Control.bLeftHandTop=!Appearance.LeftHandedBat;
    // A play-and-miss is shown as one: only a stroke that will meet the ball (middle or edge) is aimed at it.
    const bool Stroke=Enabled&&Athlete->Action==EC26Action::Batting&&CurrentClip&&CurrentClip->Sequence
        &&CurrentClip->Event==TEXT("BatContact")&&!Athlete->ContactTarget.IsZero()&&Athlete->ExpectedTiming!=EC26Timing::Miss;
    if(!Stroke)
    {
        // Outside a stroke the last reach is kept, gated to its own clip, so the replay of that ball shows it.
        if(!Enabled||Athlete->Action==EC26Action::Batting){Control.ReachSequence=nullptr;ReachClip=nullptr;}
        return;
    }
    if(ReachClip==CurrentClip&&ReachTiming==uint8(Athlete->ExpectedTiming)&&FVector::DistSquared(ReachTarget,Athlete->ContactTarget)<4.f)return;
    ReachClip=CurrentClip;ReachTarget=Athlete->ContactTarget;ReachTiming=uint8(Athlete->ExpectedTiming);
    // Measure the blade in the clip's own authored contact pose (no blend, no life, no reach).
    const FTransform Blade=Bat->GetRelativeTransform()*PosedSocket(CurrentClip->Sequence,CurrentClip->EventTime(),Bat->GetAttachSocketName())
        *Body->GetComponentTransform();
    // Bat space: the handle is +Z and the blade runs below Z=-24 (the GoldenGate blade measurement uses the
    // same split). The wider horizontal extent is the face, the narrower one the thickness.
    const FBox Box=Bat->GetStaticMesh()->GetBoundingBox();
    const FVector Centre=Box.GetCenter(),Extent=Box.GetExtent();
    const int32 Face=Extent.X>=Extent.Y?0:1,Depth=1-Face;
    const FVector Local=Blade.InverseTransformPosition(ReachTarget);
    FVector Aim=Local;
    if(Athlete->ExpectedTiming==EC26Timing::Edge)Aim[Face]=Centre[Face]+(Local[Face]>=Centre[Face]?Extent[Face]:-Extent[Face]);
    else Aim[Face]=FMath::Clamp(Local[Face],Centre[Face]-Extent[Face]*.35f,Centre[Face]+Extent[Face]*.35f);
    Aim[Depth]=FMath::Clamp(Local[Depth],Centre[Depth]-Extent[Depth]*.5f,Centre[Depth]+Extent[Depth]*.5f);
    Aim.Z=FMath::Clamp(Local.Z,Box.Min.Z+10.f,-32.f);
    // A clip that is authored nowhere near the ball stays authored: reach is a correction, not a teleport.
    const FVector World=ReachTarget-Blade.TransformPosition(Aim);
    const FVector Applied=World.GetClampedToMaxSize(30.f);
    Control.ReachSequence=CurrentClip->Sequence;Control.ReachTime=CurrentClip->EventTime();
    Control.Reach=Body->GetComponentTransform().InverseTransformVector(Applied);
    UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_REACH id=%s clip=%s timing=%d need=%.1fcm applied=%.1fcm"),
        *Appearance.PlayerID.ToString(),*CurrentState.ToString(),int32(Athlete->ExpectedTiming),World.Size(),Applied.Size());
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
    const int32 Tier=Distance>6000.f?3:Distance>3000.f?2:Distance>1400.f?1:0;QualityTier=Tier;
    const int32 Bias=Quality==EC26CharacterQuality::Low?1:0;
    int32 Lod=FMath::Clamp(Tier+Bias,0,FMath::Max(0,Count-1));
#if !UE_BUILD_SHIPPING
    FParse::Value(FCommandLine::Get(),TEXT("C26CharacterLOD="),Lod);
#endif
    Body->SetForcedLOD(Lod+1);
    Body->UpdateLODStatus();
    const bool Shadow=Active||Distance<4000.f;
    Body->SetCastShadow(Shadow);
    // Gear shadows go with the body's: a cap or pad shadow without the athlete's is both wrong and wasted.
    for(const auto& Pair:Equipment)if(Pair.Value)Pair.Value->SetCastShadow(Shadow);
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
    // Two-handed grip, measured on what renders: the top hand stays above the bottom hand on the handle and
    // neither hand leaves the handle axis (same thresholds as the -C26ShotReview gate, here in live play).
    // Reactions may take a hand off the bat on purpose (BatterBeaten, BatterAcknowledge), so only technique is checked.
    const bool TwoHanded=Athlete->Action==EC26Action::Batting||CurrentState==ReadyKey();
    if(const UStaticMeshComponent* Bat=GetBat();TwoHanded&&Bat&&Bat->IsVisible()&&(VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker))
    {
        const auto Measure=[&](float& Order,float& Off)
        {
            const FTransform Grip=Bat->GetRelativeTransform()*Body->GetSocketTransform(Bat->GetAttachSocketName());
            const FVector Origin=Grip.GetLocation(),Axis=Grip.GetUnitAxis(EAxis::Z);
            const FVector Top=Body->GetBoneLocation(Appearance.LeftHandedBat?TEXT("hand_r"):TEXT("hand_l"));
            const FVector Bottom=Body->GetBoneLocation(Appearance.LeftHandedBat?TEXT("hand_l"):TEXT("hand_r"));
            const auto OffAxis=[&](const FVector& H){const FVector Rel=H-Origin;return (Rel-Axis*FVector::DotProduct(Rel,Axis)).Size();};
            Order=FVector::DotProduct(Top-Bottom,Axis);Off=FMath::Max(OffAxis(Top),OffAxis(Bottom));
        };
        float Order=0,Off=0;Measure(Order,Off);
        if((Order<3.f||Off>10.f)&&Clock-GripLastReport>.25f)
        {
            GripLastReport=Clock;
            auto* Anim=Cast<UC26CricketerAnimInstance>(Body->GetAnimInstance());
            const float Alpha=Anim->BlendAlpha,Time=Anim->CurrentTime;
            // Re-pose the current clip alone at the same time: tells an authored defect from a runtime one.
            const TObjectPtr<UAnimSequence> SavedPrevious=Anim->PreviousSequence;const float SavedPreviousTime=Anim->PreviousTime;
            const FC26SecondaryMotion SavedLife=Anim->Life;const FC26BatControl SavedControl=Anim->BatControl;
            Anim->PreviousSequence=Anim->CurrentSequence;Anim->PreviousTime=Time;Anim->BlendAlpha=1;Anim->Life=FC26SecondaryMotion();Anim->BatControl=FC26BatControl();
            Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
            float PureOrder=0,PureOff=0;Measure(PureOrder,PureOff);
            Anim->PreviousSequence=SavedPrevious;Anim->PreviousTime=SavedPreviousTime;Anim->BlendAlpha=Alpha;Anim->Life=SavedLife;Anim->BatControl=SavedControl;
            Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
            UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_GRIP_FAIL id=%s state=%s prev=%s alpha=%.2f clip=%.3f order=%.1fcm offaxis=%.1fcm pure_order=%.1fcm pure_offaxis=%.1fcm lod=%d"),
                *Appearance.PlayerID.ToString(),*CurrentState.ToString(),*PreviousState.ToString(),Alpha,Time,Order,Off,PureOrder,PureOff,Body->GetPredictedLODLevel());
        }
    }
    if(C26CharacterDebug.GetValueOnGameThread())
    {
        const FString Text=FString::Printf(TEXT("%s Role:%d %s\n%s LOD:%d Speed:%.0f Dir:%.0f\nAnim:%s Gear:%d RootMotion:off RuntimeIK:off"),
            *Appearance.PlayerID.ToString(),int32(VisualRole),*CurrentState.ToString(),*GetNameSafe(Body->GetSkeletalMeshAsset()),Body->GetPredictedLODLevel(),
            Locomotion.GroundSpeed,Locomotion.Direction,*GetNameSafe(Body->GetAnimClass()),Equipment.Num());
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
    Anim->PreviousTime=Frame.PreviousTime;Anim->BlendAlpha=Frame.Alpha;Anim->Life=FC26SecondaryMotion();
    CurrentState=Frame.State;PreviousState=Frame.PreviousState;CurrentClip=Clip;
    Clock=FMath::Lerp(A.Clock,B.Clock,Alpha);BlendClock=Frame.Alpha*Clip->BlendSeconds;
    Locomotion.Reset(GetOwner()->GetActorTransform());Locomotion.Distance=FMath::Lerp(A.Distance,B.Distance,Alpha);
    Locomotion.GroundSpeed=FMath::Lerp(A.GroundSpeed,B.GroundSpeed,Alpha);bWasMoving=Locomotion.GroundSpeed>12.f;
    Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
}
