#include "C26Athlete.h"
#include "C26Types.h"
#include "C26Motion.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
AC26Athlete::AC26Athlete()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
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
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Player(TEXT("/Game/Cricket26/Characters/SK_Cricketer_KitBase.SK_Cricketer_KitBase"));
    if(Player.Succeeded())Mesh->SetSkinnedAssetAndUpdate(Player.Object);
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
void AC26Athlete::BeginPlay()
{
    Super::BeginPlay();
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
        AuthoredKit=S->GetName().Contains(TEXT("_Match"));
        const auto& R=S->GetRefSkeleton();Reference.SetNum(R.GetNum());Parents.SetNum(R.GetNum());
        for(int I=0;I<R.GetNum();++I)
        {
            Parents[I]=R.GetParentIndex(I);Reference[I]=R.GetRefBonePose()[I];
            if(Parents[I]>=0)Reference[I]*=Reference[Parents[I]];
            FString Name=R.GetBoneName(I).ToString();Name.RemoveFromStart(TEXT("mixamorig:"));Name.RemoveFromStart(TEXT("mixamorig_"));
            Bones.Add(Name,I);
        }
        // The Mixamo rig imports at roughly 378 cm; the venue is real-world scale. Shrinking the
        // reference pose about the ground origin puts the athlete at about 182 cm so every authored
        // IK target, piece of equipment and contact point lines up. Rotations are untouched.
        // Scale the skinning transforms as well as joint positions. Translating the joints alone
        // compresses the limbs while leaving vertex offsets (head, hair, shoulders) at import size.
        // Equipment is already authored in real centimetres and must not inherit that import scale.
        for(auto& T:Reference)
        {T.SetLocation(T.GetLocation()*0.48f);T.SetScale3D(T.GetScale3D()*0.48f);}
        Pose=Reference;
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
        UE_LOG(LogTemp,Log,TEXT("C26_RIG shoulder=%.1f hip=%.1f ankle=%.1f armspan=%.1f palm=%.1f"),ShoulderZ,HipZ,AnkleZ,ArmSpan,PalmReach);
    }
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
void AC26Athlete::Configure(EC26Role NewRole,int Team,int Number)
{
    if(Shirt&&Role==NewRole&&TeamId==Team){SetAction(EC26Action::Ready);return;}
    Role=NewRole;TeamId=Team;
    // Every athlete material is derived from M_Surface: it is the one generated material with the
    // skeletal-mesh usage flag, and the noise-based ones silently fall back to default grey.
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Surface.M_Surface"));
    if(!Base)return;
    auto Make=[&](FLinearColor C,float Rough)
    {auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Tint"),C);M->SetScalarParameterValue(TEXT("Roughness"),Rough);M->SetScalarParameterValue(TEXT("Glow"),0.f);return M;};
    // Kit albedo sits above the turf's so the players separate from the field they stand on.
    const FLinearColor Team0(.030,.345,.395),Team1(.660,.100,.058),Official(.070,.092,.140);
    const FLinearColor Kit=Role==EC26Role::Umpire?Official:Team==0?Team0:Team1;
    auto* Fabric=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Fabric.M_Fabric"));
    Shirt=UMaterialInstanceDynamic::Create(Fabric?Fabric:Base,this);Shirt->SetVectorParameterValue(TEXT("Tint"),Kit);
    // Cricket whites. Team-coloured trousers made the striker read as one teal mass from the
    // batting camera: shirt, trousers and helmet all returned the same value, so the only thing
    // separating his legs from his torso was a shadow. Cream trousers also give the pads
    // something to sit against -- white gear on a white leg is the one pairing that does not
    // read, so the trouser is warmed and the pads stay cool and brighter.
    Trousers=Make(Role==EC26Role::Umpire?FLinearColor(.020,.024,.036):FLinearColor(.560,.545,.500),.92f);
    // Skin has to survive the same floodlit night as the shirt. The imported Bodymat response goes
    // almost black on vertical surfaces, so the head and forearms take a controlled mid-brown that
    // stays readable without blowing out. Linear-space, roughly sRGB (215,168,146) darkened a stop
    // for the 4-lux key.
    SquadNumber=Number;
    // Deterministic per-player skin variation so ten athletes sharing one team kit do not
    // read as clones. Team shirt/trouser colours are never varied.
    const float Tone=0.90f+0.20f*float((FMath::Abs(Number)*37)%10)/10.f;
    Skin=Make(FLinearColor(.42f*Tone,.235f*Tone,.155f*Tone),.62f);
    Gear=Make(FLinearColor(.58,.61,.57),.86f);
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
        for(int I=0;I<S->GetMaterials().Num();++I)
        {
            const FString Name=S->GetMaterials()[I].MaterialSlotName.ToString();
            if(Name.Contains(TEXT("Top"))||Name.Contains(TEXT("Jersey")))Mesh->SetMaterial(I,Shirt);
            else if(Name.Contains(TEXT("Bottom"))||Name.Contains(TEXT("Trouser")))Mesh->SetMaterial(I,Trousers);
            else if(Name.Contains(TEXT("Body")))Mesh->SetMaterial(I,Skin);
            if(Name.Contains(TEXT("Hair")))
                for(int LOD=0;LOD<S->GetLODNum();++LOD)Mesh->ShowMaterialSection(I,0,false,LOD);
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
    auto MakeFrom=[&](UMaterialInterface* From,FLinearColor C,float Rough)
    {auto* M=UMaterialInstanceDynamic::Create(From?From:Base,this);M->SetVectorParameterValue(TEXT("Tint"),C);M->SetScalarParameterValue(TEXT("Roughness"),Rough);M->SetScalarParameterValue(TEXT("Glow"),0.f);return M;};

    Dress(Bat,TEXT("Willow"),MakeFrom(Willow,FLinearColor(.402,.330,.196),.42f));
    Dress(Bat,TEXT("Grip"),Make(FLinearColor(.016,.018,.022),.88f));
    Dress(Bat,TEXT("Cane"),Make(FLinearColor(.300,.222,.118),.56f));
    Dress(Bat,TEXT("Twine"),Make(FLinearColor(.052,.046,.040),.80f));
    Dress(Bat,TEXT("Label"),Make(Accent*1.15f+FLinearColor(.03,.03,.03),.34f));

    Dress(Headwear,TEXT("Shell"),Make(Kit*.92f,.20f));
    Dress(Headwear,TEXT("Crown"),Make(Kit*.92f,.58f));
    Dress(Headwear,TEXT("Peak"),Make(Kit*.66f,.19f));
    Dress(Headwear,TEXT("Trim"),Make(FLinearColor(.022,.024,.029),.55f));
    Dress(Headwear,TEXT("Pad"),Make(FLinearColor(.036,.034,.032),.93f));
    Dress(Grill,TEXT("Bar"),Make(FLinearColor(.300,.318,.348),.22f));
    Dress(Grill,TEXT("Trim"),Make(FLinearColor(.022,.024,.029),.55f));

    // Pads and gloves read as protective gear because they are matte and slightly off-white, and
    // because the straps and buckles that break them up are dark and sharp against that.
    auto* PadFace=Make(FLinearColor(.700,.712,.686),.76f);
    auto* PadRoll=Make(FLinearColor(.612,.624,.600),.84f);
    auto* Strap=Make(FLinearColor(.028,.030,.036),.70f);
    auto* Buckle=Make(FLinearColor(.330,.342,.362),.26f);
    for(UStaticMeshComponent* P:{PadL.Get(),PadR.Get()})
    {
        Dress(P,TEXT("PadFace"),PadFace);Dress(P,TEXT("PadRoll"),PadRoll);
        Dress(P,TEXT("PadStrap"),Strap);Dress(P,TEXT("PadBuckle"),Buckle);
    }
    auto* GlovePalm=Make(FLinearColor(.212,.150,.098),.62f);
    auto* GlovePad=Make(FLinearColor(.732,.744,.716),.74f);
    for(UStaticMeshComponent* G:{GloveL.Get(),GloveR.Get()})
    {
        Dress(G,TEXT("GlovePalm"),GlovePalm);Dress(G,TEXT("GlovePad"),GlovePad);
        Dress(G,TEXT("GloveCuff"),PadRoll);Dress(G,TEXT("PadStrap"),Strap);
    }
    // Cricket shoes, not the base character's street trainers. Under the night rig those read as
    // two black holes exactly where the athlete meets the turf, which is the worst place in the
    // frame to lose contrast: the feet are what ground the player.
    auto* ShoeUpper=Make(FLinearColor(.740,.752,.734),.44f);
    auto* ShoeSole=Make(FLinearColor(.048,.052,.062),.66f);
    auto* ShoeFlash=Make(Accent,.40f);
    auto* ShoeLace=Make(FLinearColor(.520,.528,.512),.90f);
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

    // Role-based visual resolver. Baked hero scans are used ONLY where their baked
    // equipment matches the match role, verified scan by scan in Blender against the
    // source FBX plus its runtime texture:
    //   Batter (striker #7 and non-striker #18)
    //                        raw07 bake: helmet + pads + gloves + bat (own UVs/material)
    //   Keeper                   raw09 bake: helmet + pads + gloves, NO bat
    // NOTE: the raw06 "batter" bake is a truncated half-body (shorts, no legs, no gear)
    // and must never be assigned. Both batters therefore share the raw07 bake.
    // Bowlers and fielders resolve to the animated team kit: every remaining scan
    // carries baked batting pads, which those roles must never wear. Pairings are
    // always mesh+material from the SAME source bake; cross-bake pairing renders as
    // camouflage noise, so any half-missing pick falls back to the animated kit with
    // role-gated separate equipment (never slop) and logs an error.
    auto TryMesh=[](const TCHAR* Path)->UStaticMesh*
    {
        if(!Path)return nullptr;
        if(UStaticMesh* M=LoadObject<UStaticMesh>(nullptr,Path))return M;
        UE_LOG(LogC26,Error,TEXT("C26_VISUAL missing hero mesh %s"),Path);
        return nullptr;
    };
    auto TryMat=[](const TCHAR* Path)->UMaterialInterface*
    {
        if(!Path)return nullptr;
        if(UMaterialInterface* M=LoadObject<UMaterialInterface>(nullptr,Path))return M;
        UE_LOG(LogC26,Error,TEXT("C26_VISUAL missing hero material %s"),Path);
        return nullptr;
    };
    const TCHAR* MeshPath=nullptr;
    const TCHAR* MatPath=nullptr;
    auto Consider=[&](const TCHAR* MP,const TCHAR* TP)->bool
    {
        UStaticMesh* M=TryMesh(MP);if(!M)return false;
        UMaterialInterface* MI=TryMat(TP);if(!MI)return false;
        MeshPath=MP;MatPath=TP;return true;
    };
    bHeroVisual=false;
    if(Role==EC26Role::Batter)
    {
        // USER DIRECTIVE: Fix the batter with a half body, make him full body.
        // Also he should have normal batting movement, not a static box-like figure.
        // Same for non-striker.
        // Striker and non-striker use the full-body animated skeletal mesh (SK_Cricketer_KitBase).
        MeshPath=nullptr;
        MatPath=nullptr;
    }
    else if(Role==EC26Role::Keeper)
    {
        Consider(TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Keeper.SM_C26_Player_Keeper"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Keeper.MI_Player_Keeper"));
    }
    else if(Role==EC26Role::Umpire)
    {
        Consider(TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Umpire.SM_C26_Player_Umpire"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Umpire.MI_Player_Umpire"));
    }
    else if(Role==EC26Role::Bowler)
    {
        Consider(TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Bowler.SM_C26_Player_Bowler"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Bowler.MI_Player_Bowler"));
    }
    else // Fielders
    {
        static const TCHAR* FielderMeshes[] = {
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_01.SM_C26_Player_Fielder_01"),
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_02.SM_C26_Player_Fielder_02"),
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_03.SM_C26_Player_Fielder_03"),
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_04.SM_C26_Player_Fielder_04"),
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_05.SM_C26_Player_Fielder_05"),
            TEXT("/Game/Cricket26/Characters/Players/SM_C26_Player_Fielder_06.SM_C26_Player_Fielder_06"),
        };
        static const TCHAR* FielderMats[] = {
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_01.MI_Player_Fielder_01"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_02.MI_Player_Fielder_02"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_03.MI_Player_Fielder_03"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_04.MI_Player_Fielder_04"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_05.MI_Player_Fielder_05"),
            TEXT("/Game/Cricket26/Materials/Players/MI_Player_Fielder_06.MI_Player_Fielder_06"),
        };
        const int Idx = FMath::Abs(SquadNumber) % 6;
        Consider(FielderMeshes[Idx], FielderMats[Idx]);
    }
    if(MeshPath)
    {
        if(UStaticMesh* LoadedMesh=LoadObject<UStaticMesh>(nullptr,MeshPath))
        {
            HeroMesh->SetStaticMesh(LoadedMesh);
            if(UMaterialInterface* LoadedMat=LoadObject<UMaterialInterface>(nullptr,MatPath))
                HeroMesh->SetMaterial(0,LoadedMat);
            HeroMesh->SetRelativeLocation(FVector::ZeroVector);
            HeroMesh->SetRelativeRotation(FRotator::ZeroRotator);
            // The batter bake exported at 162.7 cm; a uniform lift puts both batters
            // in the senior range. The origin is grounded so feet stay planted.
            const float HeroScale=(Role==EC26Role::Batter)?1.06f:1.f;
            HeroMesh->SetRelativeScale3D(FVector(HeroScale));
            bHeroVisual=true;
        }
    }
    else HeroMesh->SetStaticMesh(nullptr);

    ApplyVisualRole();
    SetAction(EC26Action::Ready);
}
void AC26Athlete::ApplyVisualRole()
{
    // Exactly one visible body per athlete. Hero scans carry pads/helmet/bat baked in;
    // the animated kit shows team clothing with shoes, and role-appropriate separate
    // equipment is gated in ApplyDetail. Switching roles re-runs Configure, which lands
    // here, so no stale mesh survives an innings change.
    const bool Hero=bHeroVisual&&HeroMesh&&HeroMesh->GetStaticMesh();
    bHeroVisual=Hero;
    if(HeroMesh){HeroMesh->SetVisibility(Hero);HeroMesh->SetHiddenInGame(!Hero);}
    Mesh->SetVisibility(!Hero);Mesh->SetHiddenInGame(Hero);
    Uniform->SetVisibility(!Hero);Uniform->SetHiddenInGame(Hero);
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
    if(H<0||LookAt.IsZero()||!Pose.IsValidIndex(H))return;
    const FVector Local=Mesh->GetComponentTransform().InverseTransformPosition(LookAt);
    FVector Want=Local-Pose[H].GetLocation();Want.Z*=.6f;Want=Want.GetSafeNormal();
    if(Want.IsNearlyZero())return;
    // Work out which head-local axis currently points out of the face, then rotate that onto the
    // target. Aiming the neck-to-head bone vector instead tips the skull over sideways.
    auto Face=[&](int Index)
    {
        const FVector L=Reference[Index].GetRotation().UnrotateVector(RigForward);
        return Pose[Index].GetRotation().RotateVector(L).GetSafeNormal();
    };
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
void AC26Athlete::SetAction(EC26Action NewAction,bool ResetTime){if(NewAction!=Action||ResetTime)ActionTime=0;Action=NewAction;}
void AC26Athlete::ResetAt(const FVector& Position,float Yaw)
{SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));MotionTime=0;MoveSpeed=0;GaitPhase=0;Trigger=0;ContactTarget=FVector::ZeroVector;SetAction(EC26Action::Ready);Animate(0);}
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
    const int I=Bone(TEXT("RightHand"));
    if(I<0||!Pose.IsValidIndex(I))return GetActorLocation()+FVector(0,0,210);
    return Mesh->GetComponentTransform().TransformPosition(Palm(true));
}
FVector AC26Athlete::ReceivingPosition() const
{
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
void AC26Athlete::Animate(float Dt)
{
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
    if(Dt>0.f&&Detail!=EDetail::Hero&&++SkipPhase%(Detail==EDetail::Distant?3:2)!=0)
    {
        // Every clock still advances on a skipped frame; only the rate at which the pose is
        // re-solved falls, so nothing drifts and nothing snaps when the athlete is promoted back
        // to hero. Callers asking for an exact instant pass Dt of zero and never land here.
        ActionTime+=Dt;MotionTime+=Dt;
        if(Action==EC26Action::Running)GaitPhase+=Dt*FMath::Clamp(MoveSpeed/60.f,0.f,15.f);
        return;
    }
    ActionTime+=Dt;MotionTime+=Dt;Pose=Reference;
    const bool Running=Action==EC26Action::Running;
    const bool Batting=Role==EC26Role::Batter;
    const bool Keeping=Role==EC26Role::Keeper;
    // Stride frequency follows the distance actually being covered, so feet stop skating.
    const float Cadence=FMath::Clamp(MoveSpeed/60.f,0.f,15.f);
    if(Running)GaitPhase+=Dt*Cadence;
    const float Gait=FMath::Sin(GaitPhase);
    const float Sway=FMath::Sin(MotionTime*1.7f);

    // Crouch is a hip drop; Shift moves the pelvis horizontally. A batter flexes his knees, he
    // does not sit down: the old fixed 23 cm drop held him in a squat through an entire stroke.
    float Crouch=Keeping?-42.f:Batting?-13.f:-7.f;
    FVector Shift=FVector::ZeroVector;
    float TurnRight=0,LeanForward=Keeping?24.f:Batting?9.f:7.f,LeanRight=0;
    if(Running){Crouch=-5.f+3.f*FMath::Abs(Gait);LeanForward=13.f;}
    if(Action==EC26Action::Pickup)
    {const float T=FMath::Clamp(ActionTime/.55f,0.f,1.f);Crouch=-52.f*FMath::Sin(T*PI);LeanForward=18.f+34.f*FMath::Sin(T*PI);}
    if(Role==EC26Role::Umpire){Crouch=-2.f;LeanForward=2.f;}

    FVector FL=Reference[FMath::Max(0,Bone(TEXT("LeftFoot")))].GetLocation();
    FVector FR=Reference[FMath::Max(0,Bone(TEXT("RightFoot")))].GetLocation();
    FVector LH=Rig(20,-22,96+Crouch),RH=Rig(20,22,96+Crouch);
    FVector Grip=Rig(10,14,85),Dir=Rig(.10f,.05f,.993f).GetSafeNormal();

    if(Running)
    {
        const FVector L=C26Motion::RunningFoot(GaitPhase,MoveSpeed,-9,AnkleZ,Role==EC26Role::Bowler);
        const FVector R=C26Motion::RunningFoot(GaitPhase,MoveSpeed,9,AnkleZ,Role==EC26Role::Bowler);
        FL=Rig(L.X,L.Y,L.Z);FR=Rig(R.X,R.Y,R.Z);
        LH=Rig(-Gait*38,-21,114);RH=Rig(Gait*38,21,114);
        if(Role==EC26Role::Bowler)
        {
            // A fast bowler's approach, not a jog. Longer stride, high knee drive, and arms that
            // pump with the elbows tucked and the leading hand rising as it comes through. The
            // neutral run stays as it is for fielders and for running between the wickets.
            
            LH=Rig(-Gait*31,-16,119+FMath::Max(0.f,-Gait)*13);
            RH=Rig(Gait*31,16,119+FMath::Max(0.f,Gait)*13);
            LeanForward=18.f;
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
    else if(Keeping){FL=Rig(4,-21,AnkleZ);FR=Rig(4,21,AnkleZ);LH=Rig(33,-14,44);RH=Rig(33,14,44);}
    else if(Role==EC26Role::Umpire){FL=Rig(0,-13,AnkleZ);FR=Rig(0,13,AnkleZ);LH=Rig(1,-23,95);RH=Rig(1,23,95);}
    else if(Role==EC26Role::Bowler)
    {
        // At the top of his mark. A bowler waiting to run in stands tall and square with the ball
        // held in both hands at chest height and his weight rocking onto the front foot -- it is
        // a completely different body language from a fielder crouched in the ring, and until now
        // the two were the same pose. This is what makes the active bowler readable as the active
        // bowler from the batting camera before he has moved.
        Crouch=-4.f;LeanForward=9.f;TurnRight=-14.f;
        FL=Rig(9,-11,AnkleZ);FR=Rig(-9,13,AnkleZ);
        const float Rock=FMath::Sin(MotionTime*1.35f);
        LH=Rig(26.f+Rock*1.6f,-6.f,124.f);RH=Rig(26.f+Rock*1.6f,4.f,124.f);
    }
    else
    {
        // Fielder at the ready: split stance, weight forward, hands live.
        FL=Rig(2,-15,AnkleZ);FR=Rig(-2,15,AnkleZ);
        LH=Rig(23+Sway*2.f,-23,92+Crouch);RH=Rig(23-Sway*2.f,23,92+Crouch);
    }

    if(Batting&&!Running)
    {
        if(Action==EC26Action::Batting)
        {
            const FVector Contact=Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
            const float Rad=FMath::DegreesToRadians(ShotAngle);
            const FVector Away=Rig(FMath::Cos(Rad),FMath::Sin(Rad),0).GetSafeNormal();
            if(Defending)
            {
                const float Swing=FMath::SmoothStep(0.f,1.f,FMath::Clamp(ActionTime/C26Field::BatContactPoseTime,0.f,1.f));
                const FVector Back=Rig(-4,16,112);
                const FVector Down=Contact+Rig(0,0,MiddleDrop);
                Grip=FMath::Lerp(Back,Down,Swing);
                Dir=FMath::Lerp(Rig(-.20f,.10f,.97f),(Grip-Contact).GetSafeNormal(),Swing).GetSafeNormal();
                TurnRight=40.f;LeanForward=17.f+Swing*7.f;
                FL=Rig(15.f+Swing*17.f,-3,AnkleZ);
            }
            else
            {
                const float Length=Loft?.72f:.62f;
                const float T=FMath::Clamp(ActionTime/Length,0.f,1.f);
                const float Swing=FMath::SmoothStep(0.f,1.f,FMath::Clamp(ActionTime/C26Field::BatContactPoseTime,0.f,1.f));
                const float Follow=FMath::SmoothStep(0.f,1.f,FMath::Clamp((ActionTime-C26Field::BatContactPoseTime)/(Length-C26Field::BatContactPoseTime),0.f,1.f));
                const bool Cross=Contact.Z>108.f||((ShotAngle>65.f||ShotAngle<-55.f)&&Contact.Z>38.f);
                // Backlift over the off shoulder, down through the ball, then wrap the follow-through
                // around the body in the direction the ball was actually hit.
                const FVector Lift=Cross?Rig(-14,20,124):Rig(-9,17,116);
                const FVector Meet=Contact+(Cross?(Rig(0,0,.55f)+Away*.62f).GetSafeNormal():Rig(0,0,1))*MiddleDrop;
                const FVector Wrap=Cross?Rig(4,-24,150)+Away*22.f:Rig(20,-10,158)+Away*16.f;
                Grip=Swing<1.f?FMath::Lerp(Lift,Meet,Swing):FMath::Lerp(Meet,Wrap,Follow);
                const FVector Held=(Grip-Contact).GetSafeNormal(UE_SMALL_NUMBER,Rig(0,0,1));
                const FVector Cocked=Cross?Rig(-.55f,.35f,-.76f).GetSafeNormal():Rig(-.40f,.28f,-.87f).GetSafeNormal();
                const FVector Through=Cross?(-Away*.72f+Rig(0,0,.70f)).GetSafeNormal():(-Away*.45f+Rig(-.30f,0,.84f)).GetSafeNormal();
                Dir=(Swing<1.f?FMath::Lerp(Cocked,Held,Swing):FMath::Lerp(Held,Through,Follow)).GetSafeNormal(UE_SMALL_NUMBER,Rig(0,0,1));
                TurnRight=46.f-Follow*30.f+(Cross?12.f:0.f);
                LeanForward=15.f+Swing*(Cross?3.f:17.f)-Follow*6.f;
                LeanRight=4.f+(Cross?-8.f:5.f)*Swing;
                // The golden delivery: a dead-straight front-foot drive gets a bigger press forward,
                // more weight over the front knee and a squarer chest so the head goes to the ball.
                const bool Straight=FMath::Abs(ShotAngle)<=15.f&&!Loft;
                if(Straight){LeanForward+=6.f;TurnRight-=4.f;LeanRight+=1.5f;}
                if(Cross)FR=Rig(-14.f-FMath::Sin(T*PI)*13.f,11,AnkleZ);
                else
                {
                    // Weight transfer, not a squat. The pelvis presses forward onto the striding
                    // foot and dips as the front knee takes the load, the back heel comes up, and
                    // the chest goes out over the ball. That is also what lets the hands get low
                    // and far enough forward to meet the ball on the middle of the blade rather
                    // than the last few centimetres of the toe.
                    Crouch=-13.f-Swing*10.f+Follow*6.f;
                    Shift=Rig(Swing*(Straight?17.f:12.f)-Follow*4.f,Swing*2.f,0);
                    FL=Rig(15.f+FMath::Sin(T*PI)*(Straight?34.f:26.f),-4,AnkleZ);
                    FR=Rig(-13.f,9.f,AnkleZ+Swing*6.f);
                }
            }
        }
        else if(Action==EC26Action::Ready)
        {
            // Rhythmic bat tap and a small weight shift; a still batter reads as a mannequin.
            const float Tap=FMath::Max(0.f,FMath::Sin(FMath::Fmod(MotionTime*3.1f,1.f)*PI))*9.f;
            const FVector Toe=Rig(4,16,6.f+Tap);
            Dir=Rig(.10f,.05f,.993f).GetSafeNormal();
            Grip=Toe+Dir*BatLength;
            TurnRight=46.f+Sway*2.5f;LeanForward=22.f;
        }
        else if(Action==EC26Action::Celebrate){Grip=Rig(6,26,196);Dir=Rig(-.25f,.30f,.92f).GetSafeNormal();TurnRight=12.f;LeanForward=-6.f;}
        else if(Action==EC26Action::Disappointed){Grip=Rig(14,16,74);Dir=Rig(.55f,.10f,.83f).GetSafeNormal();TurnRight=22.f;LeanForward=24.f;}
        else{Grip=Rig(10,14,85);Dir=Rig(.10f,.05f,.993f).GetSafeNormal();TurnRight=30.f;}
        if(NonStriker&&Action==EC26Action::Ready)
        {
            TurnRight=12.f;LeanForward=8.f;Crouch=-6.f;
            FL=Rig(8,-11,AnkleZ);FR=Rig(-8,11,AnkleZ);
            Grip=Rig(18,20,85);Dir=Rig(.15f,0,.99f).GetSafeNormal();
        }
        else if(Action==EC26Action::Ready)
        {
            const float Press=FMath::Sin(Trigger*PI);
            Shift=Rig(-Press*3.f,Press*1.5f,0);Grip.Z+=Press*9.f;
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

    if(Action==EC26Action::Bowling)
    {
        const auto P=C26Motion::Pace(ActionTime,AnkleZ);
        const float Blend=FMath::SmoothStep(0.f,.13f,ActionTime);
        const FVector RunL=C26Motion::RunningFoot(GaitPhase,MoveSpeed,-9,AnkleZ,true);
        const FVector RunR=C26Motion::RunningFoot(GaitPhase,MoveSpeed,9,AnkleZ,true);
        FL=FMath::Lerp(Rig(RunL.X,RunL.Y,RunL.Z),Rig(P.LeftFoot.X,P.LeftFoot.Y,P.LeftFoot.Z),Blend);
        FR=FMath::Lerp(Rig(RunR.X,RunR.Y,RunR.Z),Rig(P.RightFoot.X,P.RightFoot.Y,P.RightFoot.Z),Blend);
        RH=FMath::Lerp(Rig(Gait*31,16,119+FMath::Max(0.f,Gait)*13),Rig(P.RightHand.X,P.RightHand.Y,P.RightHand.Z),Blend);
        LH=FMath::Lerp(Rig(-Gait*31,-16,119+FMath::Max(0.f,-Gait)*13),Rig(P.LeftHand.X,P.LeftHand.Y,P.LeftHand.Z),Blend);
        TurnRight=P.Turn;LeanForward=P.Lean;LeanRight=P.Side;Crouch=P.HipDrop;
    }

    if(Action==EC26Action::Catch||Action==EC26Action::Pickup)
    {
        FVector Take=ContactTarget.IsZero()?Rig(34,0,142):Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        const float Gather=FMath::SmoothStep(0.f,.20f,ActionTime);
        const float Recover=Action==EC26Action::Pickup?FMath::SmoothStep(.24f,.53f,ActionTime):FMath::SmoothStep(.18f,.42f,ActionTime);
        if(Action==EC26Action::Pickup)
        {
            // A cricketer gathers by striding at the ball and bending from the waist, not by
            // squatting on the spot. The old pose dropped the hips 73 cm over feet that stayed
            // under the body, which folded both legs to 36 cm of an 88 cm chain -- it rendered as
            // a man kneeling. Worse, it put the shoulders 45 cm above and BEHIND the ball, and
            // the arms are only 51.8 cm long, so the two-bone IK silently clamped and the hands
            // never arrived: the ball was teleported into them. Striding through instead keeps
            // the front leg long, carries the shoulders out over the ball and lets the hands
            // genuinely reach it.
            // How far he has to get down is the ball's problem, not a constant: a ball dying at
            // his ankles needs the full bend, one that has bounced up to his hip needs almost none.
            const float Low=FMath::Clamp((92.f-Take.Z)/84.f,0.f,1.f)*Gather;
            Crouch=FMath::Lerp(-7.f,-70.f,Low)*(1.f-Recover*.62f);
            LeanForward=18.f+Low*68.f-Recover*54.f;
            Shift=Rig(Low*10.f*(1.f-Recover),0,0);
            FL=Rig(26.f+Low*32.f,-19,AnkleZ);FR=Rig(-20.f-Low*6.f,20,AnkleZ);
            Take=FMath::Lerp(Rig(28,0,95),Take,Gather);
        }
        else
        {
            Crouch=Take.Z<60.f?-62.f:Take.Z<100.f?-36.f:-8.f;
            LeanForward=Take.Z<80.f?42.f:12.f;
            FL=Rig(8,-23,AnkleZ);FR=Rig(-5,23,AnkleZ);
        }
        Take=FMath::Lerp(Take,Rig(32,0,116),Recover);
        // Aim the WRISTS one palm short of the ball along the line of the reach, so the fingers
        // close on it. Driving the wrists onto the ball put it behind the hands.
        const FVector Anchor=Rig(0,0,ShoulderZ+Crouch);
        const FVector Reach=(Take-Anchor).GetSafeNormal(UE_SMALL_NUMBER,Rig(1,0,0));
        const FVector Wrists=Take-Reach*PalmReach;
        LH=Wrists+Rig(0,-5,0);RH=Wrists+Rig(0,5,0);
    }
    if(Action==EC26Action::Throw)
    {
        const float T=FMath::Clamp(ActionTime/.5f,0.f,1.f);
        const float A=FMath::DegreesToRadians(-120.f+T*300.f);
        RH=Rig(FMath::Sin(A)*52.f,20.f,150.f+FMath::Cos(A)*62.f);
        LH=Rig(26,-26,132);TurnRight=-20.f+T*40.f;LeanForward=8.f+T*14.f;
    }
    if(Action==EC26Action::Celebrate&&!Batting){LH=Rig(-6,-30,198);RH=Rig(-6,30,198);LeanForward=-8.f;}
    if(Action==EC26Action::Disappointed&&!Batting){LH=Rig(10,-20,120);RH=Rig(10,20,120);LeanForward=22.f;}
    if(Action==EC26Action::SignalSix){LH=Rig(-4,-22,205);RH=Rig(-4,22,205);LeanForward=-4.f;}
    if(Action==EC26Action::SignalOut){RH=Rig(2,20,206);LH=Rig(1,-23,95);}
    if(Action==EC26Action::SignalFour){LH=Rig(20,-72,128);RH=Rig(20,72,128);}
    if(Action==EC26Action::SignalWide){LH=Rig(2,-84,140);RH=Rig(2,84,140);}

    MoveBone(TEXT("Hips"),Shift+FVector(0,0,Crouch));
    Twist(TEXT("Hips"),TurnRight*.42f,LeanForward*.30f,LeanRight*.5f);
    Twist(TEXT("Spine"),TurnRight*.26f,LeanForward*.34f,LeanRight*.3f);
    Twist(TEXT("Spine1"),TurnRight*.20f,LeanForward*.22f,LeanRight*.2f);
    Twist(TEXT("Spine2"),TurnRight*.12f,LeanForward*.14f,0);
    Limb(TEXT("LeftUpLeg"),TEXT("LeftLeg"),TEXT("LeftFoot"),FL,RigForward);
    Limb(TEXT("RightUpLeg"),TEXT("RightLeg"),TEXT("RightFoot"),FR,RigForward);
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
        Pose[F].SetRotation((FQuat(Rig(0,1,0),FMath::DegreesToRadians(Lift*36.f))*Reference[F].GetRotation()).GetNormalized());
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
    Limb(TEXT("LeftArm"),TEXT("LeftForeArm"),TEXT("LeftHand"),LH,Rig(-.7f,-.5f,-.5f));
    Limb(TEXT("RightArm"),TEXT("RightForeArm"),TEXT("RightHand"),RH,Rig(-.7f,.5f,-.5f));
    if(Batting)
    {
        // Rotate palms toward the handle, then curl finger chains inside the padded glove.
        // The imported open-hand idle cannot grip a bat without this hand pose.
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
    AimHead();
    Mesh->ApplyComponentPose(Pose);
    if(!AuthoredKit)UpdateUniform();
    PlaceKit(Grip,Dir,Batting,Running);
    UpdateContactShadow();
}
