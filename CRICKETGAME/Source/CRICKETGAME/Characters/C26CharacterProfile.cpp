#include "C26CharacterProfile.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Materials/MaterialInterface.h"

namespace C26Character
{
bool Allows(EC26VisualRole Role, EC26EquipmentSlot Slot)
{
    if(uint8(Role)>uint8(EC26VisualRole::Umpire))return false;
    const bool Batter=Role==EC26VisualRole::Batter||Role==EC26VisualRole::NonStriker;
    switch(Slot)
    {
    case EC26EquipmentSlot::Helmet:return Batter||Role==EC26VisualRole::Keeper;
    case EC26EquipmentSlot::Headwear:return !Batter;
    case EC26EquipmentSlot::BattingPadL:case EC26EquipmentSlot::BattingPadR:
    case EC26EquipmentSlot::BattingGloveL:case EC26EquipmentSlot::BattingGloveR:
    case EC26EquipmentSlot::Bat:return Batter;
    case EC26EquipmentSlot::KeeperPadL:case EC26EquipmentSlot::KeeperPadR:
    case EC26EquipmentSlot::KeeperGloveL:case EC26EquipmentSlot::KeeperGloveR:return Role==EC26VisualRole::Keeper;
    case EC26EquipmentSlot::Accessory:return Role!=EC26VisualRole::Umpire;
    // This component never makes a second ball. The simulation's ball follows the hand socket.
    case EC26EquipmentSlot::Ball:return false;
    default:return false;
    }
}
bool Requires(EC26VisualRole Role, EC26EquipmentSlot Slot)
{
    return Allows(Role,Slot)&&Slot!=EC26EquipmentSlot::Headwear&&Slot!=EC26EquipmentSlot::Accessory
        &&!(Slot==EC26EquipmentSlot::Helmet&&Role==EC26VisualRole::Keeper);
}
const TArray<FString>& ShotClips()
{
    static const TArray<FString> Clips={TEXT("STRAIGHTDRIVE"),TEXT("COVERDRIVE"),TEXT("ONDRIVE"),TEXT("FRONTFOOTDEFENCE"),
        TEXT("LOFTEDDRIVE"),TEXT("BACKFOOTDEFENCE"),TEXT("BACKFOOTPUNCH"),TEXT("SQUARECUT"),TEXT("PULL"),TEXT("HOOK"),
        TEXT("SWEEP"),TEXT("SLOGSWEEP"),TEXT("LOFTEDSTRAIGHT"),TEXT("LEGGLANCE")};
    return Clips;
}
FName ShotKey(const FString& Label,bool Left)
{
    FString Key=Label.ToUpper();Key.ReplaceInline(TEXT(" "),TEXT(""));Key.ReplaceInline(TEXT("-"),TEXT(""));
    // Every label C26Controls::ShotFamily can print resolves to a stroke that is biomechanically that
    // family, so the simulation's named shot is the one the batter visibly plays.
    static const TMap<FString,FString> Alias={
        {TEXT("DEFENSIVEPUSH"),TEXT("FRONTFOOTDEFENCE")},{TEXT("YORKERBLOCK"),TEXT("FRONTFOOTDEFENCE")},
        {TEXT("DUGOUTDRIVE"),TEXT("STRAIGHTDRIVE")},{TEXT("EXTRACOVERDRIVE"),TEXT("COVERDRIVE")},
        {TEXT("LOFTEDCOVERDRIVE"),TEXT("LOFTEDDRIVE")},{TEXT("LOFTEDSTRAIGHTDRIVE"),TEXT("LOFTEDSTRAIGHT")},
        {TEXT("LEGSIDEPICKUP"),TEXT("LOFTEDSTRAIGHT")},{TEXT("UPPERCUT"),TEXT("SQUARECUT")},
        {TEXT("LATECUT"),TEXT("SQUARECUT")},{TEXT("FLICK"),TEXT("LEGGLANCE")}};
    if(const FString* Clip=Alias.Find(Key))Key=*Clip;
    return FName(*(Key+(Left?TEXT("_L"):TEXT("_R"))));
}
FName BowlingKey(EC26Delivery Delivery,bool Left)
{
    const bool Wrist=Delivery==EC26Delivery::LegBreak||Delivery==EC26Delivery::Googly||Delivery==EC26Delivery::Flipper;
    const bool Finger=Delivery==EC26Delivery::OffBreak||Delivery==EC26Delivery::ArmBall||Delivery==EC26Delivery::TopSpinner||Delivery==EC26Delivery::Doosra;
    return FName(*(FString(Wrist?TEXT("LegSpin"):Finger?TEXT("OffSpin"):TEXT("FastBowl"))+(Left?TEXT("_L"):TEXT("_R"))));
}
FName Temperament(int32 Team,int32 SquadNumber)
{
    static const FName Styles[3]={TEXT("Calm"),TEXT("Aggressive"),TEXT("Energetic")};
    return Styles[uint32(Team*7+SquadNumber*5+1)%3];
}
FName ReactionKey(FName Cue,EC26VisualRole Role,FName Style,bool LeftHanded)
{
    if(Cue.IsNone()||Role==EC26VisualRole::Umpire)return NAME_None;
    const bool Calm=Style==TEXT("Calm"),Energetic=Style==TEXT("Energetic");
    if(Role==EC26VisualRole::Batter||Role==EC26VisualRole::NonStriker)
    {
        const auto Hand=[LeftHanded](const TCHAR* Base){return FName(*(FString(Base)+(LeftHanded?TEXT("_L"):TEXT("_R"))));};
        if(Cue==TEXT("Boundary")||Cue==TEXT("Support"))return Hand(TEXT("BatterAcknowledge"));
        if(Cue==TEXT("Six"))return Calm?Hand(TEXT("BatterAcknowledge")):Hand(TEXT("BatterCelebrate"));
        if(Cue==TEXT("Milestone"))return Hand(TEXT("BatterCelebrate"));
        if(Cue==TEXT("Dot"))return Hand(TEXT("BatterReset"));
        if(Cue==TEXT("PlayAndMiss")||Cue==TEXT("Beaten"))return Hand(TEXT("BatterBeaten"));
        if(Cue==TEXT("Edge"))return Hand(TEXT("BatterEdge"));
        if(Cue==TEXT("Dismissed"))return Hand(TEXT("BatterDismissed"));
        return NAME_None;
    }
    const bool Bowler=Role==EC26VisualRole::Bowler;
    if(Cue==TEXT("Wicket"))return Calm?TEXT("CelebrateRestrained"):(Energetic&&!Bowler)?TEXT("Celebrate"):TEXT("CelebrateEnergetic");
    if(Cue==TEXT("Catch"))return Calm?TEXT("Celebrate"):TEXT("CelebrateEnergetic");
    if(Cue==TEXT("Appeal"))return TEXT("Appeal");
    if(Cue==TEXT("NearMiss"))return Style==TEXT("Aggressive")?TEXT("Appeal"):TEXT("HandsOnHead");
    if(Cue==TEXT("Dropped"))return TEXT("HandsOnHead");
    if(Cue==TEXT("BoundaryConceded"))return Energetic?TEXT("HandsOnHead"):TEXT("Frustrated");
    if(Cue==TEXT("DotConfidence"))return Calm?NAME_None:FName(TEXT("Clap"));
    if(Cue==TEXT("Support")||Cue==TEXT("GoodStop"))return TEXT("Clap");
    return NAME_None;
}
bool HoldsFinalPose(FName Key)
{
    return Key==TEXT("BatterDismissed_R")||Key==TEXT("BatterDismissed_L");
}
float MapEventTime(float ActionTime,float MatchEventTime,float ClipEventTime,float ClipLength)
{
    if(MatchEventTime<=0.f||ClipEventTime<0.f)return FMath::Clamp(ActionTime,0.f,ClipLength);
    // Contact is exact; the follow-through retains the authored cadence instead of being crushed.
    return FMath::Clamp(ActionTime<=MatchEventTime?ActionTime/MatchEventTime*ClipEventTime:
        ClipEventTime+ActionTime-MatchEventTime,0.f,ClipLength);
}
}
float FC26CricketClip::EventTime() const
{
    if(Sequence&&!Event.IsNone())for(const FAnimNotifyEvent& Notify:Sequence->Notifies)
        if(Notify.NotifyName==Event)return Notify.GetTime();
    return -1.f;
}
const FC26CricketClip* UC26CharacterProfile::FindClip(FName Key,FName Fallback) const
{
    if(const auto* Clip=Clips.Find(Key))if(Clip->Sequence)return Clip;
    return Fallback.IsNone()?nullptr:FindClip(Fallback);
}
bool UC26CharacterProfile::AuditBody(USkeletalMesh* Candidate,USkeleton* Expected,TArray<FString>& Errors)
{
    const int32 Before=Errors.Num();
    if(!Candidate){Errors.Add(TEXT("Missing full-body SkeletalMesh"));return false;}
    const FString Name=Candidate->GetPathName();
    if(!Expected||Candidate->GetSkeleton()!=Expected)Errors.Add(Name+TEXT(": incompatible/missing canonical skeleton"));
    const float Height=Candidate->GetImportedBounds().BoxExtent.Z*2.f;
    if(Height<150.f||Height>210.f)Errors.Add(FString::Printf(TEXT("%s: height %.1fcm; import at real scale, not runtime .48 correction"),*Name,Height));
    const auto& Ref=Candidate->GetRefSkeleton();
    const TArray<FName> Required={TEXT("root"),TEXT("pelvis"),TEXT("spine_01"),TEXT("neck_01"),TEXT("head"),
        TEXT("clavicle_l"),TEXT("clavicle_r"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("lowerarm_l"),TEXT("lowerarm_r"),
        TEXT("hand_l"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r"),
        TEXT("thumb_01_l"),TEXT("thumb_01_r"),TEXT("index_01_l"),TEXT("index_01_r")};
    for(FName Bone:Required)if(Ref.FindBoneIndex(Bone)==INDEX_NONE)Errors.Add(Name+TEXT(": missing bone ")+Bone.ToString());
#if WITH_EDITOR
    // Skeleton/bounds alone are not proof of geometry. Inspect the actual vertices and skin
    // influences at EVERY imported/reduced LOD. Descendant weights count for an anatomical region.
    const FSkeletalMeshModel* Model=Candidate->GetImportedModel();
    if(!Model||Model->LODModels.IsEmpty())Errors.Add(Name+TEXT(": no inspectable LOD geometry"));
    else for(int32 Lod=0;Lod<Model->LODModels.Num();++Lod)
    {
        const auto& L=Model->LODModels[Lod];
        const TArray<FName> Regions={TEXT("head"),TEXT("pelvis"),TEXT("upperarm_l"),TEXT("upperarm_r"),
            TEXT("lowerarm_l"),TEXT("lowerarm_r"),TEXT("hand_l"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("thigh_r"),
            TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r")};
        TArray<int32> Counts;Counts.Init(0,Regions.Num());
        for(const auto& Section:L.Sections)if(!Section.bDisabled)
        for(const auto& Vertex:Section.SoftVertices)
        {
            for(int32 Region=0;Region<Regions.Num();++Region)
            {
                const int32 Root=Ref.FindBoneIndex(Regions[Region]);if(Root<0)continue;
                for(int32 Influence=0;Influence<MAX_TOTAL_INFLUENCES;++Influence)
                {
                    if(Vertex.InfluenceWeights[Influence]==0||!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Influence]))continue;
                    const int32 Bone=Section.BoneMap[Vertex.InfluenceBones[Influence]];
                    if(Bone==Root||Ref.BoneIsChildOf(Bone,Root)){++Counts[Region];break;}
                }
            }
        }
        for(int32 Region=0;Region<Regions.Num();++Region)if(Counts[Region]<4)
            Errors.Add(FString::Printf(TEXT("%s LOD%d: absent weighted geometry for %s (%d vertices)"),*Name,Lod,*Regions[Region].ToString(),Counts[Region]));
    }
#endif
    return Errors.Num()==Before;
}
FName FC26EquipmentDefinition::ResolveSocket(bool LeftHanded) const
{
    return LeftHanded&&!LeftHandedSocket.IsNone()?LeftHandedSocket:Socket;
}
const FTransform& FC26EquipmentDefinition::ResolveOffset(bool LeftHanded) const
{
    return LeftHanded&&!LeftHandedSocket.IsNone()?LeftHandedOffset:Offset;
}
USkeletalMesh* UC26CharacterProfile::ResolveBody(EC26VisualRole Role,FName Preset) const
{
    if(Role==EC26VisualRole::Umpire)return UmpireBody;
    if(const auto* Variant=BodyPresets.Find(Preset))return *Variant;
    return Body;
}
bool UC26CharacterProfile::ValidateMaterialOverrides(TArray<FString>& Errors) const
{
    const int32 Before=Errors.Num();
    TArray<USkeletalMesh*> Models={Body,UmpireBody};
    for(const auto& Variant:BodyPresets)Models.AddUnique(Variant.Value);
    for(const auto& Override:MaterialOverrides)
    {
        if(Override.Key.IsNone()||!Override.Value)
            Errors.Add(TEXT("Material override requires a slot name and material"));
        for(const USkeletalMesh* Model:Models)
        {
            if(Model&&!Model->GetMaterials().ContainsByPredicate([&Override](const FSkeletalMaterial& Material)
                {return Material.MaterialSlotName==Override.Key;}))
                Errors.Add(Model->GetName()+TEXT(": missing material override slot ")+Override.Key.ToString());
        }
    }
    return Errors.Num()==Before;
}
bool UC26CharacterProfile::Validate(TArray<FString>& Errors,bool RequireApproval) const
{
    if(RequireApproval&&(!ApprovedForMatch||VisualReviewEvidence.IsEmpty()))Errors.Add(TEXT("Gates A-F not approved: actual-match visual evidence required"));
    if(SourceAndLicense.IsEmpty())Errors.Add(TEXT("Missing asset provenance/license record"));
    AuditBody(Body,Skeleton,Errors);AuditBody(UmpireBody,Skeleton,Errors);
    for(const auto& Variant:BodyPresets)AuditBody(Variant.Value,Skeleton,Errors);
    ValidateMaterialOverrides(Errors);
    for(USkeletalMesh* Model:{Body.Get(),UmpireBody.Get()})if(Model)
        for(FName Socket:{LeftHandSocket,RightHandSocket})if(!Model->FindSocket(Socket))Errors.Add(Model->GetName()+TEXT(": missing ")+Socket.ToString());
    TSet<EC26EquipmentSlot> Slots;
    for(const auto& Item:Equipment)
    {
        if(Slots.Contains(Item.Slot))Errors.Add(TEXT("Duplicate equipment slot ")+StaticEnum<EC26EquipmentSlot>()->GetNameStringByValue(int64(Item.Slot)));
        Slots.Add(Item.Slot);
        if(Item.Slot==EC26EquipmentSlot::Ball)Errors.Add(TEXT("Ball must remain the existing match actor; no equipment ball allowed"));
        if(!Item.Mesh)Errors.Add(TEXT("Equipment mesh missing: ")+Item.Socket.ToString());
        if(Item.Socket.IsNone()||!Body||!Body->FindSocket(Item.Socket))Errors.Add(TEXT("Equipment socket missing: ")+Item.Socket.ToString());
        if(!Item.LeftHandedSocket.IsNone()&&(!Body||!Body->FindSocket(Item.LeftHandedSocket)))
            Errors.Add(TEXT("Left-handed equipment socket missing: ")+Item.LeftHandedSocket.ToString());
    }
    for(uint8 R=0;R<=uint8(EC26VisualRole::Umpire);++R)for(uint8 S=0;S<=uint8(EC26EquipmentSlot::Accessory);++S)
        if(C26Character::Requires(EC26VisualRole(R),EC26EquipmentSlot(S))&&!Slots.Contains(EC26EquipmentSlot(S)))
            Errors.Add(FString::Printf(TEXT("Role %d missing required equipment slot %d"),R,S));
    const TArray<FName> Base={TEXT("FielderReady"),TEXT("BatterReady_R"),TEXT("BatterReady_L"),TEXT("BowlerReady"),
        TEXT("KeeperReady"),TEXT("UmpireReady"),TEXT("Walk"),TEXT("Run"),TEXT("BatterRun_R"),TEXT("BatterRun_L"),
        TEXT("Start"),TEXT("Stop"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Pickup"),TEXT("Throw"),TEXT("Catch"),TEXT("KeeperReceive"),
        TEXT("Celebrate"),TEXT("BatterCelebrate_R"),TEXT("BatterCelebrate_L"),TEXT("Disappointed"),
        TEXT("SignalOut"),TEXT("SignalFour"),TEXT("SignalSix"),TEXT("SignalWide")};
    for(FName Key:Base)if(!FindClip(Key))Errors.Add(TEXT("Missing required animation: ")+Key.ToString());
    for(const TCHAR* Hand:{TEXT("_R"),TEXT("_L")})
    {
        for(const FString& Shot:C26Character::ShotClips())
        {
            const FName Key(*(FString(Shot)+Hand));const auto* Clip=FindClip(Key);
            if(!Clip||Clip->Event!=TEXT("BatContact"))Errors.Add(TEXT("Missing shot/BatContact: ")+Key.ToString());
        }
        for(const TCHAR* Style:{TEXT("FastBowl"),TEXT("OffSpin"),TEXT("LegSpin")})
        {
            const FName Key(*(FString(Style)+Hand));const auto* Clip=FindClip(Key);
            if(!Clip||Clip->Event!=TEXT("BallRelease"))Errors.Add(TEXT("Missing bowling style/BallRelease: ")+Key.ToString());
        }
    }
    const TMap<FName,FName> Contacts={{TEXT("Pickup"),TEXT("Pickup")},{TEXT("Throw"),TEXT("ThrowRelease")},
        {TEXT("Catch"),TEXT("Catch")},{TEXT("KeeperReceive"),TEXT("Catch")}};
    for(const auto& Pair:Contacts){const auto* Clip=FindClip(Pair.Key);if(!Clip||Clip->Event!=Pair.Value)Errors.Add(TEXT("Missing contact event for ")+Pair.Key.ToString());}
    for(const auto& Pair:Clips)
    {
        const auto& Clip=Pair.Value;const FString Prefix=Pair.Key.ToString()+TEXT(": ");
        if(!Clip.Sequence){Errors.Add(Prefix+TEXT("missing sequence"));continue;}
        if(Clip.Sequence->GetSkeleton()!=Skeleton)Errors.Add(Prefix+TEXT("incompatible skeleton; retarget offline"));
        if(Clip.Sequence->GetPlayLength()<.1f)Errors.Add(Prefix+TEXT("invalid duration"));
        if(Clip.Sequence->bEnableRootMotion)Errors.Add(Prefix+TEXT("root motion forbidden in authoritative in-place profile"));
        if(!Clip.Event.IsNone())
        {
            int32 Count=0;for(const auto& Notify:Clip.Sequence->Notifies)if(Notify.NotifyName==Clip.Event)++Count;
            if(Count!=1||Clip.EventTime()<=0.f||Clip.EventTime()>=Clip.Sequence->GetPlayLength())Errors.Add(Prefix+TEXT("requires exactly one interior contact/release notify"));
        }
    }
    return Errors.IsEmpty();
}
bool UC26CharacterProfile::ValidateForMatch() const
{
    TArray<FString> Errors;const bool Good=Validate(Errors);
    for(const FString& Error:Errors)UE_LOG(LogTemp,Error,TEXT("C26_CHARACTER_INVALID %s"),*Error);
    return Good;
}

TArray<FString> UC26CharacterProfile::InspectRole(EC26VisualRole Role) const
{
    TArray<FString> Errors;
    if(uint8(Role)>uint8(EC26VisualRole::Umpire)){Errors.Add(TEXT("Invalid review role"));return Errors;}
    if(SourceAndLicense.IsEmpty())Errors.Add(TEXT("Missing asset provenance/license record"));
    USkeletalMesh* Model=Role==EC26VisualRole::Umpire?UmpireBody:Body;
    AuditBody(Model,Skeleton,Errors);
    for(const auto& Variant:BodyPresets)AuditBody(Variant.Value,Skeleton,Errors);
    if(Model)for(FName Socket:{LeftHandSocket,RightHandSocket})
        if(!Model->FindSocket(Socket))Errors.Add(TEXT("Missing hand socket: ")+Socket.ToString());
    TSet<EC26EquipmentSlot> Slots;
    for(const auto& Item:Equipment)
    {
        if(Slots.Contains(Item.Slot))Errors.Add(TEXT("Duplicate equipment slot"));
        Slots.Add(Item.Slot);
        if(Item.Slot==EC26EquipmentSlot::Ball)Errors.Add(TEXT("Duplicate equipment ball forbidden"));
        if(!C26Character::Allows(Role,Item.Slot))continue;
        if(!Item.Mesh)Errors.Add(TEXT("Missing equipment mesh: ")+Item.Socket.ToString());
        if(!Model||!Model->FindSocket(Item.Socket))Errors.Add(TEXT("Missing equipment socket: ")+Item.Socket.ToString());
        if(!Item.LeftHandedSocket.IsNone()&&(!Model||!Model->FindSocket(Item.LeftHandedSocket)))
            Errors.Add(TEXT("Missing left-handed socket: ")+Item.LeftHandedSocket.ToString());
    }
    for(uint8 S=0;S<=uint8(EC26EquipmentSlot::Accessory);++S)
        if(C26Character::Requires(Role,EC26EquipmentSlot(S))&&!Slots.Contains(EC26EquipmentSlot(S)))
            Errors.Add(FString::Printf(TEXT("Role %d missing required equipment slot %d"),uint8(Role),S));
    TMap<FName,FName> Required;
    for(const TCHAR* Key:{TEXT("Walk"),TEXT("Run"),TEXT("Start"),TEXT("Stop"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Celebrate"),TEXT("Disappointed")})
        Required.Add(Key,NAME_None);
    if(Role==EC26VisualRole::Batter||Role==EC26VisualRole::NonStriker)
    {
        for(const TCHAR* Hand:{TEXT("_L"),TEXT("_R")})
        {
            for(const TCHAR* Key:{TEXT("BatterReady"),TEXT("BatterRun"),TEXT("BatterCelebrate")})Required.Add(FName(*(FString(Key)+Hand)),NAME_None);
            for(const FString& Key:C26Character::ShotClips())
                Required.Add(FName(*(Key+Hand)),TEXT("BatContact"));
        }
    }
    else if(Role==EC26VisualRole::Umpire)
        for(const TCHAR* Key:{TEXT("UmpireReady"),TEXT("UmpireWalk"),TEXT("SignalOut"),TEXT("SignalFour"),TEXT("SignalSix"),TEXT("SignalWide")})Required.Add(Key,NAME_None);
    else
    {
        Required.Add(Role==EC26VisualRole::Keeper?TEXT("KeeperReady"):Role==EC26VisualRole::Bowler?TEXT("BowlerReady"):TEXT("FielderReady"),NAME_None);
        Required.Add(TEXT("Pickup"),TEXT("Pickup"));Required.Add(TEXT("Throw"),TEXT("ThrowRelease"));
        Required.Add(Role==EC26VisualRole::Keeper?TEXT("KeeperReceive"):TEXT("Catch"),TEXT("Catch"));
        if(Role==EC26VisualRole::Bowler)for(const TCHAR* Hand:{TEXT("_L"),TEXT("_R")})
            for(const TCHAR* Key:{TEXT("FastBowl"),TEXT("OffSpin"),TEXT("LegSpin")})Required.Add(FName(*(FString(Key)+Hand)),TEXT("BallRelease"));
    }
    for(const auto& Pair:Required)
    {
        const auto* Clip=FindClip(Pair.Key);const FString Prefix=Pair.Key.ToString()+TEXT(": ");
        if(!Clip){Errors.Add(Prefix+TEXT("missing role animation"));continue;}
        if(Clip->Sequence->GetSkeleton()!=Skeleton)Errors.Add(Prefix+TEXT("incompatible skeleton"));
        if(Clip->Sequence->GetPlayLength()<.1f)Errors.Add(Prefix+TEXT("invalid duration"));
        if(Clip->Sequence->bEnableRootMotion)Errors.Add(Prefix+TEXT("root motion forbidden"));
        if(Clip->Event!=Pair.Value)Errors.Add(Prefix+TEXT("wrong action marker"));
        if(!Pair.Value.IsNone())
        {
            int32 Count=0;for(const auto& Notify:Clip->Sequence->Notifies)if(Notify.NotifyName==Pair.Value)++Count;
            if(Count!=1||Clip->EventTime()<=0||Clip->EventTime()>=Clip->Sequence->GetPlayLength())Errors.Add(Prefix+TEXT("requires exactly one interior action marker"));
        }
    }
    return Errors;
}

bool UC26CharacterProfile::SetEquipmentSocket(USkeletalMesh* Candidate,FName Name,FName Bone,FTransform Local)
{
#if WITH_EDITOR
    if(!Candidate||!Candidate->GetSkeleton()||Name.IsNone()||Candidate->GetRefSkeleton().FindBoneIndex(Bone)<0)return false;
    auto* Skel=Candidate->GetSkeleton();Skel->Modify();
    USkeletalMeshSocket* Socket=nullptr;
    for(const auto& Existing:Skel->Sockets)if(Existing&&Existing->SocketName==Name){Socket=Existing;break;}
    if(!Socket){Socket=NewObject<USkeletalMeshSocket>(Skel);Socket->SocketName=Name;Skel->Sockets.Add(Socket);}
    Socket->Modify();Socket->BoneName=Bone;Socket->RelativeLocation=Local.GetLocation();
    Socket->RelativeRotation=Local.Rotator();Socket->RelativeScale=Local.GetScale3D();Skel->MarkPackageDirty();return true;
#else
    return false;
#endif
}

TArray<FString> UC26CharacterProfile::InspectBody(USkeletalMesh* Candidate)
{TArray<FString> Errors;AuditBody(Candidate,Candidate?Candidate->GetSkeleton():nullptr,Errors);return Errors;}
TMap<FName,FTransform> UC26CharacterProfile::BindPose(USkeletalMesh* Candidate)
{
    TMap<FName,FTransform> Out;if(!Candidate)return Out;
    const auto& Ref=Candidate->GetRefSkeleton();TArray<FTransform> Transforms;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        FTransform T=Ref.GetRefBonePose()[I];const int32 Parent=Ref.GetParentIndex(I);
        if(Parent>=0)T*=Transforms[Parent];Transforms.Add(T);Out.Add(Ref.GetBoneName(I),T);
    }
    return Out;
}

TMap<int32,FString> UC26CharacterProfile::ExportMaterialMap(USkeletalMesh* Candidate,int32 Lod)
{
    TMap<int32,FString> Out;
#if WITH_EDITOR
    if(!Candidate||!Candidate->GetImportedModel()||!Candidate->GetImportedModel()->LODModels.IsValidIndex(Lod))return Out;
    const auto* Info=Candidate->GetLODInfo(Lod);const auto& Sections=Candidate->GetImportedModel()->LODModels[Lod].Sections;
    for(int32 I=0;I<Sections.Num();++I)
    {
        int32 Material=Sections[I].MaterialIndex;
        if(Info&&Info->LODMaterialMap.IsValidIndex(I)&&Info->LODMaterialMap[I]>=0)Material=Info->LODMaterialMap[I];
        if(Candidate->GetMaterials().IsValidIndex(Material))
            Out.Add(Sections[I].MaterialIndex,GetPathNameSafe(Candidate->GetMaterials()[Material].MaterialInterface));
    }
#endif
    return Out;
}
