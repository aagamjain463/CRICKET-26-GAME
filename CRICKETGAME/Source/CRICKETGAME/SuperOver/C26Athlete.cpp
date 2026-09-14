#include "C26Athlete.h"
#include "Characters/C26CharacterPresentationComponent.h"
#include "C26Types.h"
#include "C26Motion.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Stats/Stats.h"
// Perf instrumentation for the mobile budget: `stat Cricket26` on device shows
// the per-frame cost of the pose solve, the authored-clip sampling inside it,
// and the procedural garment rebuild. All three were the systems this overhaul
// touched, so they are the three that have to be measurable before anyone
// optimises them.
DECLARE_STATS_GROUP(TEXT("Cricket26 Athletes"), STATGROUP_C26Athlete, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Pose solve (Animate)"), STAT_C26_Animate, STATGROUP_C26Athlete);
DECLARE_CYCLE_STAT(TEXT("Authored clip sample"), STAT_C26_AuthoredClip, STATGROUP_C26Athlete);
DECLARE_CYCLE_STAT(TEXT("Garment rebuild (Uniform)"), STAT_C26_Uniform, STATGROUP_C26Athlete);

namespace
{
// Length from the top of the handle to the toe of the blade, and how far below the hands the ball
// meets the middle of the blade. Both are real bat dimensions and both are used by the posing code,
// so bat, hands and contact point can never drift apart.
constexpr float BatLength=C26Field::BatLength;
constexpr float MiddleDrop=62.f;
/** Mesh-space forward for the imported rig. */
const FVector RigForward(0,1,0);
}

void UC26PoseMesh::ApplyComponentPose(const TArray<FTransform>& Pose)
{
    if(!GetSkinnedAsset()||BoneSpaceTransforms.Num()!=Pose.Num())return;
    const auto& Ref=GetSkinnedAsset()->GetRefSkeleton();
    for(int I=0;I<Pose.Num();++I)
    {
        const int P=Ref.GetParentIndex(I);
        BoneSpaceTransforms[I]=P<0?Pose[I]:Pose[I].GetRelativeTransform(Pose[P]);
    }
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
void UC26PoseMesh::ApplyLocalPose(const TArray<FTransform>& Local)
{
    if(!GetSkinnedAsset()||BoneSpaceTransforms.Num()!=Local.Num())return;
    BoneSpaceTransforms=Local;
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
AC26Athlete::AC26Athlete()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Presentation=CreateDefaultSubobject<UC26CharacterPresentationComponent>(TEXT("CharacterPresentation"));
    Mesh=CreateDefaultSubobject<UC26PoseMesh>(TEXT("Cricketer"));Mesh->SetupAttachment(RootComponent);
    HeroMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroAthleteMesh"));
    HeroMesh->SetupAttachment(RootComponent);
    HeroMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeroMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    HeroMesh->SetCastShadow(true);
    HeroMesh->SetVisibility(true);
    // No default hero scan. The role resolver in Configure() assigns the correct baked
    // mesh per match role; defaulting to the batter scan made every not-yet-configured
    // athlete render as a padded batter holding nothing.
    HeroMesh->SetVisibility(false);
    HeroMesh->SetHiddenInGame(true);

    // The imported rig faces mesh +Y with mesh +X out to its left. Yawing the mesh by -90 makes the
    // actor's own +X the athlete's forward, so every yaw the match code already computes -- fielders
    // turning to the middle, the striker facing the bowler, the bowler running in -- points the
    // right way. Without this the whole side stands square to the play.
    Mesh->SetRelativeRotation(FRotator(0,-90,0));
    // USER DIRECTIVE: SK_Cricketer_KitBase's own body/trouser geometry is documented-incomplete
    // (Mixamo deleted torso/thigh geometry under the clothes; Bottoms stops at the knee), which is
    // the real cause of the blotchy, gapped-looking hip/thigh seen in gameplay captures -- not a
    // material bug. SK_Cricketer_Match (ArtSource/Blender/Characters/build_match_kit.py) is the
    // already-authored, already-imported fix: full torso/thigh skin plus properly weighted
    // full-length jersey and trousers on the same shared skeleton. Switching to it.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Player(TEXT("/Game/Cricket26/Characters/SK_Cricketer_Match.SK_Cricketer_Match"));
    if(Player.Succeeded())Mesh->SetSkinnedAssetAndUpdate(Player.Object);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAsset(TEXT("/Game/Cricket26/Animations/A_Run.A_Run"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Cricket26/Animations/A_Idle.A_Idle"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkinAsset(TEXT("/Game/Cricket26/Materials/M_C26_PlayerSkin.M_C26_PlayerSkin"));
    RunClip=RunAsset.Object;IdleClip=IdleAsset.Object;TexturedSkin=SkinAsset.Object;
    // NOTE: the authored clips (incl. the two original family bases, BattingDrive and
    // BowlingPace) are NOT loaded here -- they come through LoadShotLibrary()'s lazy
    // LoadObject, so a checkout that has not yet run Tools/ImportAnimations.py boots
    // clean and degrades to the procedural actions instead of logging import errors
    // for every athlete CDO.
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);
    Mesh->SetVisibility(false);Mesh->SetHiddenInGame(true);
    // Equipment rides in mesh space so it shares one frame with the posed skeleton.
    // Authored in Blender (ArtSource/Blender/Equipment), imported and size-checked by
    // Tools/ImportEquipment.py. Each mesh is modelled in the local frame the posing code below
    // already works in, so replacing the old procedural ring-lofts changed the geometry without
    // re-deriving a single attach transform.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BatAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Bat_Hero.SM_C26_Bat_Hero"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HelmetAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Helmet_Hero.SM_C26_Helmet_Hero"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrilleAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_HelmetGrille_Hero.SM_C26_HelmetGrille_Hero"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PadLAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Pad_L.SM_C26_Pad_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PadRAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Pad_R.SM_C26_Pad_R"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GloveLAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Glove_L.SM_C26_Glove_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GloveRAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Glove_R.SM_C26_Glove_R"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShoeLAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Shoe_L.SM_C26_Shoe_L"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShoeRAsset(TEXT("/Game/Cricket26/Equipment/SM_C26_Shoe_R.SM_C26_Shoe_R"));
    auto Part=[this](const TCHAR* Name,const ConstructorHelpers::FObjectFinder<UStaticMesh>& Asset)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        C->SetupAttachment(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
        C->SetCastShadow(false);
        C->SetVisibility(false);
        C->SetHiddenInGame(true);
        if(Asset.Succeeded())C->SetStaticMesh(Asset.Object);
        return C;
    };
    Bat=Part(TEXT("WillowBat"),BatAsset);
    Headwear=Part(TEXT("Headwear"),HelmetAsset);
    Grill=Part(TEXT("HelmetGrille"),GrilleAsset);
    PadL=Part(TEXT("LeftPad"),PadLAsset);
    PadR=Part(TEXT("RightPad"),PadRAsset);
    GloveL=Part(TEXT("LeftGlove"),GloveLAsset);
    GloveR=Part(TEXT("RightGlove"),GloveRAsset);
    ShoeL=Part(TEXT("LeftShoe"),ShoeLAsset);
    ShoeR=Part(TEXT("RightShoe"),ShoeRAsset);
    // Headwear sits millimetres from the face. Left casting, the peak, shell and grille throw the
    // whole head into shadow and the face renders as a dark mass in every replay close-up. Their
    // ground shadows are sub-pixel at gameplay distance, so unchecking them costs nothing visible.
    Headwear->SetCastShadow(false);
    Grill->SetCastShadow(false);
    Uniform=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TailoredCricketTrousers"));Uniform->SetupAttachment(Mesh);
    Uniform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShirtNumber=CreateDefaultSubobject<UTextRenderComponent>(TEXT("KitNumber"));ShirtNumber->SetupAttachment(Mesh);
    ShirtNumber->SetHorizontalAlignment(EHTA_Center);ShirtNumber->SetVerticalAlignment(EVRTA_TextCenter);
    ShirtNumber->SetWorldSize(19);ShirtNumber->SetTextRenderColor(FColor(213,237,231));ShirtNumber->SetCastShadow(false);
    // The contact shadow lives on the actor root, not on Mesh: athletes are only ever yawed, so it
    // stays flat on the turf without having to undo the rig's -90 mesh rotation every frame.
    Shade=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContactShadow"));Shade->SetupAttachment(RootComponent);
    Shade->SetCollisionEnabled(ECollisionEnabled::NoCollision);Shade->SetCastShadow(false);
    Shade->bReceivesDecals=false;Shade->SetTranslucentSortPriority(-4);
    // Every component here defaults to Static mobility, which is what CreateDefaultSubobject gives
    // you. A static primitive is never entered into the dynamic shadow pass, so until this loop
    // existed no athlete in the game cast a shadow on the ground at any quality level -- measured
    // from a capture, the turf under a batter sampled identically to the turf beside him. These
    // actors are moved every frame, so Movable is also simply the correct mobility for them.
    for(UActorComponent* Component:GetComponents())
        if(auto* Scene=Cast<USceneComponent>(Component))Scene->SetMobility(EComponentMobility::Movable);
}
void AC26Athlete::RebuildReference()
{
    auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset());
    if(!S)return;
    AuthoredKit=S->GetName().Contains(TEXT("_Match"));
    const auto& R=S->GetRefSkeleton();Reference.SetNum(R.GetNum());Parents.SetNum(R.GetNum());
    Bones.Empty();
    for(int I=0;I<R.GetNum();++I)
    {
        Parents[I]=R.GetParentIndex(I);Reference[I]=R.GetRefBonePose()[I];
        if(Parents[I]>=0)Reference[I]*=Reference[Parents[I]];
        FString Name=R.GetBoneName(I).ToString();Name.RemoveFromStart(TEXT("mixamorig:"));Name.RemoveFromStart(TEXT("mixamorig_"));
        Bones.Add(Name,I);
    }
    // The legacy Mixamo rigs import at roughly 340-380 cm (bounds-verified in-editor)
    // while the venue is real-world scale; the authored hero meshes export at real height
    // (173-183 cm, bounds-verified). Shrink about the ground origin only when the bound
    // mesh arrives at import scale, so every authored IK target, piece of equipment and
    // contact point lines up on either mesh.
    const float BoundHeight=S->GetBounds().BoxExtent.Z*2.f;
    const float GroundScale=BoundHeight>250.f?0.48f:1.f;
    // Scale the skinning transforms as well as joint positions. Translating the joints alone
    // compresses the limbs while leaving vertex offsets (head, hair, shoulders) at import size.
    // Equipment is already authored in real centimetres and must not inherit that import scale.
    for(auto& T:Reference)
    {T.SetLocation(T.GetLocation()*GroundScale);T.SetScale3D(T.GetScale3D()*GroundScale);}
    Pose=Reference;
    // A rebuild means a different skeleton or a different scale. Carrying a displayed pose across
    // that would smooth between two unrelated bodies.
    Shown.Reset();Goal.Reset();
    const int Sh=Bone(TEXT("LeftArm")),Hp=Bone(TEXT("Hips")),An=Bone(TEXT("LeftFoot"));
    ShoulderZ=Sh>=0?Reference[Sh].GetLocation().Z:144.f;
    HipZ=Hp>=0?Reference[Hp].GetLocation().Z:100.f;
    AnkleZ=An>=0?Reference[An].GetLocation().Z:11.8f;
    const int Fa=Bone(TEXT("LeftForeArm")),Hd=Bone(TEXT("LeftHand"));
    ArmSpan=Sh>=0&&Fa>=0&&Hd>=0
        ?(Reference[Fa].GetLocation()-Reference[Sh].GetLocation()).Size()
            +(Reference[Hd].GetLocation()-Reference[Fa].GetLocation()).Size()
        :54.f;
    const int Wr=Bone(TEXT("RightHand")),Kn=Bone(TEXT("RightHandMiddle1")),Tp=Bone(TEXT("RightHandMiddle3"));
    if(Wr>=0&&Kn>=0&&Tp>=0)
        PalmReach=float((FMath::Lerp(Reference[Kn].GetLocation(),Reference[Tp].GetLocation(),.72f)-Reference[Wr].GetLocation()).Size());
    UE_LOG(LogTemp,Log,TEXT("C26_RIG mesh=%s scale=%.2f shoulder=%.1f hip=%.1f ankle=%.1f armspan=%.1f palm=%.1f"),
        *S->GetName(),GroundScale,ShoulderZ,HipZ,AnkleZ,ArmSpan,PalmReach);
}
void AC26Athlete::BeginPlay()
{
    Super::BeginPlay();
    RebuildReference();
    BuildContactShadow();
    ApplyDetail();
}
void AC26Athlete::Dress(UStaticMeshComponent* Part,const TCHAR* Key,UMaterialInstanceDynamic* M)
{
    if(!Part||!Part->GetStaticMesh()||!M)return;
    const TArray<FStaticMaterial>& Slots=Part->GetStaticMesh()->GetStaticMaterials();
    for(int I=0;I<Slots.Num();++I)
        if(Slots[I].MaterialSlotName.ToString().Contains(Key))Part->SetMaterial(I,M);
}
void AC26Athlete::UpdateDetail(const FVector& ViewPoint)
{
    if(Presentation&&Presentation->IsActive()){Presentation->SetQualityForView(ViewPoint);return;}
    // The striker, the bowler and the keeper are hero wherever they stand: they are what the
    // presentation is about, and demoting them by distance would drop the grille off a batter
    // during a wide replay. Fielders earn hero quality by being near the play.
    const float Range=FVector::Dist(GetActorLocation(),ViewPoint);
    const bool Principal=Role==EC26Role::Batter||Role==EC26Role::Bowler||Role==EC26Role::Keeper;
    Wanted=Principal||Range<HeroRange?EDetail::Hero:Range<MidRange?EDetail::Mid:EDetail::Distant;
    if(Wanted!=Detail)ApplyDetail();
}
void AC26Athlete::ApplyDetail()
{
    Detail=Wanted;
    if(bHeroVisual&&HeroMesh&&HeroMesh->GetStaticMesh())
    {
        const bool Far=Detail==EDetail::Distant;
        HeroMesh->SetCastShadow(!Far);
        Mesh->SetVisibility(false);
        Bat->SetVisibility(false);
        Headwear->SetVisibility(false);
        Grill->SetVisibility(false);
        PadL->SetVisibility(false);
        PadR->SetVisibility(false);
        GloveL->SetVisibility(false);
        GloveR->SetVisibility(false);
        ShoeL->SetVisibility(false);
        ShoeR->SetVisibility(false);
        Uniform->SetVisibility(false);
        ShirtNumber->SetVisibility(false);
        return;
    }
    const bool Guarded=Role==EC26Role::Batter||Role==EC26Role::Keeper;
    const bool Near=Detail==EDetail::Hero;
    const bool Far=Detail==EDetail::Distant;
    // A grille is a cage of 0.3 cm bars. Past hero range it costs 876 triangles to render a grey
    // smudge, and past mid range the whole head is a few pixels across.
    Grill->SetVisibility(Guarded&&Near);Grill->SetHiddenInGame(!(Guarded&&Near));
    // Gloves are the smallest piece of kit an athlete carries and the first thing that stops
    // resolving. Pads and shoes stay on at every tier: they are most of the lower silhouette.
    GloveL->SetVisibility(Guarded&&!Far);GloveL->SetHiddenInGame(!(Guarded&&!Far));
    GloveR->SetVisibility(Guarded&&!Far);GloveR->SetHiddenInGame(!(Guarded&&!Far));
    // Separate authored equipment is role-gated, and only on the animated kit path: the
    // hero branch returns before this point. Batters alone carry a bat; batters and
    // keepers wear pads and headwear; bowlers and fielders show team clothing and shoes.
    const bool KittedHead=!bHeroVisual&&(Role==EC26Role::Batter||Role==EC26Role::Keeper);
    const bool KittedBat=!bHeroVisual&&Role==EC26Role::Batter;
    Bat->SetVisibility(KittedBat);Bat->SetHiddenInGame(!KittedBat);
    Headwear->SetVisibility(KittedHead);Headwear->SetHiddenInGame(!KittedHead);
    PadL->SetVisibility(KittedHead);PadL->SetHiddenInGame(!KittedHead);
    PadR->SetVisibility(KittedHead);PadR->SetHiddenInGame(!KittedHead);
    ShoeL->SetVisibility(true);ShoeL->SetHiddenInGame(false);
    ShoeR->SetVisibility(true);ShoeR->SetHiddenInGame(false);
    ShirtNumber->SetVisibility(Role!=EC26Role::Umpire&&!Far);
    // Collar, placket, hem and leg stripe are centimetre-scale trim. They are what makes the kit
    // read in a replay close-up and pure cost on a fielder at the rope.
    for(int Section=3;Section<6;++Section)
        if(Uniform->GetNumSections()>Section)Uniform->SetMeshSectionVisible(Section,Near);
    if(Uniform->GetNumSections()>1)Uniform->SetMeshSectionVisible(1,!Far);
    Mesh->SetCastShadow(!Far);
}
void AC26Athlete::BuildContactShadow()
{
    // A unit-radius disc whose vertex alpha runs 1 at the centre to 0 at the rim, in two rings so
    // the falloff has a dense core and a soft skirt. All the shaping -- how long the shadow is,
    // which way it leans, how dark it gets -- is done by the transform and the Opacity parameter in
    // UpdateContactShadow, so this geometry is built exactly once per athlete.
    if(!Shade||Shade->GetNumSections()>0)return;
    TArray<FVector> V;TArray<FVector> N;TArray<int32> T;TArray<FVector2D> UV;
    TArray<FLinearColor> C;TArray<FProcMeshTangent> Tan;
    constexpr int Sides=24;
    const float Radius[3]={0.f,.55f,1.f};
    const float Alpha[3]={1.f,.72f,0.f};
    for(int Ring=0;Ring<3;++Ring)
        for(int J=0;J<Sides;++J)
        {
            const float A=2*PI*J/Sides;
            V.Add(FVector(FMath::Cos(A)*Radius[Ring],FMath::Sin(A)*Radius[Ring],0));
            N.Add(FVector::UpVector);UV.Add(FVector2D(J/float(Sides),Radius[Ring]));
            C.Add(FLinearColor(0,0,0,Alpha[Ring]));
        }
    for(int Ring=0;Ring<2;++Ring)
        for(int J=0;J<Sides;++J)
        {
            const int A=Ring*Sides+J,B=Ring*Sides+(J+1)%Sides;
            T.Append({A,B,A+Sides,B,B+Sides,A+Sides});
        }
    Shade->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Shade.M_Shade"));
    if(!Base){UE_LOG(LogC26,Warning,TEXT("C26_SHADE M_Shade missing; contact shadows disabled."));return;}
    ShadeMaterial=UMaterialInstanceDynamic::Create(Base,this);
    Shade->SetMaterial(0,ShadeMaterial);
    // A visible fallback size, so a shadow that never receives a pose update is still obvious on
    // screen rather than a 1 cm speck nobody can see.
    Shade->SetRelativeScale3D(FVector(58,36,1));
    if(FParse::Param(FCommandLine::Get(),TEXT("C26ShadeDebug")))
    {
        ShadeMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,0,1));
        ShadeMaterial->SetScalarParameterValue(TEXT("Glow"),1.f);
        ShadeMaterial->SetScalarParameterValue(TEXT("Opacity"),1.f);
    }
    UE_LOG(LogC26,Display,TEXT("C26_SHADE built verts=%d tris=%d sections=%d material=%s"),
        V.Num(),T.Num()/3,Shade->GetNumSections(),*Base->GetName());
}
void AC26Athlete::UpdateContactShadow()
{
    if(bHeroVisual && HeroMesh && HeroMesh->GetStaticMesh())
    {
        if(Shade) Shade->SetVisibility(false);
        return;
    }
    if(!Shade||Shade->GetNumSections()==0)return;
    const int Lf=Bone(TEXT("LeftFoot")),Rf=Bone(TEXT("RightFoot"));
    if(Lf<0||Rf<0)return;
    // Mesh-space foot positions, converted through the mesh's own relative rotation into the actor
    // frame the shadow component lives in. Anchoring to the feet rather than the actor origin means
    // the shadow tracks a batter's stride and a bowler's delivery leap instead of sliding under him.
    const FTransform MeshLocal=Mesh->GetRelativeTransform();
    const FVector Left=MeshLocal.TransformPosition(Pose[Lf].GetLocation());
    const FVector Right=MeshLocal.TransformPosition(Pose[Rf].GetLocation());
    const FVector Feet=(Left+Right)*.5f;
    const float Lift=FMath::Max(0.f,FMath::Min(Left.Z,Right.Z)-AnkleZ*.48f);
    // KeyLight sits at pitch -58 / yaw -38, so the cast runs out along this world direction with a
    // length of about 0.62 of the caster's height. Unrotating by the actor yaw keeps that world
    // direction correct for a fielder facing any way round the ground.
    const FVector CastWorld(.646f,-.505f,0);
    const FVector Cast=GetActorRotation().UnrotateVector(CastWorld);
    const float Spread=FMath::Clamp((Left-Right).Size2D()*.5f,0.f,34.f);
    const float Reach=52.f+Spread;
    // Clear of the tallest thing the athlete can be standing on. The pitch plate is at Z=2.2 and
    // the worn landing areas at 2.6, so a patch drawn at 1.6 sat *inside* the pitch and was
    // invisible for exactly the two players who matter most -- the striker and the bowler.
    Shade->SetRelativeLocation(FVector(Feet.X,Feet.Y,3.4f)+Cast*34.f);
    Shade->SetRelativeRotation(FRotator(0,Cast.Rotation().Yaw,0));
    // Long axis down the cast direction, narrow across it: a real floodlit shadow is an ellipse,
    // not a circle. A player off the ground loses contact, so the patch spreads and fades.
    const float Air=FMath::Clamp(Lift/45.f,0.f,1.f);
    Shade->SetRelativeScale3D(FVector(Reach*(1.f+Air*.55f),(30.f+Spread)*(1.f+Air*.55f),1.f));
    if(ShadeMaterial&&!FParse::Param(FCommandLine::Get(),TEXT("C26ShadeDebug")))
        ShadeMaterial->SetScalarParameterValue(TEXT("Opacity"),.62f*(1.f-Air*.65f));
    static int32 Reported=0;
    if(Reported<3)
    {
        ++Reported;
        UE_LOG(LogC26,Display,TEXT("C26_SHADE place role=%d world=%s scale=%s visible=%d"),
            int(Role),*Shade->GetComponentLocation().ToCompactString(),
            *Shade->GetRelativeScale3D().ToCompactString(),Shade->IsVisible()?1:0);
    }
}
bool AC26Athlete::MatchesBindPose(USkeletalMesh* Candidate)
{
    // Refuse a body whose geometry does not occupy its own rig's bind pose.
    //
    // This gate exists because the ten `SK_Cricketer_Hero*` bodies do not. They are Sketchfab
    // cricketer scans that were parented to the 67-bone Mixamo rig and auto-weighted, and three
    // measurements (Blender, on the source FBX) say they never fitted it:
    //
    //   * they are HALF BODIES -- the geometry stops at mid-thigh, so there are no legs to skin.
    //     That is the torso-standing-in-a-hole every player rendered as.
    //   * they stand with their arms at their sides while the rig is a T-pose, so the arm bones
    //     lie outside the mesh entirely. `LeftArm`, `LeftForeArm`, `LeftHand`, `RightArm`,
    //     `RightForeArm` and `RightHand` ended up owning no vertices at all.
    //   * what weights they did get are nonsense: `mixamorig:Spine` dominated vertices from ankle
    //     height to the chest, `LeftEye` owned the whole head, `RightToe_End` owned the lower leg.
    //     Only 24 of 67 bones had a vertex group.
    //
    // The cheap, allocation-free signal that catches all three is the mesh's widest horizontal
    // span against the rig's own hand-to-hand reach. A mesh bound in a T-pose is as wide as its
    // skeleton's arms; the scans measure 0.64 m across a rig that reaches about 1.5 m, a ratio of
    // 0.43. Nothing about a correctly bound body produces that, and a body that fails it cannot be
    // animated no matter what the rest of this class does to it.
    if(!Candidate)return false;
    const auto& Ref=Candidate->GetRefSkeleton();
    auto Place=[&Ref](const TCHAR* Want)->FVector
    {
        for(int I=0;I<Ref.GetNum();++I)
        {
            FString Name=Ref.GetBoneName(I).ToString();
            Name.RemoveFromStart(TEXT("mixamorig:"));Name.RemoveFromStart(TEXT("mixamorig_"));
            if(Name!=Want)continue;
            FTransform T=FTransform::Identity;
            for(int J=I;J>=0;J=Ref.GetParentIndex(J))T*=Ref.GetRefBonePose()[J];
            return T.GetLocation();
        }
        return FVector::ZeroVector;
    };
    const FVector L=Place(TEXT("LeftHand")),R=Place(TEXT("RightHand"));
    const float Reach=float((L-R).Size());
    if(Reach<UE_KINDA_SMALL_NUMBER)return true;
    // Measure the mesh along the axis the hands actually separate on, not along whichever of X or
    // Y happens to be biggest: one of these scans is a metre deep front-to-back, which sailed
    // through an axis-agnostic max() while still being only 64 cm across the arms.
    const FVector Across=(L-R).GetSafeNormal();
    const FVector Extent=Candidate->GetImportedBounds().BoxExtent;
    const float Span=2.f*float(FMath::Abs(Extent.X*Across.X)+FMath::Abs(Extent.Y*Across.Y)+FMath::Abs(Extent.Z*Across.Z));
    const float Ratio=Span/Reach;
    if(Ratio<.75f)
    {
        UE_LOG(LogTemp,Warning,TEXT("C26_SCAN %s rejected: %.0f cm wide across a %.0f cm rig (%.2f) -- not bound to its bind pose"),
            *Candidate->GetName(),Span,Reach,Ratio);
        return false;
    }
    return true;
}
void AC26Athlete::ApplyHeroScan()
{
    // Ten hero scans live in Content/Cricket26/Characters/Players, one per role, each with its own
    // UV-matched MI_Player_* instance -- the pairing is one-to-one because each scan is a bake of
    // one body, and Tools/FixPlayerVisualPairs.py records that crossing a scan with another
    // player's texture renders as camouflage noise.
    //
    // Every one of them currently fails MatchesBindPose above, so this function falls back to the
    // animated kit mesh and the match plays on that. It is kept wired rather than deleted because
    // the path is correct and the gate is the only thing between it and a usable scan: drop in a
    // full-body scan that is actually skinned to this rig and it binds with no code change.
    static const TCHAR* const FielderScan[6]={
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder01.SK_Cricketer_HeroFielder01"),
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder02.SK_Cricketer_HeroFielder02"),
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder03.SK_Cricketer_HeroFielder03"),
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder04.SK_Cricketer_HeroFielder04"),
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder05.SK_Cricketer_HeroFielder05"),
        TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroFielder06.SK_Cricketer_HeroFielder06")};
    static const TCHAR* const FielderMat[6]={
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_01.MI_Player_Fielder_01"),
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_02.MI_Player_Fielder_02"),
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_03.MI_Player_Fielder_03"),
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_04.MI_Player_Fielder_04"),
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_05.MI_Player_Fielder_05"),
        TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_06.MI_Player_Fielder_06")};
    const TCHAR* ScanPath=nullptr;
    const TCHAR* MatPath=nullptr;
    switch(Role)
    {
        case EC26Role::Batter:
            ScanPath=TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBatter.SK_Cricketer_HeroBatter");
            MatPath=TEXT("/Game/Cricket26/Materials/Players/MI_Player_Batter.MI_Player_Batter");break;
        case EC26Role::Bowler:
            ScanPath=TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroBowler.SK_Cricketer_HeroBowler");
            MatPath=TEXT("/Game/Cricket26/Materials/Players/MI_Player_Bowler.MI_Player_Bowler");break;
        case EC26Role::Keeper:
            ScanPath=TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroKeeper.SK_Cricketer_HeroKeeper");
            MatPath=TEXT("/Game/Cricket26/Materials/Players/MI_Player_Keeper.MI_Player_Keeper");break;
        case EC26Role::Umpire:
            ScanPath=TEXT("/Game/Cricket26/Characters/Players/SK_Cricketer_HeroUmpire.SK_Cricketer_HeroUmpire");
            MatPath=TEXT("/Game/Cricket26/Materials/Players/MI_Player_Umpire.MI_Player_Umpire");break;
        default:
        {
            // Eleven fielders share six scans. The squad number is stable for a match, so a given
            // fielder keeps the same face all innings instead of changing on every Configure.
            const int I=(FMath::Max(1,SquadNumber)-1)%6;
            ScanPath=FielderScan[I];MatPath=FielderMat[I];break;
        }
    }
    if(!ScanPath)return;
    auto* Scan=LoadObject<USkeletalMesh>(nullptr,ScanPath);
    if(!Scan)
    {
        // Keep the previous body rather than blanking the athlete. The kit mesh still renders and
        // still animates, so a missing scan degrades to the pre-scan look instead of an empty field.
        UE_LOG(LogTemp,Warning,TEXT("C26_SCAN missing %s -- keeping the kit mesh"),ScanPath);
        return;
    }
    if(!MatchesBindPose(Scan))
    {
        // The kit mesh already on Mesh is a complete, correctly weighted cricketer, so keeping it
        // is a working player rather than a broken one. Still rebuild the reference: Configure
        // reads ShoulderZ/ArmSpan straight after this to place the kit.
        RebuildReference();
        return;
    }
    if(Mesh->GetSkinnedAsset()!=Scan)Mesh->SetSkinnedAssetAndUpdate(Scan,true);
    // The scans arrive with their single slot unset, which is what rendered every player as the
    // engine's grey default. Their own MI is the only UV-correct material for them, so it is
    // applied only once the scan is confirmed bound: putting a scan's bake on the Mixamo kit mesh
    // would render as camouflage noise, which is worse than leaving that mesh as it was.
    if(Mesh->GetSkinnedAsset()==Scan)
    {
        if(auto* MI=LoadObject<UMaterialInterface>(nullptr,MatPath))Mesh->SetMaterial(0,MI);
        else UE_LOG(LogTemp,Warning,TEXT("C26_SCAN missing material %s -- body stays untextured"),MatPath);
        ScanAsset=Scan;bScanVisual=true;
    }
    // Bone list and scale both come from the bound mesh: the scans are real-world height while the
    // Mixamo kit imports at ~343 cm, and RebuildReference is what reconciles the two.
    RebuildReference();
    UE_LOG(LogTemp,Log,TEXT("C26_SCAN role=%d number=%d mesh=%s"),
        int32(Role),SquadNumber,*Scan->GetName());
}
void AC26Athlete::Configure(EC26Role NewRole,int Team,int Number)
{
    if(Presentation)
    {
        const EC26Role OldRole=Role;const int32 OldTeam=TeamId,OldNumber=SquadNumber;
        Role=NewRole;TeamId=Team;SquadNumber=Number;
        if(Presentation->TryActivate(this)){Presentation->Configure(this);SetAction(EC26Action::Ready);return;}
        Role=OldRole;TeamId=OldTeam;SquadNumber=OldNumber;
    }
    if(Shirt&&Role==NewRole&&TeamId==Team&&SquadNumber==Number){SetAction(EC26Action::Ready);return;}
    Role=NewRole;TeamId=Team;
    // Squad number picks which of the six fielder scans this athlete wears, so it has to be current
    // before the body is chosen. Configure re-assigns it later for the shirt text; same value.
    SquadNumber=Number;
    // Bind the body before anything measures the rig: the equipment placement below reads
    // ShoulderZ/HipZ/ArmSpan/PalmReach, and every one of those is derived from the bound mesh.
    ApplyHeroScan();
    // Every athlete material is derived from M_Surface: it is the one generated material with the
    // skeletal-mesh usage flag, and the noise-based ones silently fall back to default grey.
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Surface.M_Surface"));
    if(!Base)return;
    // M_Surface is the flat fallback: a Tint vector and one Roughness scalar, no texture. It stays
    // as the guaranteed-renders option for anything the textured family does not cover.
    auto Make=[&](FLinearColor C,float Rough)
    {auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Tint"),C);M->SetScalarParameterValue(TEXT("Roughness"),Rough);M->SetScalarParameterValue(TEXT("Glow"),0.f);return M;};
    // The textured family, built by Tools/BuildPlayerMaterials.py. Each of these samples a real
    // albedo, normal and roughness, so a kit piece returns weave, fold shading and its own
    // highlight instead of one flat colour. Parameter names beyond Tint/Roughness/Glow are
    // additive -- setting a parameter a material does not declare is a no-op -- so one helper
    // dresses M_Surface, M_C26_Cloth, M_C26_Gear, M_C26_Shell and M_C26_PlayerSkin alike.
    auto* ClothMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Cloth.M_C26_Cloth"));
    auto* GearMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Gear.M_C26_Gear"));
    auto* ShellMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_C26_Shell.M_C26_Shell"));
    // Amount and WeaveTiling are deliberately not named Detail/Sheen: `Detail` is already an
    // AC26Athlete field (the presentation tier) and -Wshadow is an error in this build.
    auto Textured=[&](UMaterialInterface* From,FLinearColor C,float Rough,float Amount,float Rim,float WeaveTiling,float BumpStrength)
    {
        auto* M=UMaterialInstanceDynamic::Create(From?From:Base,this);
        M->SetVectorParameterValue(TEXT("Tint"),C);
        M->SetScalarParameterValue(TEXT("Roughness"),Rough);
        M->SetScalarParameterValue(TEXT("Glow"),0.f);
        M->SetScalarParameterValue(TEXT("Detail"),Amount);
        M->SetScalarParameterValue(TEXT("Sheen"),Rim);
        M->SetScalarParameterValue(TEXT("NormalTiling"),WeaveTiling);
        M->SetScalarParameterValue(TEXT("NormalStrength"),BumpStrength);
        return M;
    };
    // Kit albedo sits above the turf's so the players separate from the field they stand on.
    const FLinearColor Team0(.030,.345,.395),Team1(.660,.100,.058),Official(.070,.092,.140);
    const FLinearColor Kit=Role==EC26Role::Umpire?Official:Team==0?Team0:Team1;
    // The shirt is woven cloth: a fine weave normal, a tileable weave albedo modulating the team
    // colour, and a strong grazing sheen, which is the single thing that stops a jersey reading as
    // painted plastic at broadcast distance. Tiling runs high because the body atlas is a
    // full-body layout, so 24 repeats is a centimetre-scale weave.
    Shirt=Textured(ClothMat,Kit,.86f,.70f,.62f,24.f,1.f);
    // Cricket whites. Team-coloured trousers made the striker read as one teal mass from the
    // batting camera: shirt, trousers and helmet all returned the same value, so the only thing
    // separating his legs from his torso was a shadow. Cream trousers also give the pads
    // something to sit against -- white gear on a white leg is the one pairing that does not
    // read, so the trouser is warmed and the pads stay cool and brighter. Trousers take a tighter
    // weave and a weaker sheen than the shirt, because a heavier cloth is flatter and matte.
    Trousers=Textured(ClothMat,Role==EC26Role::Umpire?FLinearColor(.020,.024,.036):FLinearColor(.560,.545,.500),
        .93f,.62f,.48f,32.f,.85f);
    // Skin has to survive the same floodlit night as the shirt. The imported body diffuse renders
    // almost black on a vertical torso under this rig, so the head and forearms take a controlled
    // mid-brown that stays readable without blowing out -- linear-space, roughly sRGB
    // (215,168,146) darkened a stop for the 4-lux key. That tone is the Tint here, and it now
    // carries pore detail and a pore normal instead of being a perfectly flat surface. This is
    // also a real fix, not a tweak: the previous M_C26_PlayerSkin had no parameters at all, so
    // every Tint set below was silently discarded and all eleven players rendered one identical
    // face value.
    SquadNumber=Number;
    // Deterministic per-player skin variation so ten athletes sharing one team kit do not
    // read as clones. Team shirt/trouser colours are never varied.
    const float Tone=0.90f+0.20f*float((FMath::Abs(Number)*37)%10)/10.f;
    Skin=Textured(TexturedSkin?TexturedSkin.Get():Base,FLinearColor(.46f*Tone,.268f*Tone,.180f*Tone),
        .58f,.32f,.22f,44.f,.75f);
    // Protective gear is leather and webbing, not cloth and not metal: a coarse cell grain, matte,
    // low sheen, higher specular than cloth so a strap keeps an edge under the floodlights.
    Gear=Textured(GearMat,FLinearColor(.58,.61,.57),.78f,.55f,.16f,6.f,.9f);

    // ---- Head: skin, face, eyes, lashes -------------------------------------------------
    // The match mesh carries five slots -- Bodymat, Jerseymat, Trousermat, Eyelashmat, Eyesmat --
    // and only the first three were ever dressed. Eyesmat and Eyelashmat were left on the engine
    // default material, which is why every cricketer in the game had a blank brown head: the eyes
    // were present in the geometry the whole time and simply had nothing bound to them.
    // Separately, the project ships the avatar's own 2048x2048 body atlas and the skin ignored it,
    // because M_C26_PlayerSkin declares only Cloth and Weave -- a Tint was the most detail it could
    // ever return. M_Athlete_PBR is the master that actually takes a BaseTexture, so the real
    // skin (pores, tone variation, brows, lips) goes through that.
    auto* Pbr=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Athlete_PBR.M_Athlete_PBR"));
    auto* BodyTex=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Cricket26/Characters/Remy_Body_Diffuse.Remy_Body_Diffuse"));
    auto TexturedPbr=[&](UTexture2D* Tex,FLinearColor C,float Rough)
    {
        auto* M=UMaterialInstanceDynamic::Create(Pbr?Pbr:Base,this);
        M->SetVectorParameterValue(TEXT("Tint"),C);
        M->SetScalarParameterValue(TEXT("Roughness"),Rough);
        M->SetScalarParameterValue(TEXT("Glow"),0.f);
        if(Tex)M->SetTextureParameterValue(TEXT("BaseTexture"),Tex);
        return M;
    };
    // Real skin when the atlas is present, the previous flat tone when it is not, so a missing
    // texture degrades to the old look rather than to engine grey.
    FaceMat=BodyTex?TexturedPbr(BodyTex,FLinearColor::White,.52f):Skin.Get();
    // An eye is mostly shadow with one small specular. Held dark and glossy rather than textured:
    // the eye mesh has its own UV island and there is no matching atlas in the project, and a
    // wrong albedo there is far more obvious than a clean dark bead.
    Eyes=TexturedPbr(nullptr,FLinearColor(.020,.016,.014),.11f);
    Lash=TexturedPbr(nullptr,FLinearColor(.028,.021,.017),.86f);
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
        for(int I=0;I<S->GetMaterials().Num();++I)
        {
            const FString Name=S->GetMaterials()[I].MaterialSlotName.ToString();
            if(Name.Contains(TEXT("Top"))||Name.Contains(TEXT("Jersey")))Mesh->SetMaterial(I,Shirt);
            else if(Name.Contains(TEXT("Bottom"))||Name.Contains(TEXT("Trouser")))Mesh->SetMaterial(I,Trousers);
            // Eyes and lashes before the body test: both are their own island on the head and both
            // spent the whole project on the engine default material.
            else if(Name.Contains(TEXT("Eyelash")))Mesh->SetMaterial(I,Lash);
            else if(Name.Contains(TEXT("Eye")))Mesh->SetMaterial(I,Eyes);
            // The textured face when the atlas resolved, otherwise this player's own flat tone.
            else if(Name.Contains(TEXT("Body")))Mesh->SetMaterial(I,FaceMat);
            if(Name.Contains(TEXT("Hair")))
                for(int LOD=0;LOD<S->GetLODNum();++LOD)
                    Mesh->ShowMaterialSection(I,0,Role!=EC26Role::Batter&&Role!=EC26Role::Keeper,LOD);
            if(Name.Contains(TEXT("Bottom")))
                for(int LOD=0;LOD<S->GetLODNum();++LOD)Mesh->ShowMaterialSection(I,0,true,LOD);
        }
    }
    Uniform->SetMaterial(2,Shirt);Uniform->SetMaterial(0,Trousers);Uniform->SetMaterial(1,Make(Team==0?FLinearColor(.10,.40,.43):FLinearColor(.72,.24,.07),.88f));
    Uniform->SetMaterial(3,Shirt);Uniform->SetMaterial(4,Shirt);Uniform->SetMaterial(5,Shirt);
    ShirtNumber->SetText(FText::AsNumber(Number));

    // Authored kit materials. The old kit ran on three instances -- one for the bat, one for the
    // helmet, one grey "Gear" shared by pads and gloves -- so every piece of protective equipment
    // returned the same value under the floodlights and the batter's hands, pads and helmet
    // merged into one pale mass at gameplay distance. Each authored slot now gets its own tone
    // and, more importantly, its own roughness, because roughness separation is what actually
    // reads at 900 cm: a hard helmet shell, matte pad fronts, a leather palm, a rubber grip and
    // a satin blade must not all return the same highlight.
    const FLinearColor Accent=Role==EC26Role::Umpire?FLinearColor(.28,.30,.33):Kit;
    auto* Willow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Willow.M_Willow"));

    // The willow is a natural material: longitudinal grain along the blade, matte, a satin rather
    // than a gloss highlight. Its grain is authored along the blade so it tiles low.
    Dress(Bat,TEXT("Willow"),Textured(Willow,FLinearColor(.402,.330,.196),.42f,.55f,.10f,4.f,.9f));
    // Rubber grip, cane splice and twine binding are all leather-family surfaces: coarse grain,
    // very matte, no sheen. The grip tiles high because the handle is a small object.
    Dress(Bat,TEXT("Grip"),Textured(GearMat,FLinearColor(.016,.018,.022),.88f,.30f,.05f,14.f,.8f));
    Dress(Bat,TEXT("Cane"),Textured(GearMat,FLinearColor(.300,.222,.118),.56f,.60f,.08f,8.f,.9f));
    Dress(Bat,TEXT("Twine"),Textured(GearMat,FLinearColor(.052,.046,.040),.80f,.85f,.04f,30.f,1.f));
    Dress(Bat,TEXT("Label"),Make(Accent*1.15f+FLinearColor(.03,.03,.03),.34f));

    // The helmet is moulded polycarbonate: the one hard surface on a cricketer. It takes the shell
    // material, which is the only one here with a genuinely low roughness and a real specular, so
    // it returns a tight highlight the way a helmet does and the pads do not. The peak is the same
    // plastic; the crown and the padding inside are not.
    Dress(Headwear,TEXT("Shell"),Textured(ShellMat,Kit*.92f,.24f,.24f,.05f,12.f,.55f));
    Dress(Headwear,TEXT("Crown"),Textured(ClothMat,Kit*.92f,.86f,.55f,.45f,22.f,.8f));
    Dress(Headwear,TEXT("Peak"),Textured(ShellMat,Kit*.66f,.28f,.30f,.05f,14.f,.6f));
    Dress(Headwear,TEXT("Trim"),Textured(GearMat,FLinearColor(.022,.024,.029),.62f,.40f,.06f,16.f,.8f));
    Dress(Headwear,TEXT("Pad"),Textured(GearMat,FLinearColor(.036,.034,.032),.93f,.75f,.03f,18.f,1.f));
    // Grille bars are round-section titanium: high specular, almost no grain, low roughness. A
    // grille that returns the same value as the shell it sits in is a grille nobody can see.
    Dress(Grill,TEXT("Bar"),Textured(ShellMat,FLinearColor(.300,.318,.348),.18f,.12f,.04f,26.f,.4f));
    Dress(Grill,TEXT("Trim"),Textured(GearMat,FLinearColor(.022,.024,.029),.62f,.40f,.06f,16.f,.8f));

    // Pads and gloves read as protective gear because they are matte and slightly off-white, and
    // because the straps and buckles that break them up are dark and sharp against that. The pad
    // face is the largest single surface on a striker and it used to be one flat colour; it now
    // carries a grain, and the knee roll -- which is a different, softer material in real gear --
    // takes a lower normal strength so it reads round rather than stamped.
    auto* PadFace=Textured(GearMat,FLinearColor(.700,.712,.686),.78f,.50f,.16f,7.f,.85f);
    auto* PadRoll=Textured(GearMat,FLinearColor(.612,.624,.600),.84f,.62f,.20f,10.f,1.f);
    auto* Strap=Textured(GearMat,FLinearColor(.028,.030,.036),.70f,.70f,.05f,22.f,.9f);
    auto* Buckle=Textured(ShellMat,FLinearColor(.330,.342,.362),.26f,.25f,.05f,18.f,.5f);
    for(UStaticMeshComponent* P:{PadL.Get(),PadR.Get()})
    {
        Dress(P,TEXT("PadFace"),PadFace);Dress(P,TEXT("PadRoll"),PadRoll);
        Dress(P,TEXT("PadStrap"),Strap);Dress(P,TEXT("PadBuckle"),Buckle);
    }
    // A batting glove is leather on the palm and cloth padding on the back of the hand, and the
    // two have to return different highlights or the hand reads as one white lump.
    auto* GlovePalm=Textured(GearMat,FLinearColor(.212,.150,.098),.52f,.80f,.10f,20.f,1.f);
    auto* GlovePad=Textured(GearMat,FLinearColor(.732,.744,.716),.76f,.55f,.18f,9.f,.9f);
    for(UStaticMeshComponent* G:{GloveL.Get(),GloveR.Get()})
    {
        Dress(G,TEXT("GlovePalm"),GlovePalm);Dress(G,TEXT("GlovePad"),GlovePad);
        Dress(G,TEXT("GloveCuff"),PadRoll);Dress(G,TEXT("PadStrap"),Strap);
    }
    // Cricket shoes, not the base character's street trainers. Under the night rig those read as
    // two black holes exactly where the athlete meets the turf, which is the worst place in the
    // frame to lose contrast: the feet are what ground the player. The upper is a coated textile,
    // the sole is the hard shell, and the flash is the only piece of kit carrying the team colour
    // at ground level.
    auto* ShoeUpper=Textured(GearMat,FLinearColor(.740,.752,.734),.48f,.45f,.14f,16.f,.75f);
    auto* ShoeSole=Textured(ShellMat,FLinearColor(.048,.052,.062),.42f,.35f,.05f,20.f,.6f);
    auto* ShoeFlash=Make(Accent,.40f);
    auto* ShoeLace=Textured(GearMat,FLinearColor(.520,.528,.512),.90f,.95f,.04f,34.f,1.f);
    for(UStaticMeshComponent* S:{ShoeL.Get(),ShoeR.Get()})
    {
        Dress(S,TEXT("ShoeUpper"),ShoeUpper);Dress(S,TEXT("ShoeSole"),ShoeSole);
        Dress(S,TEXT("ShoeFlash"),ShoeFlash);Dress(S,TEXT("ShoeLace"),ShoeLace);
        Dress(S,TEXT("ShoeSpike"),Buckle);
    }

    Bat->SetVisibility(false); Bat->SetHiddenInGame(true);
    Headwear->SetVisibility(false); Headwear->SetHiddenInGame(true);
    Grill->SetVisibility(false); Grill->SetHiddenInGame(true);
    PadL->SetVisibility(false); PadL->SetHiddenInGame(true);
    PadR->SetVisibility(false); PadR->SetHiddenInGame(true);
    GloveL->SetVisibility(false); GloveL->SetHiddenInGame(true);
    GloveR->SetVisibility(false); GloveR->SetHiddenInGame(true);
    ShoeL->SetVisibility(false); ShoeL->SetHiddenInGame(true);
    ShoeR->SetVisibility(false); ShoeR->SetHiddenInGame(true);
    Uniform->SetVisibility(false); Uniform->SetHiddenInGame(true);
    ShirtNumber->SetVisibility(false); ShirtNumber->SetHiddenInGame(true);
    Shade->SetVisibility(false); Shade->SetHiddenInGame(true);
    Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true);

    // Use the authored, deformable cricket kit on the shared animation skeleton.
    // The generated role meshes have incompatible bind poses and baked equipment.
    bHeroVisual=false;
    HeroMesh->SetStaticMesh(nullptr);
    RebuildReference();
    UE_LOG(LogC26,Display,TEXT("C26_PLAYER_REFRESH role=%d mesh=%s run=%d idle=%d textured_skin=%d"),
        int(Role),*Mesh->GetSkinnedAsset()->GetName(),RunClip!=nullptr,IdleClip!=nullptr,TexturedSkin!=nullptr);

    ApplyVisualRole();
    SetAction(EC26Action::Ready);
}
void AC26Athlete::ApplyVisualRole()
{
    if(Presentation&&Presentation->IsActive()){Presentation->Configure(this);return;}
    // Exactly one visible body per athlete. Hero scans carry pads/helmet/bat baked in;
    // the animated kit shows team clothing with shoes, and role-appropriate separate
    // equipment is gated in ApplyDetail. Switching roles re-runs Configure, which lands
    // here, so no stale mesh survives an innings change.
    const bool Hero=bHeroVisual&&HeroMesh&&HeroMesh->GetStaticMesh();
    bHeroVisual=Hero;
    if(HeroMesh){HeroMesh->SetVisibility(Hero);HeroMesh->SetHiddenInGame(!Hero);}
    Mesh->SetVisibility(!Hero);Mesh->SetHiddenInGame(Hero);
    Uniform->SetVisibility(!Hero&&!AuthoredKit);Uniform->SetHiddenInGame(Hero||AuthoredKit);
    const bool Shadowed=!Hero&&Shade&&Shade->GetNumSections()>0;
    Shade->SetVisibility(Shadowed);Shade->SetHiddenInGame(!Shadowed);
    ShirtNumber->SetVisibility(!Hero);ShirtNumber->SetHiddenInGame(Hero);
    ShoeL->SetVisibility(!Hero);ShoeL->SetHiddenInGame(Hero);
    ShoeR->SetVisibility(!Hero);ShoeR->SetHiddenInGame(Hero);
    Bat->SetVisibility(false);Bat->SetHiddenInGame(true);
    Headwear->SetVisibility(false);Headwear->SetHiddenInGame(true);
    Grill->SetVisibility(false);Grill->SetHiddenInGame(true);
    PadL->SetVisibility(false);PadL->SetHiddenInGame(true);
    PadR->SetVisibility(false);PadR->SetHiddenInGame(true);
    GloveL->SetVisibility(false);GloveL->SetHiddenInGame(true);
    GloveR->SetVisibility(false);GloveR->SetHiddenInGame(true);
    ApplyDetail();
}
int AC26Athlete::Bone(const FString& Name)const{const int* I=Bones.Find(Name);return I?*I:INDEX_NONE;}
void AC26Athlete::RebuildChildren(int Index)
{
    for(int I=Index+1;I<Pose.Num();++I)
    {
        int P=Parents[I];bool Desc=false;while(P>=0){if(P==Index){Desc=true;break;}P=Parents[P];}
        if(Desc){const FTransform Local=Reference[I].GetRelativeTransform(Reference[Parents[I]]);Pose[I]=Local*Pose[Parents[I]];}
    }
}
void AC26Athlete::MoveBone(const FString& Name,const FVector& Offset)
{int I=Bone(Name);if(I<0)return;Pose[I].AddToTranslation(Offset);RebuildChildren(I);}
void AC26Athlete::Aim(const FString& Name,const FString& Child,const FVector& Target)
{
    int I=Bone(Name),J=Bone(Child);if(I<0||J<0)return;
    FQuat Q=FQuat::FindBetweenVectors((Pose[J].GetLocation()-Pose[I].GetLocation()).GetSafeNormal(),(Target-Pose[I].GetLocation()).GetSafeNormal());
    Pose[I].SetRotation((Q*Pose[I].GetRotation()).GetNormalized());RebuildChildren(I);
}
void AC26Athlete::Twist(const FString& Name,float TurnRight,float LeanForward,float LeanRight)
{
    const int I=Bone(Name);if(I<0)return;
    // Rotations are expressed as an athlete would describe them, about the mesh axes, pivoting on
    // the bone itself. This is how the batter is opened up side-on without moving any IK target.
    const FQuat Q=FQuat(FVector(0,0,1),FMath::DegreesToRadians(TurnRight))
        *FQuat(FVector(-1,0,0),FMath::DegreesToRadians(LeanForward))
        *FQuat(FVector(0,-1,0),FMath::DegreesToRadians(LeanRight));
    Pose[I].SetRotation((Q*Pose[I].GetRotation()).GetNormalized());RebuildChildren(I);
}
void AC26Athlete::AimHead()
{
    const int H=Bone(TEXT("Head")),Nk=Bone(TEXT("Neck"));
    if(H<0||!Pose.IsValidIndex(H))return;
    // Work out which head-local axis currently points out of the face, then rotate that onto the
    // target. Aiming the neck-to-head bone vector instead tips the skull over sideways.
    auto Face=[&](int Index)
    {
        const FVector L=Reference[Index].GetRotation().UnrotateVector(RigForward);
        return Pose[Index].GetRotation().RotateVector(L).GetSafeNormal();
    };
    FVector Want;
    if(LookAt.IsZero())
    {
        // Nobody plays cricket looking at their own boots. The forward lean that makes a stance
        // read as loaded -- 25 degrees for a batter, 42 through a gather, 24 for a keeper -- also
        // pitches the skull down with it, and a head aimed at the turf is what makes a posed
        // figure read as a mannequin no matter how good the rest of the pose is. With no explicit
        // target the gaze is levelled back toward the horizon and left there.
        const FVector Held=Face(H);
        Want=FVector(Held.X,Held.Y,Held.Z*.22f).GetSafeNormal();
    }
    else
    {
        const FVector Local=Mesh->GetComponentTransform().InverseTransformPosition(LookAt);
        Want=Local-Pose[H].GetLocation();Want.Z*=.6f;Want=Want.GetSafeNormal();
    }
    if(Want.IsNearlyZero())return;
    auto TurnTo=[&](int Index,float Amount,float MaxDegrees)
    {
        if(Index<0)return;
        FQuat Delta=FQuat::FindBetweenNormals(Face(Index),Want);
        FVector Axis;float Angle;Delta.ToAxisAndAngle(Axis,Angle);
        Angle=FMath::Clamp(Angle*Amount,-FMath::DegreesToRadians(MaxDegrees),FMath::DegreesToRadians(MaxDegrees));
        Pose[Index].SetRotation((FQuat(Axis,Angle)*Pose[Index].GetRotation()).GetNormalized());
        RebuildChildren(Index);
    };
    TurnTo(Nk,.40f,26.f);TurnTo(H,1.f,68.f);
}
void AC26Athlete::Limb(const FString& Upper,const FString& Lower,const FString& End,const FVector& Target,const FVector& Bend)
{
    int A=Bone(Upper),B=Bone(Lower),C=Bone(End);if(A<0||B<0||C<0)return;
    const FVector Start=Pose[A].GetLocation();const float L1=(Reference[B].GetLocation()-Reference[A].GetLocation()).Size();
    const float L2=(Reference[C].GetLocation()-Reference[B].GetLocation()).Size();
    const float D=FMath::Clamp((Target-Start).Size(),FMath::Abs(L1-L2)+.01f,L1+L2-.01f);
    const FVector Dir=(Target-Start).GetSafeNormal();const float X=(L1*L1-L2*L2+D*D)/(2*D);
    const FVector Normal=(Bend-Dir*FVector::DotProduct(Bend,Dir)).GetSafeNormal();
    const FVector Elbow=Start+Dir*X+Normal*FMath::Sqrt(FMath::Max(0.f,L1*L1-X*X));
    Aim(Upper,Lower,Elbow);Aim(Lower,End,Target);
}
float AC26Athlete::PoseLag() const
{
    // Actions whose timing is load-bearing keep a short lag: the gate measures bat-ball contact
    // and ball release against these clocks, and a filter that dragged the bat 50 ms behind the
    // swing would move the contact point down the blade. Stances, gathers and the run can afford
    // a slower, softer approach because nothing is being measured against them.
    const bool Timed=Action==EC26Action::Batting||Action==EC26Action::Bowling
        ||Action==EC26Action::Throw||Action==EC26Action::Catch||Action==EC26Action::Dive;
    const float Base=Timed?.022f:Action==EC26Action::Pickup?.038f:.058f;
    // A pose only re-solved every second or third frame needs a filter slow enough to bridge the
    // gap, or the smoothing becomes the stutter it was added to remove.
    return Detail==EDetail::Hero?Base:Base*(Detail==EDetail::Mid?1.7f:2.6f);
}
void AC26Athlete::SmoothPose(float Dt,bool Solved)
{
    if(Pose.Num()!=Parents.Num()||Pose.IsEmpty())return;
    if(Solved)
    {
        Goal.SetNum(Pose.Num());
        for(int I=0;I<Pose.Num();++I){const int P=Parents[I];Goal[I]=P<0?Pose[I]:Pose[I].GetRelativeTransform(Pose[P]);}
    }
    if(Goal.Num()!=Pose.Num())return;
    // Dt of zero is a caller asking for one exact instant -- an event pose, a reset, a measurement.
    // It gets the authored pose itself, with no history and no lag.
    const float Alpha=Dt>0.f&&Shown.Num()==Goal.Num()?1.f-FMath::Exp(-Dt/FMath::Max(.001f,PoseLag())):1.f;
    if(Alpha>=1.f)Shown=Goal;
    else for(int I=0;I<Shown.Num();++I)Shown[I].Blend(Shown[I],Goal[I],Alpha);
    // Everything downstream -- bat, pads, gloves, shoes, shirt number, contact shadow -- is placed
    // off the component pose, so it is rebuilt from what is actually on screen. Placing kit off the
    // unsmoothed target is how a bat separates from the hands holding it.
    for(int I=0;I<Pose.Num();++I){const int P=Parents[I];Pose[I]=P<0?Shown[I]:Shown[I]*Pose[P];}
}
void AC26Athlete::ShoulderReach(const FString& Side,const FVector& Target,float Amount)
{
    const int S=Bone(Side+TEXT("Shoulder")),A=Bone(Side+TEXT("Arm"));
    if(S<0||A<0||!Pose.IsValidIndex(A))return;
    const FVector Root=Pose[S].GetLocation();
    const FVector Have=(Pose[A].GetLocation()-Root).GetSafeNormal();
    const FVector Want=(Target-Root).GetSafeNormal();
    if(Have.IsNearlyZero()||Want.IsNearlyZero())return;
    FVector Axis;float Angle;
    FQuat::FindBetweenNormals(Have,Want).ToAxisAndAngle(Axis,Angle);
    // The collarbone contributes, it does not lead. Clamped hard because an unclamped socket
    // rotation tears the deltoid weights apart on a full overhead reach.
    Angle=FMath::Clamp(Angle*Amount,-FMath::DegreesToRadians(30.f),FMath::DegreesToRadians(30.f));
    Pose[S].SetRotation((FQuat(Axis,Angle)*Pose[S].GetRotation()).GetNormalized());
    RebuildChildren(S);
}
void AC26Athlete::CurlFingers(const FString& Side,float Amount)
{
    if(Amount<=0.f)return;
    const int W=Bone(Side+TEXT("Hand")),K=Bone(Side+TEXT("HandMiddle1"));
    const int First=Bone(Side+TEXT("HandIndex1")),Last=Bone(Side+TEXT("HandPinky1"));
    if(W<0||K<0||First<0||Last<0||!Pose.IsValidIndex(Last))return;
    // The palm's inward normal, derived from the posed hand itself rather than assumed from a bone
    // axis convention: the line out of the wrist crossed with the line across the knuckles. The
    // thumb sits on the flexion side, which resolves the sign on any rig.
    FVector Normal=FVector::CrossProduct(Pose[K].GetLocation()-Pose[W].GetLocation(),
        Pose[Last].GetLocation()-Pose[First].GetLocation()).GetSafeNormal();
    if(Normal.IsNearlyZero())return;
    const int Thumb=Bone(Side+TEXT("HandThumb2"));
    if(Thumb>=0&&Pose.IsValidIndex(Thumb)
        &&FVector::DotProduct(Normal,Pose[Thumb].GetLocation()-Pose[W].GetLocation())<0.f)Normal=-Normal;
    // Knuckle, middle joint, tip. A relaxed hand closes most at the middle joint, which is what
    // gives a slack hand its curve instead of the even arc of a cartoon fist.
    const float Joint[3]={50.f,62.f,42.f};
    for(const TCHAR* Finger:{TEXT("Thumb"),TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Pinky")})
    {
        int Chain[4],Count=0;
        for(int J=1;J<=4&&Count<4;++J)
        {
            const int B=Bone(Side+TEXT("Hand")+Finger+FString::FromInt(J));
            if(B<0||!Pose.IsValidIndex(B))break;
            Chain[Count++]=B;
        }
        if(Count<3)continue;
        // A thumb opposes across the palm rather than folding into it, so it takes far less of the
        // same rotation; left at full curl it drives straight through the fingers.
        const float Reach=FCString::Strcmp(Finger,TEXT("Thumb"))==0?.38f:1.f;
        for(int J=0;J+1<Count;++J)
        {
            const FVector Along=(Pose[Chain[J+1]].GetLocation()-Pose[Chain[J]].GetLocation()).GetSafeNormal();
            const FVector Axis=FVector::CrossProduct(Along,Normal).GetSafeNormal();
            if(Axis.IsNearlyZero())continue;
            Pose[Chain[J]].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Joint[J]*Amount*Reach))
                *Pose[Chain[J]].GetRotation()).GetNormalized());
            // A finger is a leaf chain: re-deriving the rest of this one is the whole update, and
            // it avoids a full-skeleton descendant walk fifteen times per hand.
            for(int N=J+1;N<Count;++N)
                Pose[Chain[N]]=Reference[Chain[N]].GetRelativeTransform(Reference[Chain[N-1]])*Pose[Chain[N-1]];
        }
    }
}
void AC26Athlete::SetAction(EC26Action NewAction,bool ResetTime)
{
    const bool Fresh=NewAction!=Action||ResetTime;
    // A gather is the end of a chase, so the chase has to be remembered across the boundary. The
    // match code squares the fielder up and zeroes MoveSpeed immediately after this call, and it
    // then solves a pose with Dt of zero -- which snaps ShownSpeed to zero too. The speed and
    // stride phase are therefore only readable here, on the frame the action changes.
    if(Fresh&&NewAction==EC26Action::Pickup)
    {
        ApproachSpeed=FMath::Max(ShownSpeed,FMath::Max(0.f,MoveSpeed));
        ApproachGait=GaitPhase;
    }
    if(Fresh)ActionTime=0;
    Action=NewAction;
}
void AC26Athlete::ResetAt(const FVector& Position,float Yaw)
{SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));if(Presentation&&Presentation->IsActive())Presentation->ResetMotion();MotionTime=0;MoveSpeed=0;GaitPhase=0;ApproachSpeed=0;ApproachGait=0;Trigger=0;ContactTarget=FVector::ZeroVector;SetAction(EC26Action::Ready);Animate(0);}
void AC26Athlete::SetShotContact(const FVector& Target,float Angle,bool bLoft)
{ContactTarget=Target;ShotAngle=Angle;Loft=bLoft;SetAction(EC26Action::Batting);}
FVector AC26Athlete::Palm(bool Right) const
{
    const FString Side=Right?TEXT("Right"):TEXT("Left");
    const int Wrist=Bone(Side+TEXT("Hand"));
    if(Wrist<0||!Pose.IsValidIndex(Wrist))return FVector::ZeroVector;
    const int Knuckle=Bone(Side+TEXT("HandMiddle1")),Tip=Bone(Side+TEXT("HandMiddle3"));
    if(Knuckle>=0&&Tip>=0&&Pose.IsValidIndex(Tip))
        return FMath::Lerp(Pose[Knuckle].GetLocation(),Pose[Tip].GetLocation(),.72f);
    const int Fore=Bone(Side+TEXT("ForeArm"));
    const FVector Along=Fore>=0&&Pose.IsValidIndex(Fore)
        ?(Pose[Wrist].GetLocation()-Pose[Fore].GetLocation()).GetSafeNormal():FVector(0,0,-1);
    return Pose[Wrist].GetLocation()+Along*PalmReach;
}
FVector AC26Athlete::HandPosition() const
{
    if(Presentation&&Presentation->IsActive())return Presentation->BallHandPosition();
    const int I=Bone(TEXT("RightHand"));
    if(I<0||!Pose.IsValidIndex(I))return GetActorLocation()+FVector(0,0,210);
    return Mesh->GetComponentTransform().TransformPosition(Palm(true));
}
FVector AC26Athlete::ReceivingPosition() const
{
    if(Presentation&&Presentation->IsActive())return Presentation->ReceivePosition();
    const int L=Bone(TEXT("LeftHand")),R=Bone(TEXT("RightHand"));
    if(L<0||R<0)return HandPosition();
    return Mesh->GetComponentTransform().TransformPosition((Palm(false)+Palm(true))*.5f);
}
void AC26Athlete::PlaceKit(const FVector& Grip,const FVector& Dir,bool Batting,bool Running)
{
    if(bHeroVisual && HeroMesh && HeroMesh->GetStaticMesh())
    {
        Bat->SetVisibility(false);
        Headwear->SetVisibility(false);
        Grill->SetVisibility(false);
        PadL->SetVisibility(false);
        PadR->SetVisibility(false);
        GloveL->SetVisibility(false);
        GloveR->SetVisibility(false);
        ShoeL->SetVisibility(false);
        ShoeR->SetVisibility(false);
        ShirtNumber->SetVisibility(false);
        return;
    }
    const int Head=Bone(TEXT("Head"));
    if(Head>=0)
    {
        const FVector L=Reference[Head].GetRotation().UnrotateVector(RigForward);
        const FVector Face=Pose[Head].GetRotation().RotateVector(L).GetSafeNormal();
        const FQuat HeadDelta=Pose[Head].GetRotation()*Reference[Head].GetRotation().Inverse();
        const FVector Up=HeadDelta.RotateVector(FVector::UpVector);
        const FRotator Look=FRotationMatrix::MakeFromXZ(Face,Up).Rotator();
        // Crown offset belongs to the head frame, including its nod/roll, not world vertical.
        const FVector Skull=Pose[Head].GetLocation()+Up*10.5f+Face*.8f;
        Headwear->SetRelativeLocation(Skull);Headwear->SetRelativeRotation(Look);
        Grill->SetRelativeLocation(Skull);Grill->SetRelativeRotation(Look);
    }
    // Everything below hangs off joints that were actually posed, never off the targets the shot
    // asked for. Two-bone IK silently clamps at arm's length, so a target the arms cannot reach
    // used to leave the blade hanging in mid-air beside the batter.
    const int Pelvis=Bone(TEXT("Hips"));
    const FVector Facing=Pelvis>=0
        ?Pose[Pelvis].GetRotation().RotateVector(Reference[Pelvis].GetRotation().UnrotateVector(RigForward)).GetSafeNormal(UE_SMALL_NUMBER,RigForward)
        :RigForward;
    // Where a hand actually holds something: part way from the wrist joint out to the knuckles.
    auto Palm=[this](const FString& Side)
    {
        const int W=Bone(Side+TEXT("Hand")),K=Bone(Side+TEXT("HandMiddle1"));
        if(W<0)return FVector::ZeroVector;
        return K>=0?FMath::Lerp(Pose[W].GetLocation(),Pose[K].GetLocation(),.55f):Pose[W].GetLocation();
    };
    if(Batting)
    {
        const FVector Top=Palm(TEXT("Left")),Bottom=Palm(TEXT("Right"));
        // Top hand high on the handle, bottom hand a fist below it: the span between the two posed
        // palms is the handle, so the bat physically cannot separate from the grip.
        FVector Shaft=Top-Bottom;
        Shaft=Shaft.SizeSquared()>16.f?Shaft.GetSafeNormal():Dir;
        // Point the blade face along the shot so the bat meets the ball with a plausible face angle.
        FVector FaceDir=Running?RigForward:FVector(-FMath::Sin(FMath::DegreesToRadians(ShotAngle)),FMath::Cos(FMath::DegreesToRadians(ShotAngle)),0);
        // A horizontal-bat stroke can line the shaft up with the shot direction, which leaves the
        // blade roll undefined. Fall back to a reference across the shaft instead of letting it spin.
        if(FMath::Abs(FVector::DotProduct(Shaft,FaceDir))>.94f)
            FaceDir=FVector::CrossProduct(Shaft,FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER,RigForward);
        Bat->SetRelativeLocation((Top.IsZero()?Grip:Top)+Shaft*4.5f);
        Bat->SetRelativeRotation(FRotationMatrix::MakeFromZX(Shaft,FaceDir).Rotator());
    }
    auto PlacePad=[&](UStaticMeshComponent* Pad,const FString& Knee,const FString& Foot)
    {
        const int K=Bone(Knee),F=Bone(Foot);if(K<0||F<0)return;
        const FVector Kn=Pose[K].GetLocation(),Ft=Pose[F].GetLocation();
        const FVector Shin=Kn-Ft;
        // A cricket pad runs from above the knee roll down to the instep and wraps the front of the
        // shin. Centring it on the knee-ankle midpoint with a fixed mesh-space offset left the roll
        // off the top and pushed the pad sideways as soon as the batter turned side-on.
        Pad->SetRelativeLocation(FMath::Lerp(Ft,Kn,.56f)+Shin.GetSafeNormal()*3.5f+Facing*.8f);
        Pad->SetRelativeRotation(FRotationMatrix::MakeFromZX(Shin,Facing).Rotator());
    };
    PlacePad(PadL,TEXT("LeftLeg"),TEXT("LeftFoot"));PlacePad(PadR,TEXT("RightLeg"),TEXT("RightFoot"));
    auto PlaceGlove=[&](UStaticMeshComponent* Glove,const FString& Side)
    {
        const int H=Bone(Side+TEXT("Hand")),F=Bone(Side+TEXT("ForeArm"));if(H<0||F<0)return;
        const FVector Along=(Pose[H].GetLocation()-Pose[F].GetLocation()).GetSafeNormal(UE_SMALL_NUMBER,RigForward);
        // Centre the glove on the palm, not the wrist, so the padding swallows the fingers instead
        // of leaving them poking out of the front.
        Glove->SetRelativeLocation(FMath::Lerp(Pose[H].GetLocation(),Palm(Side),1.15f)+Along*1.5f);
        Glove->SetRelativeRotation(FRotationMatrix::MakeFromZX(Along,Facing).Rotator());
    };
    PlaceGlove(GloveL,TEXT("Left"));PlaceGlove(GloveR,TEXT("Right"));
    // The shoe is authored standing on its own sole with the origin at ground level under the
    // ankle, so it is placed by dropping from the ankle joint along the foot's OWN up axis. Using
    // world up instead would leave the shoe flat on the turf while the foot rolled onto its toe,
    // which is exactly the moment -- the back foot of a drive, every stride of the run-up -- that
    // a viewer looks at the feet.
    auto PlaceShoe=[&](UStaticMeshComponent* Shoe,const FString& Side)
    {
        const int F=Bone(Side+TEXT("Foot")),T=Bone(Side+TEXT("ToeBase"));
        if(F<0)return;
        const FQuat Roll=Pose[F].GetRotation()*Reference[F].GetRotation().Inverse();
        const FVector Up=Roll.RotateVector(FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector);
        FVector Ahead=T>=0?Pose[T].GetLocation()-Pose[F].GetLocation():Facing;
        Ahead-=Up*FVector::DotProduct(Ahead,Up);
        Shoe->SetRelativeLocation(Pose[F].GetLocation()-Up*(AnkleZ-0.6f));
        Shoe->SetRelativeRotation(FRotationMatrix::MakeFromXZ(Ahead.GetSafeNormal(UE_SMALL_NUMBER,Facing),Up).Rotator());
    };
    PlaceShoe(ShoeL,TEXT("Left"));PlaceShoe(ShoeR,TEXT("Right"));
    const int Chest=Bone(TEXT("Spine2"));
    if(Chest>=0)
    {
        const FVector Forward=Pose[Chest].GetRotation().RotateVector(Reference[Chest].GetRotation().UnrotateVector(RigForward));
        ShirtNumber->SetRelativeLocation(Pose[Chest].GetLocation()-Forward*12.f-FVector(0,0,10));
        ShirtNumber->SetRelativeRotation(FRotationMatrix::MakeFromXZ(-Forward,FVector::UpVector).Rotator());
    }
}
void AC26Athlete::UpdateUniform()
{
    SCOPE_CYCLE_COUNTER(STAT_C26_Uniform);
    if(bHeroVisual && HeroMesh && HeroMesh->GetStaticMesh())
    {
        Uniform->SetVisibility(false);
        return;
    }
    // Lightweight cloth envelope follows the same posed joints as the skin. No cloth simulation
    // and no extra skeletons; distant players can update this with their reduced pose cadence.
    TArray<FVector> Vertices,Normals,StripeV,StripeN,SleeveV,SleeveN;TArray<int32> Indices,StripeT,SleeveT;
    TArray<FVector2D> UV,StripeUV,SleeveUV;TArray<FLinearColor> Colors;TArray<FProcMeshTangent> Tangents;
    // A tube's ring frame is built by crossing the limb against a reference axis. RigForward alone
    // degenerates whenever a limb points along it -- a fully horizontal dive or reach -- and the
    // cross product collapses toward zero, taking every normal on that ring with it. This is a
    // latent guard, not a fix for anything currently on screen: the run-up's bright leading thigh
    // measures the same luma with and without it, so that contrast is real key light on a raised
    // thigh against a self-shadowed trailing leg. Roll the reference toward vertical as the limb
    // approaches horizontal; a circular tube is rotationally symmetric, so the roll costs nothing.
    auto Upright=[](const FVector& Along)
    {
        const float Align=FMath::Abs(FVector::DotProduct(Along,RigForward));
        return FMath::Lerp(RigForward,FVector::UpVector,FMath::SmoothStep(.84f,.99f,Align)).GetSafeNormal();
    };
    auto Leg=[&](const FString& Side,float Sign)
    {
        const int H=Bone(Side+TEXT("UpLeg")),K=Bone(Side+TEXT("Leg")),F=Bone(Side+TEXT("Foot"));
        if(H<0||K<0||F<0)return;
        const FVector Top=Pose[H].GetLocation()+FVector(0,0,10),Knee=Pose[K].GetLocation(),Foot=Pose[F].GetLocation()+FVector(0,0,2);
        const FVector Centers[]={Top,FMath::Lerp(Top,Knee,.45f),Knee,FMath::Lerp(Knee,Foot,.5f),Foot};
        // Radii in centimetres at hip, mid-thigh, knee, mid-calf and ankle. These were roughly twice
        // life size, which inflated the legs into a toy silhouette and pushed the trouser out through
        // the pads. A 185 cm athlete measures about this. The knee ring runs slightly full so a bent
        // front knee never peeks skin through the cloth in a replay close-up. The hip ring runs full
        // so the base-mesh waist never peeks out between shirt and trouser now that skin is bright.
        // Mid-thigh and knee stay a touch proud of the base mesh for the same reason: the imported
        // thighs are heavier than a tailor's chart, and skin poking through reads far worse than a
        // slightly fuller leg. Both still sit well inside the pads.
        const float Widths[]={11.6f,10.2f,8.4f,7.0f,5.5f};
        const int Base=Vertices.Num();constexpr int Sides=12;
        for(int Row=0;Row<5;++Row)
        {
            const FVector Along=(Centers[FMath::Min(4,Row+1)]-Centers[FMath::Max(0,Row-1)]).GetSafeNormal();
            const FVector Across=FVector::CrossProduct(Along,Upright(Along)).GetSafeNormal();
            const FVector Front=FVector::CrossProduct(Across,Along).GetSafeNormal();
            for(int J=0;J<Sides;++J)
            {
                const float A=2*PI*J/Sides;
                const FVector N=Across*FMath::Cos(A)+Front*FMath::Sin(A);
                Vertices.Add(Centers[Row]+N*Widths[Row]);Normals.Add(N);UV.Add(FVector2D(J/float(Sides),Row*.25f));
            }
            const FVector Outside=Across*Sign;
            for(int Edge:{-1,1})
            {StripeV.Add(Centers[Row]+Outside*(Widths[Row]+.12f)+Front*(Edge*.65f));StripeN.Add(Outside);StripeUV.Add(FVector2D(Edge>0?1:0,Row*.25f));}
        }
        for(int R=0;R<4;++R)for(int J=0;J<Sides;++J)
        {const int A=Base+R*Sides+J,B=Base+R*Sides+(J+1)%Sides;Indices.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
        const int SB=StripeV.Num()-10;
        for(int R=0;R<4;++R){int A=SB+R*2;StripeT.Append({A,A+1,A+2,A+1,A+3,A+2});}
    };
    Leg(TEXT("Left"),-1);Leg(TEXT("Right"),1);
    // A short shirt sleeve down to mid-bicep, with a hem that flares. Without it the kit ends at
    // the shoulder and the arm reads as bare skin growing straight out of a smooth teal volume,
    // which is what made the torso look like a balloon in replay close-ups.
    auto Sleeve=[&](const FString& Side)
    {
        const int Shoulder=Bone(Side+TEXT("Arm")),Elbow=Bone(Side+TEXT("ForeArm"));
        if(Shoulder<0||Elbow<0)return;
        const FVector Top=Pose[Shoulder].GetLocation(),Bend=Pose[Elbow].GetLocation();
        const FVector Along=(Bend-Top).GetSafeNormal(UE_SMALL_NUMBER,-RigForward);
        // Row 0 sits back inside the torso so the seam never opens at the shoulder.
        const FVector Centers[]={Top-Along*5.f,FMath::Lerp(Top,Bend,.24f),FMath::Lerp(Top,Bend,.44f),FMath::Lerp(Top,Bend,.49f)};
        // Shoulder, upper bicep, sleeve, hem. A 185 cm athlete's bicep is about 13 cm across; the
        // cloth sits just outside that, and only the hem flares.
        // Row 0 is buried inside the torso, so it only has to be wide enough to close the seam;
        // at 9.4 it pushed out past the deltoid and read as a shoulder pad.
        const float Widths[]={8.2f,7.4f,6.7f,7.1f};
        const FVector Across=FVector::CrossProduct(Along,Upright(Along)).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector);
        const FVector Front=FVector::CrossProduct(Across,Along).GetSafeNormal();
        const int Base=SleeveV.Num();constexpr int Sides=10;
        for(int Row=0;Row<4;++Row)for(int J=0;J<Sides;++J)
        {
            const float A=2*PI*J/Sides;
            const FVector N=Across*FMath::Cos(A)+Front*FMath::Sin(A);
            SleeveV.Add(Centers[Row]+N*Widths[Row]);SleeveN.Add(N);SleeveUV.Add(FVector2D(J/float(Sides),Row/3.f));
        }
        for(int R=0;R<3;++R)for(int J=0;J<Sides;++J)
        {const int A=Base+R*Sides+J,B=Base+R*Sides+(J+1)%Sides;SleeveT.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
    };
    Sleeve(TEXT("Left"));Sleeve(TEXT("Right"));
    // Tailored shirt details, posed off the same joints as the sleeves. The base-mesh shirt is one
    // smooth volume, so the collar, placket and hem band are what stop the torso reading as a
    // balloon in replay close-ups. Sections 3/4/5 take the Shirt material in Configure. Every
    // dimension derives from the posed bones, so nothing floats or clips as the batter moves.
    TArray<FVector> CollarV,CollarN,HemV,HemN,PlacketV,PlacketN;
    TArray<int32> CollarT,HemT,PlacketT;
    TArray<FVector2D> CollarUV,HemUV,PlacketUV;
    const int Neck=Bone(TEXT("Neck")),Head=Bone(TEXT("Head"));
    const int Spine2=Bone(TEXT("Spine2")),Pelvis=Bone(TEXT("Hips"));
    const FVector ChestFacing=Pelvis>=0
        ?Pose[Pelvis].GetRotation().RotateVector(Reference[Pelvis].GetRotation().UnrotateVector(RigForward)).GetSafeNormal(UE_SMALL_NUMBER,RigForward)
        :RigForward;
    if(Neck>=0)
    {
        const FVector NeckPos=Pose[Neck].GetLocation();
        const FVector Axis=(Head>=0?Pose[Head].GetLocation()-NeckPos:FVector(0,0,12.f)).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector);
        FVector Across=FVector::CrossProduct(Axis,Upright(Axis)).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector);
        const FVector Depth=FVector::CrossProduct(Across,Axis).GetSafeNormal(UE_SMALL_NUMBER,RigForward);
        // Collar: short flared band around the base of the neck. Bottom ring sits on the trapezius
        // so no gap opens when the head turns to track the ball.
        {
            const FVector Centers[]={NeckPos+Axis*.5f,NeckPos+Axis*4.8f};
            const float Widths[]={8.6f,7.6f};
            constexpr int Sides=12;
            const int Base=CollarV.Num();
            for(int Row=0;Row<2;++Row)for(int J=0;J<Sides;++J)
            {
                const float A=2*PI*J/Sides;
                const FVector N=Across*FMath::Cos(A)+Depth*FMath::Sin(A);
                CollarV.Add(Centers[Row]+N*Widths[Row]);CollarN.Add(N);CollarUV.Add(FVector2D(J/float(Sides),Row));
            }
            for(int J=0;J<Sides;++J)
            {const int A=Base+J,B=Base+(J+1)%Sides;CollarT.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
        }
        // Placket: narrow plate down the front of the chest. Single-sided geometry is enough; the
        // kit materials are two-sided. The middle row stands a touch prouder to follow the chest.
        if(Spine2>=0)
        {
            const FVector SideDir=FVector::CrossProduct(FVector::UpVector,ChestFacing).GetSafeNormal(UE_SMALL_NUMBER,Across);
            const FVector Top=NeckPos-Axis*1.5f+ChestFacing*11.5f;
            const FVector Mid=Top-Axis*6.5f+ChestFacing*.5f;
            const FVector Bot=Top-Axis*13.f;
            const FVector Rows[]={Top,Mid,Bot};
            const int Base=PlacketV.Num();constexpr int Cols=2;
            for(int Row=0;Row<3;++Row)for(int C=0;C<Cols;++C)
            {
                PlacketV.Add(Rows[Row]+SideDir*((C?1.f:-1.f)*1.7f));
                PlacketN.Add(ChestFacing);PlacketUV.Add(FVector2D(float(C),Row*.5f));
            }
            for(int Row=0;Row<2;++Row)
            {const int A=Base+Row*Cols;PlacketT.Append({A,A+1,A+Cols,A+1,A+Cols+1,A+Cols});}
        }
    }
    if(Pelvis>=0)
    {
        // Hem band: elliptical ring where the shirt meets the trousers, so the shirt reads as
        // tucked cloth with an edge rather than melting into the hips. Cut deliberately proud of
        // the base shirt -- a slightly loose hem reads as cloth, a flush one disappears entirely.
        const FVector C=Pose[Pelvis].GetLocation()+FVector(0,0,10.f);
        const FVector SideH=FVector::CrossProduct(FVector::UpVector,ChestFacing).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,1,0));
        constexpr int Sides=14;
        const int Base=HemV.Num();
        for(int Row=0;Row<2;++Row)for(int J=0;J<Sides;++J)
        {
            const float A=2*PI*J/Sides;
            // Hugs the waist. Cut proud enough to read as cloth and no further: at 18.2 x 13.2
            // the band stood clear of the shirt as a shelf around the hips, which is exactly the
            // silhouette a viewer reads as "this is not a real garment".
            const FVector Out=SideH*FMath::Cos(A)*15.2f+ChestFacing*FMath::Sin(A)*11.4f;
            HemV.Add(C+Out-FVector(0,0,Row*2.4f));HemN.Add(Out.GetSafeNormal());HemUV.Add(FVector2D(J/float(Sides),Row));
        }
        for(int J=0;J<Sides;++J)
        {const int A=Base+J,B=Base+(J+1)%Sides;HemT.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
    }
    if(Uniform->GetNumSections()<6)
    {
        Uniform->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(1,StripeV,StripeT,StripeN,StripeUV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(2,SleeveV,SleeveT,SleeveN,SleeveUV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(3,CollarV,CollarT,CollarN,CollarUV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(4,PlacketV,PlacketT,PlacketN,PlacketUV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(5,HemV,HemT,HemN,HemUV,Colors,Tangents,false);
    }
    else
    {
        Uniform->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(1,StripeV,StripeN,StripeUV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(2,SleeveV,SleeveN,SleeveUV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(3,CollarV,CollarN,CollarUV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(4,PlacketV,PlacketN,PlacketUV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(5,HemV,HemN,HemUV,Colors,Tangents);
    }
}
void AC26Athlete::ApplyRecordedMotion(bool Running,bool Batting)
{
    UAnimSequence* Clip=Running?RunClip.Get():IdleClip.Get();
    const bool Idle=!Running&&Action==EC26Action::Ready&&!Batting&&Role!=EC26Role::Keeper;
    if((!Running&&!Idle)||!Clip||!Clip->GetSkeleton()||Clip->GetPlayLength()<=0.f)return;
    const auto& Source=Clip->GetSkeleton()->GetReferenceSkeleton();
    const auto& Target=Mesh->GetSkinnedAsset()->GetRefSkeleton();
    const float Time=Running?FMath::Frac(GaitPhase/(2.f*PI))*Clip->GetPlayLength()
        :FMath::Fmod(MotionTime+SquadNumber*.37f,Clip->GetPlayLength());
    const FAnimExtractContext Context(Time,false);
    if(Idle)
    {
        // Layer the recorded breathing onto the role's ready stance, retaining its hand targets.
        for(const TCHAR* Name:{TEXT("Spine"),TEXT("Spine1"),TEXT("Spine2")})
        {
            const int I=Bone(Name);if(I<0)continue;
            const int J=Source.FindBoneIndex(Target.GetBoneName(I));if(J<0)continue;
            FTransform Now=Source.GetRefBonePose()[J],Start=Now;
            Clip->GetBoneTransform(Now,FSkeletonPoseBoneIndex(J),Context,false);
            Clip->GetBoneTransform(Start,FSkeletonPoseBoneIndex(J),FAnimExtractContext(0.0),false);
            const FQuat Delta=FQuat::Slerp(FQuat::Identity,Now.GetRotation()*Start.GetRotation().Inverse(),.45f);
            Pose[I].SetRotation((Pose[I].GetRotation()*Delta).GetNormalized());
            RebuildChildren(I);
        }
        return;
    }
    TArray<FTransform> Animated;Animated.SetNum(Pose.Num());
    const float Weight=FMath::SmoothStep(0.f,.16f,ActionTime);
    for(int I=0;I<Pose.Num();++I)
    {
        const int P=Parents[I];
        const FTransform Rest=P<0?Reference[I]:Reference[I].GetRelativeTransform(Reference[P]);
        FTransform Local=P<0?Pose[I]:Pose[I].GetRelativeTransform(Pose[P]);
        const int J=Source.FindBoneIndex(Target.GetBoneName(I));
        if(J>=0)
        {
            FTransform Sample=Source.GetRefBonePose()[J];
            Clip->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(J),Context,false);
            FTransform Retargeted=Rest;
            Retargeted.SetRotation((Sample.GetRotation()*Source.GetRefBonePose()[J].GetRotation().Inverse()*Rest.GetRotation()).GetNormalized());
            if(I==Bone(TEXT("Hips")))
            {
                // Only the match simulation translates the athlete across the field.
                const float Scale=Reference[I].GetScale3D().Z;
                Retargeted.AddToTranslation(FVector(0,0,(Sample.GetLocation().Z-Source.GetRefBonePose()[J].GetLocation().Z)*Scale));
            }
            // Batters and bowlers retain their dedicated athletic upper body posture above the pelvis.
            const FString Name=Target.GetBoneName(I).ToString();
            const bool Lower=Name.Contains(TEXT("Leg"))||Name.Contains(TEXT("Foot"))||Name.Contains(TEXT("Toe"))||I==Bone(TEXT("Hips"));
            const bool RetainUpper=Batting||(Role==EC26Role::Bowler);
            Local.Blend(Local,Retargeted,RetainUpper&&!Lower?0.f:Weight);
        }
        Animated[I]=P<0?Local:Local*Animated[P];
    }
    Pose=MoveTemp(Animated);
    const int L=Bone(TEXT("LeftFoot")),R=Bone(TEXT("RightFoot"));
    if(L>=0&&R>=0)
    {
        const float Lift=FMath::Max(0.f,AnkleZ-FMath::Min(Pose[L].GetLocation().Z,Pose[R].GetLocation().Z));
        for(auto& Transform:Pose)Transform.AddToTranslation(FVector(0,0,Lift));
    }
}
namespace
{
    // The authored actions are keyed at 24 fps. These are the frames that must land on the match's
    // own timing authority, because every visual and every result in this game is timed from those
    // two instants: the bat meeting the ball (C26Field::BatContactPoseTime) and the ball leaving
    // the hand (C26Field::ReleasePoseTime). Everything else about the clip is free.
    constexpr float C26AuthoredFps = 24.f;
    constexpr int32 C26BattingContactFrame = 23;   // A_C26_BattingDrive
    constexpr int32 C26BowlingReleaseFrame = 31;   // A_C26_BowlingPace
}
void AC26Athlete::GatherDrivenBones(UAnimSequence* Clip,TSet<int32>& Out)
{
    Out.Reset();
    if(!Clip||!Clip->GetSkeleton()||!Mesh||!Mesh->GetSkinnedAsset())return;
    const auto& Source=Clip->GetSkeleton()->GetReferenceSkeleton();
    const auto& Target=Mesh->GetSkinnedAsset()->GetRefSkeleton();
    const float Length=Clip->GetPlayLength();
    if(Length<=0.f)return;
    // Sample right across the clip. Seventeen passes over 67 bones is nothing, and it is measured
    // once per athlete rather than per frame.
    const int32 Steps=16;
    for(int32 S=0;S<=Steps;++S)
    {
        const FAnimExtractContext Context(Length*float(S)/float(Steps),false);
        for(int32 I=0;I<Pose.Num();++I)
        {
            if(Out.Contains(I))continue;
            const int32 J=Source.FindBoneIndex(Target.GetBoneName(I));
            if(J<0)continue;
            FTransform Sample=Source.GetRefBonePose()[J];
            Clip->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(J),Context,false);
            // 1e-3 is deliberately loose. The question is "does this clip drive this bone at all",
            // not "is this bone off its rest pose on this particular frame".
            if(!Sample.Equals(Source.GetRefBonePose()[J],1e-3f))Out.Add(I);
        }
    }
    // A clip that could not be interrogated must still animate rather than silently do nothing.
    if(Out.IsEmpty())
        for(int32 I=0;I<Pose.Num();++I)
            if(Source.FindBoneIndex(Target.GetBoneName(I))>=0)Out.Add(I);
}
void AC26Athlete::LoadShotLibrary()
{
    if(ShotLibraryLoaded)return;
    ShotLibraryLoaded=true;
    // Lazy, not ConstructorHelpers: a missing asset must degrade to the base clip
    // (and then to the procedural action), not stop the CDO from loading.
    auto Load=[this](const TCHAR* Path,TObjectPtr<UAnimSequence>& Out)
    {
        if(!Out)Out=LoadObject<UAnimSequence>(nullptr,Path);
        if(!Out)UE_LOG(LogTemp,Warning,TEXT("C26_SHOTLIB missing %s (run Tools/ImportAnimations.py)"),Path);
    };
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingDrive.A_C26_BattingDrive"),BattingClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BowlingPace.A_C26_BowlingPace"),BowlingClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingPull.A_C26_BattingPull"),BattingPullClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingCut.A_C26_BattingCut"),BattingCutClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingSweep.A_C26_BattingSweep"),BattingSweepClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingDefence.A_C26_BattingDefence"),BattingDefenceClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingBackFootDefence.A_C26_BattingBackFootDefence"),BattingBackFootDefenceClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingUpperCut.A_C26_BattingUpperCut"),BattingUpperCutClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingLateCut.A_C26_BattingLateCut"),BattingLateCutClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingHook.A_C26_BattingHook"),BattingHookClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingLoftedDrive.A_C26_BattingLoftedDrive"),BattingLoftedDriveClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BattingGlance.A_C26_BattingGlance"),BattingGlanceClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BowlingOffSpin.A_C26_BowlingOffSpin"),BowlingOffSpinClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_BowlingLegSpin.A_C26_BowlingLegSpin"),BowlingLegSpinClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_UmpireSignalWide.A_C26_UmpireSignalWide"),UmpireWideClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_UmpireSignalSix.A_C26_UmpireSignalSix"),UmpireSixClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_UmpireSignalOut.A_C26_UmpireSignalOut"),UmpireOutClip);
    Load(TEXT("/Game/Cricket26/Animations/A_C26_UmpireSignalFour.A_C26_UmpireSignalFour"),UmpireFourClip);
}

UAnimSequence* AC26Athlete::SelectBattingClip()
{
    LoadShotLibrary();
    // The simulation's own shot intent, not a visual guess. ShotAngle is the shot's
    // horizontal direction (0 = straight, negative = leg side, positive = off side),
    // StrideIntent says whether the weight went back or forward, Loft is the
    // player's own loft toggle, and the ball's height at the contact point is
    // measured off ContactTarget -- the same geometry C26Controls::ShotFamily()
    // names the stroke from, so the clip shows the shot the scorecard printed.
    // Visual only: no branch below can change what the simulation scored.
    const float BallHeight=ContactTarget.IsZero()?60.f:ContactTarget.Z-GetActorLocation().Z;
    if(Defending&&BattingDefenceClip)
        // The defensive answer to a short ball stays on the back foot; only a
        // full ball is pressed forward under the eyes.
        return (BallHeight>108.f&&BattingBackFootDefenceClip)?BattingBackFootDefenceClip:BattingDefenceClip;
    if(BallHeight>108.f)  // a short ball: the swing goes horizontal, not through
    {
        if(ShotAngle<=0.f)  // leg side: hook if it is up at the head, pull at the chest
            return BallHeight>148.f?(BattingHookClip?BattingHookClip:BattingPullClip)
                                   :(BattingPullClip?BattingPullClip:BattingClip);
        // off side: upper cut if it is climbing past the shoulder, square cut at
        // the chest.
        return BallHeight>148.f&&BattingUpperCutClip?BattingUpperCutClip
            :(BattingCutClip?BattingCutClip:BattingClip);
    }
    if(Loft&&FMath::Abs(ShotAngle)<40.f&&BattingLoftedDriveClip)return BattingLoftedDriveClip;
    if(ShotAngle<=-75.f)return BattingGlanceClip?BattingGlanceClip:BattingClip;  // leg glance
    if(ShotAngle<=-40.f)  // leg side, full: a low full ball on the toes is swept,
        return BallHeight<45.f&&StrideIntent>=0.f  // anything higher is flicked
            ?(BattingSweepClip?BattingSweepClip:BattingClip)
            :(BattingPullClip?BattingPullClip:BattingClip);
    if(ShotAngle>78.f)  // taken behind the hip line, steered fine
        return BattingLateCutClip?BattingLateCutClip:(BattingCutClip?BattingCutClip:BattingClip);
    if(ShotAngle>=40.f&&StrideIntent<0.f)
        return BattingCutClip?BattingCutClip:BattingClip;
    return BattingClip;
}

UAnimSequence* AC26Athlete::SelectBowlingClip()
{
    LoadShotLibrary();
    switch(DeliveryStyle)
    {
        case EC26Delivery::OffBreak:case EC26Delivery::ArmBall:
        case EC26Delivery::TopSpinner:case EC26Delivery::Doosra:
            return BowlingOffSpinClip?BowlingOffSpinClip:BowlingClip;
        case EC26Delivery::LegBreak:case EC26Delivery::Googly:
        case EC26Delivery::Flipper:
            return BowlingLegSpinClip?BowlingLegSpinClip:BowlingClip;
        default:return BowlingClip;
    }
}

UAnimSequence* AC26Athlete::SelectSignalClip()
{
    LoadShotLibrary();
    switch(Action)
    {
        case EC26Action::SignalWide:return UmpireWideClip;
        case EC26Action::SignalSix:return UmpireSixClip;
        case EC26Action::SignalOut:return UmpireOutClip;
        case EC26Action::SignalFour:return UmpireFourClip;
        default:return nullptr;
    }
}

TSet<int32>& AC26Athlete::DrivenFor(UAnimSequence* Clip)
{
    static TSet<int32> Empty;
    if(!Clip)return Empty;
    const FName Key=Clip->GetFName();
    if(TSet<int32>* Found=ClipDriven.Find(Key))return *Found;
    TSet<int32>& Out=ClipDriven.Add(Key);
    GatherDrivenBones(Clip,Out);
    return Out;
}

void AC26Athlete::ApplyAuthoredClip(UAnimSequence* Clip,const TSet<int32>& Driven,float Time,float Weight)
{
    SCOPE_CYCLE_COUNTER(STAT_C26_AuthoredClip);
    if(!Clip||Weight<=0.f||Driven.IsEmpty()||!Clip->GetSkeleton()||!Mesh||!Mesh->GetSkinnedAsset())return;
    const auto& Source=Clip->GetSkeleton()->GetReferenceSkeleton();
    const auto& Target=Mesh->GetSkinnedAsset()->GetRefSkeleton();
    const FAnimExtractContext Context(FMath::Clamp(Time,0.f,Clip->GetPlayLength()),false);
    TArray<FTransform> Animated;Animated.SetNum(Pose.Num());
    for(int32 I=0;I<Pose.Num();++I)
    {
        const int32 P=Parents[I];
        FTransform Local=P<0?Pose[I]:Pose[I].GetRelativeTransform(Pose[P]);
        if(Driven.Contains(I))
        {
            const int32 J=Source.FindBoneIndex(Target.GetBoneName(I));
            if(J>=0)
            {
                const FTransform Rest=P<0?Reference[I]:Reference[I].GetRelativeTransform(Reference[P]);
                FTransform Sample=Source.GetRefBonePose()[J];
                Clip->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(J),Context,false);
                // Only the delta from the clip's own reference pose is applied, so the clip and the
                // mesh can disagree about rest and the action still lands in the same place.
                FTransform Retargeted=Rest;
                Retargeted.SetRotation((Sample.GetRotation()*Source.GetRefBonePose()[J].GetRotation().Inverse()*Rest.GetRotation()).GetNormalized());
                if(I==Bone(TEXT("Hips")))
                {
                    // Unlike the looping run, an action clip IS allowed to carry the body fore and
                    // aft: the batter's weight going forward onto the front foot and the bowler
                    // bounding over the braced leg are the action, not decoration. The magnitudes
                    // are centimetres (the authored 0.18 m of hip travel scales to about 9 cm), so
                    // this never fights the match for ownership of the athlete's position.
                    const float Scale=Reference[I].GetScale3D().Z;
                    Retargeted.AddToTranslation((Sample.GetLocation()-Source.GetRefBonePose()[J].GetLocation())*Scale);
                }
                Local.Blend(Local,Retargeted,Weight);
            }
        }
        Animated[I]=P<0?Local:Local*Animated[P];
    }
    Pose=MoveTemp(Animated);
    // The authored foot targets sit on the turf in the clip's own units. Correcting against the
    // posed feet, rather than trusting the conversion, is what keeps a bat swing from sinking a
    // boot through the pitch surface.
    const int32 L=Bone(TEXT("LeftFoot")),R=Bone(TEXT("RightFoot"));
    if(L>=0&&R>=0)
    {
        const float Lift=FMath::Max(0.f,AnkleZ-FMath::Min(Pose[L].GetLocation().Z,Pose[R].GetLocation().Z));
        for(auto& Transform:Pose)Transform.AddToTranslation(FVector(0,0,Lift));
    }
}
void AC26Athlete::Animate(float Dt)
{
    SCOPE_CYCLE_COUNTER(STAT_C26_Animate);

    if (Presentation && Presentation->IsActive())
    {
        ActionTime += FMath::Max(0.f, Dt);
        MotionTime += FMath::Max(0.f, Dt);
        Presentation->UpdateFromMatch(this, Dt);
        return;
    }
    if(bHeroVisual && HeroMesh && HeroMesh->GetStaticMesh())
    {
        HeroMesh->SetVisibility(true);
        Mesh->SetVisibility(false);
        Uniform->SetVisibility(false);
        Shade->SetVisibility(false);
        Bat->SetVisibility(false);
        Headwear->SetVisibility(false);
        Grill->SetVisibility(false);
        PadL->SetVisibility(false);
        PadR->SetVisibility(false);
        GloveL->SetVisibility(false);
        GloveR->SetVisibility(false);
        ShoeL->SetVisibility(false);
        ShoeR->SetVisibility(false);
        ShirtNumber->SetVisibility(false);
        return;
    }
    if(Reference.IsEmpty())return;
    // The match code can change MoveSpeed in one frame -- a fielder released at 1050 cm/s, a
    // runner turning for the second. Stride length is derived from speed, so an instantaneous
    // speed change is an instantaneous change of stride length, which is a skate. The legs see a
    // speed that accelerates.
    ShownSpeed=Dt>0.f?FMath::FInterpTo(ShownSpeed,FMath::Max(0.f,MoveSpeed),Dt,9.f):FMath::Max(0.f,MoveSpeed);
    const float Cadence=RunClip&&RunClip->GetPlayLength()>0.f
        ?2.f*PI*ShownSpeed/(480.f*RunClip->GetPlayLength())
        :FMath::Clamp(ShownSpeed/60.f,0.f,15.f);
    // Only the genuinely distant tier may skip a solve. Mid-tier athletes sit between 26 m and
    // 60 m -- close enough that a fielder on the ring is still a legible silhouette, and a pose
    // re-solved every second frame on a body that size is exactly what reads as stutter. The
    // filter below still smooths every frame; this only stops the target itself from stepping.
    // `!Shown.IsEmpty()` is load-bearing: an athlete who is distant on the very first frame he
    // ticks has never solved a pose, and skipping that frame leaves him standing in the rig's
    // bind pose until he happens to land on an even frame.
    if(Dt>0.f&&Detail==EDetail::Distant&&!Shown.IsEmpty()&&++SkipPhase%2!=0)
    {
        // Every clock still advances on a skipped frame; only the rate at which the pose is
        // re-solved falls, so nothing drifts and nothing snaps when the athlete is promoted back
        // to hero. Callers asking for an exact instant pass Dt of zero and never land here.
        ActionTime+=Dt;MotionTime+=Dt;
        if(Action==EC26Action::Running)GaitPhase+=Dt*Cadence;
        // The pose is not re-solved, but the displayed pose still travels toward the one that
        // was: half the point of the filter is that a 30 Hz solve need not look like 30 Hz. Only
        // the skin is refreshed -- kit placement lags by the residual of one filter step, which is
        // sub-millimetre at the range where an athlete is allowed to drop out of hero detail.
        // This used to test `Detail==EDetail::Mid` inside a branch that only runs when Detail is
        // Distant, so it could never fire: a distant athlete's skin was never refreshed on a
        // skipped frame, and an athlete who was distant on the frame he first ticked kept his bind
        // pose -- which is why fielders on the boundary stood in the rig's T-pose all match.
        if(!Shown.IsEmpty()){SmoothPose(Dt,false);Mesh->ApplyLocalPose(Shown);}
        return;
    }
    ActionTime+=Dt;MotionTime+=Dt;Pose=Reference;
    const bool Running=Action==EC26Action::Running;
    const bool Batting=Role==EC26Role::Batter;
    const bool Keeping=Role==EC26Role::Keeper;
    // Stride frequency follows the distance actually being covered, so feet stop skating.
    if(Running)GaitPhase+=Dt*Cadence;
    const float Gait=FMath::Sin(GaitPhase);
    // Breath, slow weight transfer and sway, all keyed off this player's own number. Eleven
    // fielders sharing one stance used to share one clock as well, so the whole side rose and fell
    // together -- which reads as clones far more strongly than shared geometry does.
    const C26Motion::FRest Easy=C26Motion::Rest(MotionTime,SquadNumber*13+int32(Role));
    const float Sway=Easy.Sway;
    float ActiveFingerCurl=0.38f;

    // Crouch is a hip drop; Shift moves the pelvis horizontally. A batter flexes his knees, he
    // does not sit down: the old fixed 23 cm drop held him in a squat through an entire stroke.
    float Crouch=Keeping?-42.f:Batting?-13.f:-7.f;
    FVector Shift=FVector::ZeroVector;
    float TurnRight=0,LeanForward=Keeping?24.f:Batting?9.f:7.f,LeanRight=0;
    // How far the chest is allowed to disagree with the pelvis. A run and a bowling action both
    // live on that disagreement: the shoulders counter-rotate against the hips, and the stretch
    // between them is what makes the torso look driven rather than carried.
    float ChestCounter=0.f;
    // Ankle pitch per foot, in degrees, positive toe-down. Zero means "derive it from how far the
    // foot is off the ground", which is all a standing pose needs.
    float PitchL=0.f,PitchR=0.f;
    if(Running){Crouch=-5.f+3.f*FMath::Abs(Gait);LeanForward=13.f;}
    if(Role==EC26Role::Umpire){Crouch=-2.f;LeanForward=2.f;}

    FVector FL=Reference[FMath::Max(0,Bone(TEXT("LeftFoot")))].GetLocation();
    FVector FR=Reference[FMath::Max(0,Bone(TEXT("RightFoot")))].GetLocation();
    FVector LH=Rig(20,-22,96+Crouch),RH=Rig(20,22,96+Crouch);
    // Elbow pole per arm. A pole pointing down and behind is correct for a hand at hip height and
    // badly wrong for one above the head: it solves the elbow underneath the shoulder, which is
    // the other half of why an overarm delivery read as a sling from below. Actions that take the
    // hands out of their default range say where the elbow should go.
    FVector PoleL=Rig(-.7f,-.5f,-.5f),PoleR=Rig(-.7f,.5f,-.5f);
    FVector Grip=Rig(10,14,85),Dir=Rig(.10f,.05f,.993f).GetSafeNormal();

    if(Running)
    {
        const bool Quick=Role==EC26Role::Bowler;
        const C26Motion::FStride L=C26Motion::Stride(GaitPhase,ShownSpeed,-9,AnkleZ,Quick);
        const C26Motion::FStride R=C26Motion::Stride(GaitPhase,ShownSpeed,9,AnkleZ,Quick);
        FL=Rig(L.Foot.X,L.Foot.Y,L.Foot.Z);FR=Rig(R.Foot.X,R.Foot.Y,R.Foot.Z);
        PitchL=L.Pitch;PitchR=R.Pitch;
        // The pelvis falls and is caught twice a stride, drops on the unsupported side and rotates
        // with the driving leg, and the shoulders turn against it. Without those three the legs
        // cycle underneath a body that is being carried along on rails.
        const C26Motion::FCarry Ride=C26Motion::Carry(GaitPhase,ShownSpeed);
        Crouch=-5.f+Ride.Bob;LeanRight=Ride.Roll;TurnRight=Ride.Yaw;ChestCounter=-Ride.Yaw*1.35f;
        // Arms drive from the shoulder, closing toward the midline and rising as they come
        // through rather than sweeping back and forth in one flat plane at one fixed height.
        const float Drive=FMath::Clamp(ShownSpeed/560.f,.30f,1.f);
        const float Fwd=FMath::Max(0.f,-Gait),Back=FMath::Max(0.f,Gait);
        LH=Rig(-Gait*38.f*Drive,-21.f+Fwd*7.f,110.f+Fwd*15.f*Drive);
        RH=Rig(Gait*38.f*Drive,21.f-Back*7.f,110.f+Back*15.f*Drive);
        LeanForward=10.f+Drive*5.f;
        if(Quick)
        {
            // A fast bowler's approach, not a jog. Authentic run-up form: both hands cradle
            // and protect the cricket ball in front of the chest, elbows tucked, pumping
            // rhythmically with stride turnover, and aggressive forward torso lean.
            RH=Rig(20.f+Gait*6.f,7.f,126.f+Fwd*5.f);
            LH=Rig(18.f-Gait*5.f,-7.f,124.f+Back*5.f);
            LeanForward=18.f+Fwd*3.f;
        }
    }
    else if(Batting)
    {
        // A batting stance, not a standing pose. Side-on with the chest opened toward the off
        // side, weight forward over the balls of the feet, and -- the part that actually reads at
        // gameplay distance -- knees genuinely loaded and a base wide enough to see. The old
        // stance stood nearly upright with the feet almost together, which is why the striker
        // looked like a mannequin holding a bat rather than a batter waiting for one.
        TurnRight=48.f;LeanForward=25.f;LeanRight=5.f;
        Crouch=-19.f;
        FL=Rig(17.f+FootworkIntent*4.f,-4.f+StrideIntent*3.f,AnkleZ);
        FR=Rig(-16.f,13.f,AnkleZ);
    }
    else if(Role==EC26Role::Umpire){FL=Rig(0,-13,AnkleZ);FR=Rig(0,13,AnkleZ);LH=Rig(1,-23,95);RH=Rig(1,23,95);ActiveFingerCurl=0.20f;}
    else if(Role==EC26Role::Bowler)
    {
        // At the top of his mark. A bowler waiting to run in stands tall and square with the ball
        // held in both hands at chest height and his weight rocking onto the front foot.
        Crouch=-4.f;LeanForward=9.f;TurnRight=-14.f;
        FL=Rig(9,-11,AnkleZ);FR=Rig(-9,13,AnkleZ);
        const float Rock=FMath::Sin(MotionTime*1.35f);
        LH=Rig(26.f+Rock*1.6f,-6.f,124.f);RH=Rig(26.f+Rock*1.6f,4.f,124.f);
        PoleL=Rig(-0.25f,-0.75f,-0.25f);PoleR=Rig(-0.25f,0.75f,-0.25f);
        ActiveFingerCurl=0.45f;
    }
    else
    {
        // Authentic, position-aware fielding ready stance (Cricket 24 broadcast inspired):
        // Wicketkeeper: low dynamic squat with gloves ready
        // Slips: deep crouch with hands cupped at knee height
        // Ring: athletic flexed stance with hands resting naturally at thighs/hips
        // Deep: relaxed upright poise
        const auto FP=C26Motion::SolveFielderReady(
            MotionTime,
            SquadNumber,
            Role,
            GetActorLocation(),
            AnkleZ);
        Crouch=FP.Crouch;
        LeanForward=FP.LeanForward;
        LeanRight=FP.LeanRight;
        TurnRight=FP.TurnRight;
        PitchL=FP.PitchL;
        PitchR=FP.PitchR;
        FL=FP.LeftFoot;
        FR=FP.RightFoot;
        LH=FP.LeftHand;
        RH=FP.RightHand;
        PoleL=FP.PoleL;
        PoleR=FP.PoleR;
        ActiveFingerCurl=FP.FingerCurl;
    }

    if(Batting&&!Running)
    {
        if(Action==EC26Action::Batting)
        {
            const FVector Contact=Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
            const auto Stroke=C26Motion::SolveBattingStroke(
                ActionTime,
                C26Field::BatContactPoseTime,
                Defending,
                ShotAngle,
                Loft,
                StrideIntent,
                FootworkIntent,
                Contact,
                AnkleZ,
                MiddleDrop);

            Grip=Stroke.Grip;
            Dir=Stroke.Dir;
            FL=Stroke.LeftFoot;
            FR=Stroke.RightFoot;
            PoleL=Stroke.PoleL;
            PoleR=Stroke.PoleR;
            Shift=Stroke.HipShift;
            Crouch=Stroke.Crouch;
            LeanForward=Stroke.LeanForward;
            LeanRight=Stroke.LeanRight;
            TurnRight=Stroke.TurnRight;
            PitchL=Stroke.PitchL;
            PitchR=Stroke.PitchR;
        }
        else if(Action==EC26Action::Ready)
        {
            // Rhythmic bat tap and a small weight shift; a still batter reads as a mannequin.
            const float Tap=FMath::Square(FMath::Max(0.f,FMath::Sin(MotionTime*2.4f)))*3.f;
            const FVector Toe=Rig(4,16,6.f+Tap);
            Dir=Rig(.10f,.05f,.993f).GetSafeNormal();
            Grip=Toe+Dir*BatLength;
            TurnRight=46.f+Sway*2.5f;LeanForward=22.f;
        }
        else if(Action==EC26Action::Celebrate){Grip=Rig(6,26,196);Dir=Rig(-.25f,.30f,.92f).GetSafeNormal();TurnRight=12.f;LeanForward=-6.f;}
        else if(Action==EC26Action::Disappointed){Grip=Rig(14,16,74);Dir=Rig(.55f,.10f,.83f).GetSafeNormal();TurnRight=22.f;LeanForward=24.f;}
        else if(Action==EC26Action::BatRaise){Grip=Rig(8,22,212);Dir=Rig(-.15f,.10f,.98f).GetSafeNormal();TurnRight=-12.f;LeanForward=-8.f;}
        else if(Action==EC26Action::GloveTap){Grip=Rig(28,14,125);Dir=Rig(.6f,.1f,.79f).GetSafeNormal();TurnRight=18.f;LeanForward=6.f;}
        else if(Action==EC26Action::Handshake){Grip=Rig(10,14,75);Dir=Rig(.10f,.05f,.993f).GetSafeNormal();TurnRight=6.f;LeanForward=4.f;}
        else if(Action==EC26Action::Discuss){Grip=Rig(18,12,105);Dir=Rig(.4f,.05f,.91f).GetSafeNormal();TurnRight=24.f;LeanForward=10.f;}
        else{Grip=Rig(10,14,85);Dir=Rig(.10f,.05f,.993f).GetSafeNormal();TurnRight=30.f;}
        if(NonStriker&&Action==EC26Action::Ready)
        {
            TurnRight=12.f;LeanForward=8.f;Crouch=-6.f;
            FL=Rig(8,-11,AnkleZ);FR=Rig(-8,11,AnkleZ);
            Grip=Rig(18,20,85);Dir=Rig(.15f,0,.99f).GetSafeNormal();
        }
        else if(Action==EC26Action::Ready)
        {
            // Trigger movement. A batter does not keep his bat on the ground while the bowler is
            // running in: he presses forward onto the front foot and lifts the bat up behind his
            // back shoulder into the backlift, so that the only thing left to do at release is
            // come down through the line of the ball. The old trigger raised the grip 9 cm, which
            // reads as a twitch rather than as a batsman loading.
            const float Press=FMath::Sin(Trigger*PI);
            Shift=Rig(-Press*4.f,Press*1.5f,0);
            const FVector Loaded=Rig(-15.f,19.f,114.f);
            Grip=FMath::Lerp(Grip,Loaded,Press*.92f);
            Dir=FMath::Lerp(Dir,Rig(-.42f,.26f,.87f).GetSafeNormal(),Press*.92f).GetSafeNormal();
            Crouch-=Press*5.f;LeanForward+=Press*4.f;
        }
        // Keep the handle inside the arms' reach before deriving the hands from it. The IK clamps
        // silently at full extension, so a follow-through that asked for more arm than the athlete
        // owns used to strand both hands short while the blade carried on to the target.
        const FVector Anchor=Rig(0,0,ShoulderZ+Crouch);
        const FVector Out=Grip-Anchor;
        if(const float Span=Out.Size();Span>ArmSpan*.95f)Grip=Anchor+Out/Span*(ArmSpan*.95f);
        // Both hands live on the handle: top hand high, bottom hand a fist below it.
        LH=Grip-Dir*4.f;RH=Grip-Dir*14.f;
    }
    if(Batting&&Running){Grip=Rig(24,20,104);Dir=Rig(-.55f,.10f,.83f).GetSafeNormal();RH=Grip-Dir*14.f;}

    // Resting motion for anyone the match is not currently driving: breath through the chest, and
    // weight drifting slowly from one foot to the other. It is small on purpose -- a player who
    // sways visibly is not standing still, he is unbalanced -- but a player with none of it at all
    // is a statue, and a field of statues is the first thing a viewer notices.
    const bool Busy=Action==EC26Action::Batting||Action==EC26Action::Bowling||Action==EC26Action::Throw
        ||Action==EC26Action::Catch||Action==EC26Action::Pickup||Action==EC26Action::Dive;
    if(!Running&&!Busy)
    {
        LeanForward+=Easy.Breath*1.2f;
        Shift+=Rig(Easy.Weight*.8f,Easy.Weight*1.4f,0);
        LeanRight+=Easy.Weight*1.5f;
    }

    if(Action==EC26Action::Bowling)
    {
        const auto P=C26Motion::Pace(ActionTime,AnkleZ);
        const float Blend=FMath::SmoothStep(0.f,.13f,ActionTime);
        const C26Motion::FStride RunL=C26Motion::Stride(GaitPhase,ShownSpeed,-9,AnkleZ,true);
        const C26Motion::FStride RunR=C26Motion::Stride(GaitPhase,ShownSpeed,9,AnkleZ,true);
        FL=FMath::Lerp(Rig(RunL.Foot.X,RunL.Foot.Y,RunL.Foot.Z),Rig(P.LeftFoot.X,P.LeftFoot.Y,P.LeftFoot.Z),Blend);
        FR=FMath::Lerp(Rig(RunR.Foot.X,RunR.Foot.Y,RunR.Foot.Z),Rig(P.RightFoot.X,P.RightFoot.Y,P.RightFoot.Z),Blend);
        // The approach's ankle roll fades out as the delivery action takes over.
        PitchL=RunL.Pitch*(1.f-Blend);PitchR=RunR.Pitch*(1.f-Blend);

        // Pelvic translation: the hips drive forward over the planted front foot and gently
        // steer away from the pitch danger area during deceleration.
        Shift=Rig(P.HipShift.X,P.HipShift.Y,0.f);

        // Bowling arm arc: smooth overhead circular rotation during backswing and delivery,
        // finishing with an authentic cross-body follow-through sweep past the left hip.
        const float Circle=C26Motion::BowlArm(ActionTime);
        const FVector Hub=Rig(P.HipShift.X,18.f+P.HipShift.Y,ShoulderZ+P.HipDrop);
        const float Radius=ArmSpan*.97f;
        const float FollowT=FMath::Clamp((ActionTime-.62f)/.46f,0.f,1.f);
        const float Lateral=FMath::Lerp(10.f*FMath::Cos(Circle*.5f),-26.f,FollowT*FollowT*(3.f-2.f*FollowT));
        const FVector Swing=Hub+Rig(FMath::Sin(Circle)*Radius,Lateral,FMath::Cos(Circle)*Radius);
        const float Fwd=FMath::Max(0.f,-Gait),Back=FMath::Max(0.f,Gait);
        const FVector RunRH=Rig(20.f+Gait*6.f,7.f,126.f+Fwd*5.f);
        RH=FMath::Lerp(RunRH,Swing,Blend);

        // Non-bowling arm: authentic biomechanical motion - rises with gather,
        // sights high toward the batsman at back-foot contact, pulls hard down into the ribs
        // at front-foot plant to generate explosive rotational torque, stays tucked tight
        // through release, and settles into fielding readiness.
        const FVector FrontTarget=Rig(P.LeftHand.X+P.HipShift.X,P.LeftHand.Y+P.HipShift.Y,P.LeftHand.Z);
        const FVector RunLH=Rig(18.f-Gait*5.f,-7.f,124.f+Back*5.f);
        LH=FMath::Lerp(RunLH,FrontTarget,Blend);

        // Elbow pole vectors: bowling elbow stays outward and up during release, then sweeps
        // diagonally forward-left; front elbow pulls back and tight into the ribs.
        const float High=FMath::Clamp(FMath::Cos(Circle),0.f,1.f)*Blend;
        const float FollowR=FMath::Clamp((ActionTime-.62f)/.5f,0.f,1.f);
        PoleR=FMath::Lerp(PoleR,FMath::Lerp(Rig(-.25f,.95f,.35f),Rig(.45f,.65f,-.40f),FollowR),Blend).GetSafeNormal();
        const float PullT=FMath::Clamp((ActionTime-.22f)/.35f,0.f,1.f);
        PoleL=FMath::Lerp(PoleL,FMath::Lerp(Rig(-.20f,-.95f,.30f),Rig(-.85f,-.45f,-.10f),PullT),Blend).GetSafeNormal();

        TurnRight=P.Turn;LeanForward=P.Lean;LeanRight=P.Side;Crouch=P.HipDrop;

        // Hip-shoulder separation: chest holds back against pelvic unwinding before whipping through
        ChestCounter=-P.Turn*.32f*Blend;
    }

    if(Action==EC26Action::Pickup)
    {
        const FVector Take=ContactTarget.IsZero()?Rig(34,0,142):Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        const auto FP=C26Motion::SolveFielderPickup(ActionTime,Take,AnkleZ,ShoulderZ,PalmReach);
        Crouch=FP.Crouch;
        LeanForward=FP.LeanForward;
        LeanRight=FP.LeanRight;
        TurnRight=FP.TurnRight;
        ChestCounter=FP.ChestCounter;
        Shift=FP.HipShift;
        FL=FP.LeftFoot;FR=FP.RightFoot;
        PitchL=FP.PitchL;PitchR=FP.PitchR;
        LH=FP.LeftHand;RH=FP.RightHand;
        PoleL=FP.PoleL;PoleR=FP.PoleR;
        ActiveFingerCurl=FP.FingerCurl;

        // Deceleration. The authored gather is a standing solve: it assumes the athlete is already
        // over the ball. A fielder who arrives at 700 cm/s is not, and cutting straight to that
        // solve is what made a chase end in a single frame with both feet arriving from nowhere.
        // The speed carried into the action decays, the stride keeps turning over at the cadence
        // that decaying speed implies, and the braking step is cross-faded into the gather -- so
        // the legs finish one more real step while the body is already going down to the ball.
        const C26Motion::FApproachBrake Brake=C26Motion::SolveApproachBrake(
            ActionTime,ApproachSpeed,ApproachGait,AnkleZ,RunClip?RunClip->GetPlayLength():0.f);
        FL=FMath::Lerp(Brake.LeftFoot,FL,Brake.Plant);
        FR=FMath::Lerp(Brake.RightFoot,FR,Brake.Plant);
        PitchL=FMath::Lerp(Brake.PitchL,PitchL,Brake.Plant);
        PitchR=FMath::Lerp(Brake.PitchR,PitchR,Brake.Plant);
        LeanForward+=Brake.LeanForward;
        Crouch+=Brake.Crouch;
    }
    else if(Action==EC26Action::Catch)
    {
        const FVector Take=ContactTarget.IsZero()?Rig(34,0,142):Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        const auto FP=C26Motion::SolveFielderCatch(ActionTime,Take,AnkleZ,ShoulderZ,PalmReach);
        Crouch=FP.Crouch;
        LeanForward=FP.LeanForward;
        Shift=FP.HipShift;
        FL=FP.LeftFoot;FR=FP.RightFoot;
        PitchL=FP.PitchL;PitchR=FP.PitchR;
        LH=FP.LeftHand;RH=FP.RightHand;
        PoleL=FP.PoleL;PoleR=FP.PoleR;
        ActiveFingerCurl=FP.FingerCurl;
    }
    else if(Action==EC26Action::Dive)
    {
        const FVector Rel=ContactTarget.IsZero()?Rig(30,0,15):Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        const auto FP=C26Motion::SolveFielderDive(ActionTime,Rel,AnkleZ,ShoulderZ,PalmReach);
        Crouch=FP.Crouch;
        LeanForward=FP.LeanForward;
        LeanRight=FP.LeanRight;
        Shift=FP.HipShift;
        FL=FP.LeftFoot;FR=FP.RightFoot;
        PitchL=FP.PitchL;PitchR=FP.PitchR;
        LH=FP.LeftHand;RH=FP.RightHand;
        PoleL=FP.PoleL;PoleR=FP.PoleR;
        ActiveFingerCurl=FP.FingerCurl;
    }
    if(Action==EC26Action::Throw)
    {
        const auto FP=C26Motion::SolveFielderThrow(ActionTime,AnkleZ,ShoulderZ);
        Crouch=FP.Crouch;
        LeanForward=FP.LeanForward;
        LeanRight=FP.LeanRight;
        TurnRight=FP.TurnRight;
        ChestCounter=FP.ChestCounter;
        Shift=FP.HipShift;
        FL=FP.LeftFoot;FR=FP.RightFoot;
        PitchL=FP.PitchL;PitchR=FP.PitchR;
        LH=FP.LeftHand;RH=FP.RightHand;
        PoleL=FP.PoleL;PoleR=FP.PoleR;
        ActiveFingerCurl=FP.FingerCurl;
    }
    if(Action==EC26Action::Celebrate&&!Batting){LH=Rig(10,-16,196);RH=Rig(10,16,196);LeanForward=-6.f;}
    if(Action==EC26Action::Disappointed&&!Batting){LH=Rig(10,-20,120);RH=Rig(10,20,120);LeanForward=22.f;}
    if(Action==EC26Action::SignalSix){LH=Rig(-4,-22,205);RH=Rig(-4,22,205);LeanForward=-4.f;}
    if(Action==EC26Action::SignalOut){RH=Rig(2,20,206);LH=Rig(1,-23,95);}
    if(Action==EC26Action::SignalFour){LH=Rig(20,-72,128);RH=Rig(20,72,128);}
    if(Action==EC26Action::SignalWide){LH=Rig(2,-84,140);RH=Rig(2,84,140);}
    if(Action==EC26Action::BatRaise&&!Batting){LH=Rig(8,-22,190);RH=Rig(8,22,210);LeanForward=-8.f;}
    if(Action==EC26Action::FistPump&&!Batting){LH=Rig(4,-18,95);RH=Rig(18,12,145);LeanForward=-12.f;TurnRight=-16.f;}
    if(Action==EC26Action::GloveTap&&!Batting){LH=Rig(4,-18,95);RH=Rig(32,10,132);LeanForward=4.f;}
    if(Action==EC26Action::Handshake){LH=Rig(2,-20,85);RH=Rig(30,6,108);LeanForward=4.f;}
    if(Action==EC26Action::Discuss){LH=Rig(4,-20,95);RH=Rig(24,18,126);LeanForward=8.f;TurnRight=15.f;}
    if(Action==EC26Action::TossFlip){
        const float FlipT=FMath::Clamp(ActionTime/1.2f,0.f,1.f);
        const float HandZ=FlipT<0.35f?FMath::Lerp(90.f,140.f,FlipT/0.35f):FMath::Lerp(140.f,110.f,(FlipT-0.35f)/0.65f);
        LH=Rig(2,-18,85);RH=Rig(22,4,HandZ);LeanForward=-4.f;
    }

    MoveBone(TEXT("Hips"),Shift+FVector(0,0,Crouch));
    Twist(TEXT("Hips"),TurnRight*.42f,LeanForward*.30f,LeanRight*.5f);
    Twist(TEXT("Spine"),TurnRight*.26f+ChestCounter*.22f,LeanForward*.34f,LeanRight*.3f);
    Twist(TEXT("Spine1"),TurnRight*.20f+ChestCounter*.38f,LeanForward*.22f,LeanRight*.2f);
    Twist(TEXT("Spine2"),TurnRight*.12f+ChestCounter*.40f,LeanForward*.14f,0);
    ApplyRecordedMotion(Running,Batting);
    // Knees track over the toes. Bending every leg toward mesh-forward puts a side-on batter's
    // knees across his own shins and drives a runner's knees straight while the foot swings wide;
    // the pole target leans toward wherever that leg's foot actually is, which is what a knee does.
    auto Knee=[](const FVector& Foot,float Splay)
    {
        // Rig() maps (forward,right,up) into mesh space, so a foot's athlete-right component is
        // -Foot.X. Clamped: a knee follows the foot, it does not point at it.
        return Rig(1.f,FMath::Clamp(-Foot.X*.030f,-.45f,.45f)+Splay,0.f).GetSafeNormal();
    };
    // No pose may ask a leg for more than the leg has. Every foot target above is authored in
    // centimetres against a nominal body, and the ones that overreach -- the bowler's 86 cm front
    // stride, a full-stretch gather -- were absorbed by the two-bone solve hauling the pelvis down
    // and forward after the foot, which is what folded the athlete in half at the crease. Clamping
    // to the real chain leaves an extreme pose extreme instead of broken.
    auto Reachable=[&](const FVector& Foot,float Side)
    {
        const FVector Hip=Shift+Rig(0,Side,HipZ+Crouch);
        const float Limit=(HipZ-AnkleZ)*.99f;
        const FVector Delta=Foot-Hip;
        return Delta.Size()>Limit?Hip+Delta.GetSafeNormal()*Limit:Foot;
    };
    FL=Reachable(FL,-9.f);FR=Reachable(FR,9.f);
    Limb(TEXT("LeftUpLeg"),TEXT("LeftLeg"),TEXT("LeftFoot"),FL,Knee(FL,-.10f));
    Limb(TEXT("RightUpLeg"),TEXT("RightLeg"),TEXT("RightFoot"),FR,Knee(FR,.10f));
    // Foot orientation is independent of shin rotation. Restoring the planted shoe frame avoids
    // toes lifting off the surface as the knee bends; swing feet get a small toe-off rotation.
    for(const FString Side:{FString(TEXT("Left")),FString(TEXT("Right"))})
    {
        const int F=Bone(Side+TEXT("Foot"));
        if(F<0)continue;
        // A foot off the ground rolls onto its toe. Held flat it reads as a mannequin being
        // carried through the air, which is what every lifted foot in the game did until now --
        // the back heel of a drive, and every stride of the bowler's run-up.
        const float Lift=FMath::Clamp((Pose[F].GetLocation().Z-AnkleZ)/16.f,0.f,1.f);
        // A stride knows exactly what its ankle is doing -- heel strike, flat mid-stance, drive off
        // the toe, dorsiflexed for clearance -- so it says so. Everything else falls back to the
        // height rule, which is all a standing pose or a one-off reach needs.
        const float Stride=Side==TEXT("Left")?PitchL:PitchR;
        const float Roll=FMath::IsNearlyZero(Stride)?Lift*36.f:Stride;
        Pose[F].SetRotation((FQuat(Rig(0,1,0),FMath::DegreesToRadians(Roll))*Reference[F].GetRotation()).GetNormalized());
        RebuildChildren(F);
    }
    if(Batting&&!Running)
    {
        // Refine the existing motion at contact: keep both wrists on one handle within the
        // *posed* shoulders' reach, and pivot that handle through the incoming ball. Previously
        // a 10 cm lateral wrist offset tilted the blade ~45 degrees away from the ball.
        const FVector Contact=Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        const float ContactWeight=Action==EC26Action::Batting
            ?1.f-FMath::SmoothStep(0.f,.12f,FMath::Abs(ActionTime-C26Field::BatContactPoseTime)):0.f;
        const FVector AuthoredDir=Dir;
        for(int Iteration=0;Iteration<8;++Iteration)
        {
            Dir=FMath::Lerp(AuthoredDir,(Grip-Contact).GetSafeNormal(UE_SMALL_NUMBER,AuthoredDir),ContactWeight).GetSafeNormal();
            // Meet the ball on the meat of the blade. The bat origin sits 4.5 cm up the shaft from
            // the top palm and the middle of the willow is 62 cm below that, so unless the grip is
            // held about that far from the ball a technically correct contact still lands on the
            // last few centimetres of the toe -- measured at 75.7 cm down an 83 cm blade.
            Grip=FMath::Lerp(Grip,Contact+Dir*61.5f,ContactWeight*.5f);
            for(int Side=0;Side<2;++Side)
            {
                const int Shoulder=Bone(Side==0?TEXT("LeftArm"):TEXT("RightArm"));
                if(Shoulder<0)continue;
                const FVector Reach=Grip-Dir*(Side==0?4.f:14.f)-Pose[Shoulder].GetLocation();
                const float Limit=ArmSpan*.98f;
                if(Reach.Size()>Limit)Grip-=Reach.GetSafeNormal()*(Reach.Size()-Limit);
            }
        }
        Dir=FMath::Lerp(AuthoredDir,(Grip-Contact).GetSafeNormal(UE_SMALL_NUMBER,AuthoredDir),ContactWeight).GetSafeNormal();
        LH=Grip-Dir*4.f;RH=Grip-Dir*14.f;
    }
    // The collarbone goes with the arm. Two-bone IK from a fixed socket has exactly the reach the
    // bind pose gave it, so an overhead catch, a celebration and the top of a bowling action all
    // ran out of arm and clamped -- and a shoulder that never moves while the arm swings through
    // 200 degrees is the tell that separates a posed doll from a body.
    auto ReachableArm=[&](const FVector& Hand,bool Right)
    {
        const int Shoulder=Bone(Right?TEXT("RightArm"):TEXT("LeftArm"));
        if(Shoulder<0||!Pose.IsValidIndex(Shoulder))return Hand;
        const FVector Origin=Pose[Shoulder].GetLocation();
        // ArmSpan is the shoulder-to-wrist chain -- measured 51.8 cm on the shipped rig -- and that
        // is how every other reach limit in this file reads it: the batting handle clamp and the
        // bowling circle are both ArmSpan*.95 and up. This one treated it as a full
        // fingertip-to-fingertip span and halved it, so the hand target was capped at half an arm
        // from the socket. A fielder could not get his hands down to a ball on the turf: the palms
        // stayed ~65 cm above it, level and square to the ball but out of reach, and the match then
        // snapped the ball up into his gloves to cover the gap -- the teleport this project refuses
        // to ship. Clamping at the real chain length leaves the guard doing its actual job, which is
        // stopping a hand from being asked for more arm than the bind pose has.
        const float MaxArm=(ArmSpan>0.f?ArmSpan:73.f)*.98f;
        const FVector Delta=Hand-Origin;
        return Delta.Size()>MaxArm?Origin+Delta.GetSafeNormal()*MaxArm:Hand;
    };
    FVector LHRaw=LH,RHRaw=RH;
    if(!Batting&&Action!=EC26Action::Bowling)
    {
        LH=ReachableArm(LH,false);
        RH=ReachableArm(RH,true);
    }
    ShoulderReach(TEXT("Left"),LH,Batting?.22f:.46f);
    ShoulderReach(TEXT("Right"),RH,Batting?.22f:.46f);
    if(!Batting&&Action!=EC26Action::Bowling)
    {
        // The clavicle has just carried the socket toward the target, so the arm is no longer asked
        // for as much as it was a moment ago: the clamp above was measured against the shoulder it
        // used to have. Re-projecting the RAW target from the new socket is what lets the arm extend
        // into the centimetres the shoulder just bought. Re-clamping the already-clamped point would
        // do nothing at all -- a clamp only ever pulls a target inward, and that point is now inside
        // the arm's reach -- which is exactly the trap this used to fall into: the elbow stayed bent
        // and the hand stopped ~8 cm short of a ball it could have touched.
        LH=ReachableArm(LHRaw,false);
        RH=ReachableArm(RHRaw,true);
    }
    Limb(TEXT("LeftArm"),TEXT("LeftForeArm"),TEXT("LeftHand"),LH,PoleL);
    Limb(TEXT("RightArm"),TEXT("RightForeArm"),TEXT("RightHand"),RH,PoleR);
    if(Batting)
    {
        // Rotate palms toward the handle, then curl finger chains inside the padded glove.
        for(const FString Side:{FString(TEXT("Left")),FString(TEXT("Right"))})
        {
            Aim(Side+TEXT("Hand"),Side+TEXT("HandMiddle1"),Grip-Dir*(Side==TEXT("Left")?8.f:18.f));
            for(const TCHAR* Finger:{TEXT("Thumb"),TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Pinky")})
            {
                const int F=Bone(Side+TEXT("Hand")+Finger+TEXT("1"));
                if(F>=0){Pose[F].SetScale3D(Pose[F].GetScale3D()*.24f);RebuildChildren(F);}
            }
        }
    }
    else
    {
        // Align wrists naturally along forearm vector with slight inward tilt
        // This eliminates the rigid bind-pose T-pose wrist rotation!
        for(const FString Side:{FString(TEXT("Left")),FString(TEXT("Right"))})
        {
            const bool IsRight=Side==TEXT("Right");
            const int W=Bone(Side+TEXT("Hand")),K=Bone(Side+TEXT("HandMiddle1")),FA=Bone(Side+TEXT("ForeArm"));
            if(W>=0&&K>=0&&FA>=0&&Pose.IsValidIndex(W)&&Pose.IsValidIndex(FA))
            {
                const FVector ForeArmDir=(Pose[W].GetLocation()-Pose[FA].GetLocation()).GetSafeNormal();
                const FVector Inward=Rig(0.f,IsRight?-1.f:1.f,0.f);
                const FVector TargetAim=Pose[W].GetLocation()+ForeArmDir*16.f+Inward*4.f;
                Aim(Side+TEXT("Hand"),Side+TEXT("HandMiddle1"),TargetAim);
            }
        }
        // Authentic, relaxed human hands for all fielders across all ranges
        CurlFingers(TEXT("Left"),ActiveFingerCurl);
        CurlFingers(TEXT("Right"),ActiveFingerCurl);
    }
    // The authored one-shot actions. Both clips were keyed so that their defining frame -- bat on
    // ball, ball out of the hand -- can be pinned to the instant the match already times everything
    // from, and the clock is warped either side of that frame rather than scaled uniformly: a
    // uniform scale would move the defining frame, which is the one thing authoring the clip was
    // for. Phase one runs from the start of the action to that frame, phase two from it to the end,
    // so the clip covers the whole action AND lands its contact exactly where the match expects it.
    if(Batting&&!Running&&Action==EC26Action::Batting)
    {
        // The clip for the shot the simulation actually played (defence, pull, cut,
        // sweep, drive); null only when even the drive is missing, in which case the
        // procedural stroke keeps ownership below.
        UAnimSequence* Shot=SelectBattingClip();
        if(!Shot)Shot=BattingClip;
        if(Shot)
        {
        const float ClipLength=Shot->GetPlayLength();
        const float ClipContact=C26BattingContactFrame/C26AuthoredFps;
        const float Contact=C26Field::BatContactPoseTime;
        // Same span the procedural pass finishes the stroke over, so the hand-back at the end has
        // nothing to hide.
        const float Length=Loft?.72f:.62f;
        const float ClipTime=ActionTime<=Contact
            ?(Contact>UE_KINDA_SMALL_NUMBER?(ActionTime/Contact)*ClipContact:ClipContact)
            :ClipContact+(ClipLength-ClipContact)*FMath::Clamp((ActionTime-Contact)/FMath::Max(UE_KINDA_SMALL_NUMBER,Length-Contact),0.f,1.f);
        // Full authority through the stroke, with a short ramp at each end so the entry out of the
        // stance and the exit into the follow-through are travelled rather than cut.
        const float Weight=FMath::Min(FMath::SmoothStep(0.f,.05f,ActionTime),FMath::SmoothStep(0.f,.09f,Length-ActionTime));
        // Re-enabled 2026-09-13. The original disable was correct: the first export of this clip
        // was keyed under a wrong rig-facing assumption (the authoring script claimed +Y forward;
        // the rig faces -Y in armature space), so its swing arc ran through the batter's back and
        // replacing the procedural stroke with it produced the contorted stance seen on capture.
        // The clip has since been re-solved with the facing repair (c26_anim_author.py::
        // repair_facing) and expressed on this skeleton by Tools/rebuild_authored_clips.py +
        // Tools/correct_authored_anim.py, whose geometric verification passes (feet planted,
        // athletic crouch, hands together on the handle, hands 81 cm IN FRONT of the hips at
        // contact, backlift behind the body; the library adds pull/cut/sweep/defence with the
        // same guarantees). The clip owns the driven bones through the stroke
        // by design; the procedural solve remains the blend basis at each end and the fallback
        // if the clip ever fails to load.
        ApplyAuthoredClip(Shot,DrivenFor(Shot),ClipTime,Weight);
        }
    }
    if(Role==EC26Role::Bowler&&Action==EC26Action::Bowling)
    {
        // The action for the delivery the match asked for (pace / off-spin / leg-spin).
        UAnimSequence* Delivery=SelectBowlingClip();
        if(!Delivery)Delivery=BowlingClip;
        if(Delivery)
        {
        const float ClipLength=Delivery->GetPlayLength();
        const float ClipRelease=C26BowlingReleaseFrame/C26AuthoredFps;
        const float Release=C26Field::ReleasePoseTime;
        // C26MatchGameMode starts this action ReleasePoseTime before the ball leaves the hand and
        // C26Motion::Pace keys it out to 1.34 s, so that is the span the clip has to cover.
        constexpr float Length=1.34f;
        const float ClipTime=ActionTime<=Release
            ?(Release>UE_KINDA_SMALL_NUMBER?(ActionTime/Release)*ClipRelease:ClipRelease)
            :ClipRelease+(ClipLength-ClipRelease)*FMath::Clamp((ActionTime-Release)/FMath::Max(UE_KINDA_SMALL_NUMBER,Length-Release),0.f,1.f);
        const float Weight=FMath::Min(FMath::SmoothStep(0.f,.06f,ActionTime),FMath::SmoothStep(0.f,.10f,Length-ActionTime));
        // Re-enabled with the corrected clip (see the batting branch above for the history). The
        // corrected release frame has the bowling hand fully extended above the head and slightly
        // in front of the shoulder, the mark/gather keys hold the ball in both hands, and the hips
        // carry the bowler 71 cm down the pitch through the action.
        ApplyAuthoredClip(Delivery,DrivenFor(Delivery),ClipTime,Weight);
        }
    }
    if(!Batting&&!Running&&(Action==EC26Action::SignalFour||Action==EC26Action::SignalSix
        ||Action==EC26Action::SignalOut||Action==EC26Action::SignalWide))
    {
        // Signals are target-free pose actions, so an authored clip fits them
        // exactly (pickup/catch/dive/throw are the opposite: they solve toward
        // the live ball and stay procedural). The clip's whole arc plays over
        // ~1 s and its LAST frame IS the signal pose, so clamping holds the
        // arms up for as long as the match holds the action; the exit blend is
        // SmoothPose's job when the next delivery resets the umpire to Ready.
        if(UAnimSequence* Sig=SelectSignalClip())
        {
            const float ClipLength=Sig->GetPlayLength();
            constexpr float Length=1.f;
            const float ClipTime=FMath::Min(ActionTime/Length,1.f)*ClipLength;
            const float Weight=FMath::SmoothStep(0.f,.14f,ActionTime);
            ApplyAuthoredClip(Sig,DrivenFor(Sig),ClipTime,Weight);
        }
    }
    AimHead();
    // One filter stands between every authored pose in this function and the screen. Nothing above
    // this line knows about time: each branch answers "where is this body at this instant", and
    // the athlete used to be snapped onto that answer, so every change of action was a one-frame
    // jump -- ready to running, running to gather, gather to throw, throw back to ready, six times
    // an over per fielder. Now the body travels there.
    SmoothPose(Dt,true);
    Mesh->ApplyLocalPose(Shown);
    if(!AuthoredKit)UpdateUniform();
    PlaceKit(Grip,Dir,Batting,Running);
    UpdateContactShadow();
}

UStaticMeshComponent* AC26Athlete::VisualBat() const
{return Presentation&&Presentation->IsActive()?Presentation->GetBat():Bat.Get();}
