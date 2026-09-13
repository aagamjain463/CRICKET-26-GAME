#include "C26CharacterPresentationComponent.h"
#include "C26CricketerAnimInstance.h"
#include "SuperOver/C26Athlete.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
UC26CharacterPresentationComponent::UC26CharacterPresentationComponent(){PrimaryComponentTick.bCanEverTick=false;}
bool UC26CharacterPresentationComponent::TryActivate(AC26Athlete* Athlete)
{
    if(bActive)return true;if(!Athlete)return false;
    FString Path=TEXT("/Game/Cricket26/Characters/Data/DA_C26_DefaultPlayer.DA_C26_DefaultPlayer");
    bool Slice=false;
#if !UE_BUILD_SHIPPING
    Slice=FParse::Param(FCommandLine::Get(),TEXT("C26CharacterSlice"));
    FParse::Value(FCommandLine::Get(),TEXT("C26CharacterProfile="),Path);
#endif
    static TSet<FString> Rejected;
    if(Rejected.Contains(Path))return false;
    Profile=LoadObject<UC26CharacterProfile>(nullptr,*Path);
    TArray<FString> Errors;
    if(!Profile)Errors.Add(TEXT("No complete approved character profile at ")+Path);
    else Profile->Validate(Errors,!Slice);
    if(!Errors.IsEmpty())
    {
        if(!Rejected.Contains(Path))
        {
            Rejected.Add(Path);
            UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_MIGRATION_BLOCKED %s (%d errors). Existing match preserved."),*Path,Errors.Num());
            for(const FString& Error:Errors)UE_LOG(LogTemp,Warning,TEXT("C26_CHARACTER_ASSET_GATE %s"),*Error);
        }
        return false;
    }
    // Slice previews one representative of each role in the real match before mass migration.
    if(Slice)
    {
        const bool Representative=Athlete->Role==EC26Role::Bowler||Athlete->Role==EC26Role::Keeper
            ||Athlete->Role==EC26Role::Umpire||(Athlete->Role==EC26Role::Fielder&&Athlete->SquadNumber==3)
            ||(Athlete->Role==EC26Role::Batter&&Athlete->SquadNumber==7);
        if(!Representative)return false;
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
    UE_LOG(LogTemp,Display,TEXT("C26_CHARACTER_ACTIVE id=%s role=%d body=%s skeleton=%s anim=%s"),
        *Appearance.PlayerID.ToString(),int32(VisualRole),*Profile->Body->GetName(),*GetNameSafe(Profile->Skeleton),*GetNameSafe(Body->GetAnimClass()));
    return true;
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
    Appearance.PlayerID=FName(*FString::Printf(TEXT("Team%d_Player%d"),Athlete->TeamId,Athlete->SquadNumber));
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
    if(Role!=EC26VisualRole::Umpire&&Profile->TeamMaterials.IsValidIndex(Athlete->TeamId))
    {
        const int32 Slot=Body->GetMaterialIndex(Profile->JerseyMaterialSlot);
        if(Slot>=0)Body->SetMaterial(Slot,Profile->TeamMaterials[Athlete->TeamId]);
    }
}
void UC26CharacterPresentationComponent::ApplyVisualRole(EC26VisualRole Role)
{
    if(!bActive||uint8(Role)>uint8(EC26VisualRole::Umpire))return;
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
FName UC26CharacterPresentationComponent::SelectState(const AC26Athlete* Athlete,float Dt)
{
    const bool Batter=VisualRole==EC26VisualRole::Batter||VisualRole==EC26VisualRole::NonStriker;
    switch(Athlete->Action)
    {
    case EC26Action::Batting:return C26Character::ShotKey(Athlete->ShotLabel,Appearance.LeftHandedBat);
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
    Locomotion.Update(Athlete->GetActorTransform(),Dt);Clock+=FMath::Max(0.f,Dt);
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
    }
    BlendClock+=FMath::Max(0.f,Dt);
    float Time=Athlete->ActionTime;
    if(State==Transition)Time=TransitionAge;
    else if(Clip->Loop)
    {
        const bool Move=State==TEXT("Run")||State==TEXT("Walk")||State==TEXT("BatterRun_R")||State==TEXT("BatterRun_L")||State==TEXT("KeeperShuffle")||State==TEXT("UmpireWalk");
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
    Anim->BlendAlpha=FMath::SmoothStep(0.f,FMath::Max(.02f,Clip->BlendSeconds),BlendClock);
    // An exact authoritative contact sample must render that event pose this frame.
    if(Dt==0.f&&!Clip->Event.IsNone()&&FMath::IsNearlyEqual(Time,Clip->EventTime(),.001f))Anim->BlendAlpha=1.f;
    Anim->GroundSpeed=Locomotion.GroundSpeed;Anim->MovementDirection=Locomotion.Direction;
    Anim->Acceleration=Locomotion.Acceleration;Anim->TurnRate=Locomotion.TurnRate;Anim->State=State;
    Body->TickAnimation(FMath::Max(0.f,Dt),false);Body->RefreshBoneTransforms();
    Debug(Athlete,Dt);
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
        const FString Text=FString::Printf(TEXT("%s Role:%d %s\n%s LOD:%d Speed:%.0f Dir:%.0f\nAnim:%s Gear:%d RootMotion:off IK:authored"),
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
    Anim->PreviousTime=Frame.PreviousTime;Anim->BlendAlpha=Frame.Alpha;
    CurrentState=Frame.State;PreviousState=Frame.PreviousState;CurrentClip=Clip;
    Clock=FMath::Lerp(A.Clock,B.Clock,Alpha);BlendClock=Frame.Alpha*Clip->BlendSeconds;
    Locomotion.Reset(GetOwner()->GetActorTransform());Locomotion.Distance=FMath::Lerp(A.Distance,B.Distance,Alpha);
    Locomotion.GroundSpeed=FMath::Lerp(A.GroundSpeed,B.GroundSpeed,Alpha);bWasMoving=Locomotion.GroundSpeed>12.f;
    Body->TickAnimation(0.f,false);Body->RefreshBoneTransforms();
}
