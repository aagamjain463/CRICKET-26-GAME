#include "C26Athlete.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Length from the top of the handle to the toe of the blade, and how far below the hands the ball
// meets the middle of the blade. Both are real bat dimensions and both are used by the posing code,
// so bat, hands and contact point can never drift apart.
constexpr float BatLength=83.f;
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
    // The imported rig faces mesh +Y with mesh +X out to its left. Yawing the mesh by -90 makes the
    // actor's own +X the athlete's forward, so every yaw the match code already computes -- fielders
    // turning to the middle, the striker facing the bowler, the bowler running in -- points the
    // right way. Without this the whole side stands square to the play.
    Mesh->SetRelativeRotation(FRotator(0,-90,0));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Player(TEXT("/Game/Cricket26/Characters/SK_Cricketer.SK_Cricketer"));
    if(Player.Succeeded())Mesh->SetSkinnedAssetAndUpdate(Player.Object);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(true);
    // Equipment rides in mesh space so it shares one frame with the posed skeleton.
    Bat=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WillowBat"));Bat->SetupAttachment(Mesh);
    Bat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Grill=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HelmetGrill"));Grill->SetupAttachment(Mesh);
    Grill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Uniform=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TailoredCricketTrousers"));Uniform->SetupAttachment(Mesh);
    Uniform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Shell=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CricketHeadwear"));Shell->SetupAttachment(Mesh);
    Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShirtNumber=CreateDefaultSubobject<UTextRenderComponent>(TEXT("KitNumber"));ShirtNumber->SetupAttachment(Mesh);
    ShirtNumber->SetHorizontalAlignment(EHTA_Center);ShirtNumber->SetVerticalAlignment(EVRTA_TextCenter);
    ShirtNumber->SetWorldSize(19);ShirtNumber->SetTextRenderColor(FColor(213,237,231));ShirtNumber->SetCastShadow(false);
    Helmet=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Helmet"));Helmet->SetupAttachment(Mesh);
    Peak=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmetPeak"));Peak->SetupAttachment(Mesh);
    PadL=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPad"));PadL->SetupAttachment(Mesh);
    PadR=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPad"));PadR->SetupAttachment(Mesh);
    GloveL=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftGlove"));GloveL->SetupAttachment(Mesh);
    GloveR=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightGlove"));GloveR->SetupAttachment(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for(auto* P:{Helmet.Get(),Peak.Get(),PadL.Get(),PadR.Get(),GloveL.Get(),GloveR.Get()})
    {P->SetStaticMesh(Sphere.Object);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetCastShadow(true);}
}
void AC26Athlete::BeginPlay()
{
    Super::BeginPlay();
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
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
        UE_LOG(LogTemp,Log,TEXT("C26_RIG shoulder=%.1f hip=%.1f ankle=%.1f armspan=%.1f"),ShoulderZ,HipZ,AnkleZ,ArmSpan);
    }
    BuildEquipment();
}
void AC26Athlete::BuildEquipment()
{
    TArray<FVector> V,N;TArray<int32>T;TArray<FVector2D>UV;TArray<FLinearColor>C;TArray<FProcMeshTangent>Tan;
    // Shaped willow: a flat hitting face on +X, a swelled back, tapered shoulders, rounded toe and a
    // separate rubber grip. The face direction is the component's own +X, so the posing code can aim
    // the bat face along the shot instead of leaving the roll to chance.
    const float Z[]={0,-13,-24,-32,-58,-74,-83};
    const float Wide[]={1.45f,1.45f,2.2f,5.35f,5.40f,5.05f,3.30f};
    const float Back[]={1.45f,1.45f,2.1f,3.55f,3.60f,3.20f,2.10f};
    const int Rows=7,Sides=10;
    for(int R=0;R<Rows;++R)for(int J=0;J<Sides;++J)
    {
        const float A=2*PI*J/Sides,S=FMath::Sin(A),Co=FMath::Cos(A);
        const float Depth=S>0?Back[R]*S:0.55f*S*(R<2?2.6f:1.f);
        V.Add(FVector(-Depth,Co*Wide[R],Z[R]));
        N.Add(FVector(-FMath::Sign(Depth==0.f?1.f:Depth)*FMath::Abs(S),Co,0).GetSafeNormal());
        UV.Add(FVector2D(J/float(Sides),R/float(Rows-1)));
    }
    for(int R=0;R<Rows-1;++R)for(int J=0;J<Sides;++J)
    {const int A=R*Sides+J,B=R*Sides+(J+1)%Sides;T.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
    Bat->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
    V.Reset();T.Reset();N.Reset();UV.Reset();
    for(int R=0;R<2;++R)for(int J=0;J<Sides;++J)
    {
        const float A=2*PI*J/Sides;
        V.Add(FVector(FMath::Sin(A)*1.75f,FMath::Cos(A)*1.75f,R?-25.f:1.f));
        N.Add(FVector(FMath::Sin(A),FMath::Cos(A),0));UV.Add(FVector2D(J/float(Sides),R));
    }
    for(int J=0;J<Sides;++J){const int A=J,B=(J+1)%Sides;T.Append({A,B,A+Sides,B,B+Sides,A+Sides});}
    Bat->CreateMeshSection_LinearColor(1,V,T,N,UV,C,Tan,false);
    // Helmet grille: five slim bars across the face, built in the head's own forward frame.
    V.Reset();T.Reset();N.Reset();UV.Reset();
    for(int Bar=0;Bar<5;++Bar)
    {
        const int K=V.Num();const float H=-1.f-Bar*2.7f,Half=7.6f-Bar*.35f;
        V.Append({FVector(9.2f,-Half,H),FVector(10.f,-Half,H+.55f),FVector(10.f,Half,H+.55f),FVector(9.2f,Half,H)});
        T.Append({K,K+1,K+2,K,K+2,K+3});
        for(int J=0;J<4;++J){N.Add(FVector(1,0,.2f).GetSafeNormal());UV.Add(FVector2D::ZeroVector);}
    }
    for(int Side:{-1,1})
    {
        const int K=V.Num();
        V.Append({FVector(9.2f,Side*7.6f,-1.f),FVector(9.2f,Side*8.f,-1.f),FVector(9.2f,Side*8.f,-13.f),FVector(9.2f,Side*7.6f,-13.f)});
        T.Append({K,K+1,K+2,K,K+2,K+3});
        for(int J=0;J<4;++J){N.Add(FVector(0,float(Side),0));UV.Add(FVector2D::ZeroVector);}
    }
    Grill->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
    V.Reset();T.Reset();N.Reset();UV.Reset();
    // Open-faced shell: crown and sides protect the head, while the face stays visible behind
    // the grille. A complete sphere cannot represent a cricket helmet.
    constexpr int CrownRings=7,CrownSides=24;
    for(int R=0;R<CrownRings;++R)for(int J=0;J<CrownSides;++J)
    {
        const float A=J*2*PI/CrownSides,E=.02f+R*1.57f/(CrownRings-1);
        const FVector Normal(FMath::Sin(E)*FMath::Cos(A),FMath::Sin(E)*FMath::Sin(A),FMath::Cos(E));
        V.Add(Normal*FVector(12.3,11.8,12.3));N.Add(Normal);UV.Add(FVector2D(J/float(CrownSides),R/float(CrownRings-1)));
    }
    for(int R=0;R<CrownRings-1;++R)for(int J=0;J<CrownSides;++J)
    {int A=R*CrownSides+J,B=R*CrownSides+(J+1)%CrownSides;T.Append({A,A+CrownSides,B,B,A+CrownSides,B+CrownSides});}
    Shell->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
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
    const FLinearColor Team0(.010,.255,.290),Team1(.560,.055,.030),Official(.055,.075,.115);
    const FLinearColor Kit=Role==EC26Role::Umpire?Official:Team==0?Team0:Team1;
    auto* Fabric=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Fabric.M_Fabric"));
    Shirt=UMaterialInstanceDynamic::Create(Fabric?Fabric:Base,this);Shirt->SetVectorParameterValue(TEXT("Tint"),Kit);
    // Trousers sit a shade cooler and flatter than the shirt so the two halves of the kit separate
    // in silhouette instead of reading as one moulded block of colour.
    Trousers=Make(Role==EC26Role::Umpire?FLinearColor(.020,.024,.036):Kit*.62f+FLinearColor(.055,.058,.062),.90f);
    Gear=Make(FLinearColor(.58,.61,.57),.86f);
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
        for(int I=0;I<S->GetMaterials().Num();++I)
        {
            const FString Name=S->GetMaterials()[I].MaterialSlotName.ToString();
            if(Name.Contains(TEXT("Top")))Mesh->SetMaterial(I,Shirt);
            else if(Name.Contains(TEXT("Bottom")))Mesh->SetMaterial(I,Trousers);
            if(Name.Contains(TEXT("Bottom"))||Name.Contains(TEXT("Hair")))
                for(int LOD=0;LOD<S->GetLODNum();++LOD)Mesh->ShowMaterialSection(I,0,false,LOD);
        }
    }
    Uniform->SetMaterial(0,Trousers);Uniform->SetMaterial(1,Make(Team==0?FLinearColor(.10,.40,.43):FLinearColor(.72,.24,.07),.88f));
    Bat->SetMaterial(0,Make(FLinearColor(.315,.258,.158),.60f));
    Bat->SetMaterial(1,Make(FLinearColor(.020,.022,.026),.86f));
    Helmet->SetMaterial(0,Make(Kit*.85f,.30f));Peak->SetMaterial(0,Make(Kit*.85f,.30f));
    Shell->SetMaterial(0,Make(Kit*.62f,.36f));
    Grill->SetMaterial(0,Make(FLinearColor(.045,.050,.058),.34f));
    PadL->SetMaterial(0,Gear);PadR->SetMaterial(0,Gear);
    GloveL->SetMaterial(0,Gear);GloveR->SetMaterial(0,Gear);
    const bool Batting=Role==EC26Role::Batter,Keeping=Role==EC26Role::Keeper;
    const bool Guarded=Batting||Keeping;
    Bat->SetVisibility(Batting);
    Helmet->SetVisibility(false);Shell->SetVisibility(true);Peak->SetVisibility(true);Grill->SetVisibility(Guarded);
    Shell->SetRelativeScale3D(Guarded?FVector(1):FVector(1,1,.68f));
    PadL->SetVisibility(Guarded);PadR->SetVisibility(Guarded);
    GloveL->SetVisibility(Guarded);GloveR->SetVisibility(Guarded);
    Helmet->SetRelativeScale3D(FVector(.235,.250,.250));
    Peak->SetRelativeScale3D(FVector(.150,.215,.038));
    // Local X is depth (out the front of the shin), Y is width across it, Z is along it. The pad has
    // to be visibly wider and deeper than the trouser tube underneath or the leg extrudes through it.
    const FVector PadSize=Keeping?FVector(.110,.180,.380):FVector(.120,.205,.470);
    PadL->SetRelativeScale3D(PadSize);PadR->SetRelativeScale3D(PadSize);
    const FVector GloveSize=Keeping?FVector(.185,.170,.245):FVector(.125,.140,.215);
    GloveL->SetRelativeScale3D(GloveSize);GloveR->SetRelativeScale3D(GloveSize);
    ShirtNumber->SetText(FText::AsNumber(Number));ShirtNumber->SetVisibility(Role!=EC26Role::Umpire);
    SetAction(EC26Action::Ready);
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
{SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));MotionTime=0;MoveSpeed=0;SetAction(EC26Action::Ready);Animate(0);}
void AC26Athlete::SetShotContact(const FVector& Target,float Angle,bool bLoft)
{ContactTarget=Target;ShotAngle=Angle;Loft=bLoft;SetAction(EC26Action::Batting);}
FVector AC26Athlete::HandPosition() const
{int I=Bone(TEXT("RightHand"));return I>=0&&Pose.IsValidIndex(I)?Mesh->GetComponentTransform().TransformPosition(Pose[I].GetLocation()):GetActorLocation()+FVector(0,0,210);}
void AC26Athlete::PlaceKit(const FVector& Grip,const FVector& Dir,bool Batting,bool Running)
{
    const int Head=Bone(TEXT("Head"));
    if(Head>=0)
    {
        const FVector L=Reference[Head].GetRotation().UnrotateVector(RigForward);
        const FVector Face=Pose[Head].GetRotation().RotateVector(L).GetSafeNormal();
        const FRotator Look=FRotationMatrix::MakeFromXZ(Face,FVector::UpVector).Rotator();
        const FVector Skull=Pose[Head].GetLocation()+FVector(0,0,14.f)+Face*.8f;
        Helmet->SetRelativeLocation(Skull);Helmet->SetRelativeRotation(Look);
        Shell->SetRelativeLocation(Skull);Shell->SetRelativeRotation(Look);
        Peak->SetRelativeLocation(Skull+Face*9.5f+FVector(0,0,2.5f));Peak->SetRelativeRotation(Look);
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
        Pad->SetRelativeLocation(FMath::Lerp(Ft,Kn,.56f)+Shin.GetSafeNormal()*3.5f+Facing*4.2f);
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
    // Lightweight cloth envelope follows the same posed joints as the skin. No cloth simulation
    // and no extra skeletons; distant players can update this with their reduced pose cadence.
    TArray<FVector> Vertices,Normals,StripeV,StripeN;TArray<int32> Indices,StripeT;
    TArray<FVector2D> UV,StripeUV;TArray<FLinearColor> Colors;TArray<FProcMeshTangent> Tangents;
    auto Leg=[&](const FString& Side,float Sign)
    {
        const int H=Bone(Side+TEXT("UpLeg")),K=Bone(Side+TEXT("Leg")),F=Bone(Side+TEXT("Foot"));
        if(H<0||K<0||F<0)return;
        const FVector Top=Pose[H].GetLocation()+FVector(0,0,8),Knee=Pose[K].GetLocation(),Foot=Pose[F].GetLocation()+FVector(0,0,2);
        const FVector Centers[]={Top,FMath::Lerp(Top,Knee,.45f),Knee,FMath::Lerp(Knee,Foot,.5f),Foot};
        // Radii in centimetres at hip, mid-thigh, knee, mid-calf and ankle. These were roughly twice
        // life size, which inflated the legs into a toy silhouette and pushed the trouser out through
        // the pads. A 185 cm athlete measures about this.
        const float Widths[]={10.4f,8.9f,7.1f,6.4f,5.5f};
        const int Base=Vertices.Num();constexpr int Sides=12;
        for(int Row=0;Row<5;++Row)
        {
            const FVector Along=(Centers[FMath::Min(4,Row+1)]-Centers[FMath::Max(0,Row-1)]).GetSafeNormal();
            const FVector Across=FVector::CrossProduct(Along,RigForward).GetSafeNormal();
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
    if(Uniform->GetNumSections()<2)
    {
        Uniform->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colors,Tangents,false);
        Uniform->CreateMeshSection_LinearColor(1,StripeV,StripeT,StripeN,StripeUV,Colors,Tangents,false);
    }
    else
    {
        Uniform->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,Tangents);
        Uniform->UpdateMeshSection_LinearColor(1,StripeV,StripeN,StripeUV,Colors,Tangents);
    }
}
void AC26Athlete::Animate(float Dt)
{
    if(Reference.IsEmpty())return;
    ActionTime+=Dt;MotionTime+=Dt;Pose=Reference;
    const bool Running=Action==EC26Action::Running;
    const bool Batting=Role==EC26Role::Batter;
    const bool Keeping=Role==EC26Role::Keeper;
    // Stride frequency follows the distance actually being covered, so feet stop skating.
    const float Cadence=FMath::Clamp(MoveSpeed/48.f,8.f,17.f);
    const float Gait=FMath::Sin(MotionTime*Cadence);
    const float Sway=FMath::Sin(MotionTime*1.7f);

    float Crouch=Keeping?-42.f:Batting?-23.f:-7.f;
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
        FL=Rig(Gait*47,-9,AnkleZ+FMath::Max(0.f,Gait)*24);
        FR=Rig(-Gait*47,9,AnkleZ+FMath::Max(0.f,-Gait)*24);
        LH=Rig(-Gait*38,-21,114);RH=Rig(Gait*38,21,114);
    }
    else if(Batting)
    {
        // Side-on: chest opened toward the off side with the head free to come back to the ball.
        TurnRight=46.f;LeanForward=22.f;LeanRight=4.f;
        FL=Rig(15.f+FootworkIntent*4.f,-1.f+StrideIntent*3.f,AnkleZ);
        FR=Rig(-13.f,9.f,AnkleZ);
    }
    else if(Keeping){FL=Rig(4,-21,AnkleZ);FR=Rig(4,21,AnkleZ);LH=Rig(33,-14,44);RH=Rig(33,14,44);}
    else if(Role==EC26Role::Umpire){FL=Rig(0,-13,AnkleZ);FR=Rig(0,13,AnkleZ);LH=Rig(1,-23,95);RH=Rig(1,23,95);}
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
                LeanForward=13.f+Swing*(Cross?2.f:11.f)-Follow*4.f;
                LeanRight=4.f+(Cross?-8.f:5.f)*Swing;
                if(Cross)FR=Rig(-14.f-FMath::Sin(T*PI)*13.f,11,AnkleZ);
                else FL=Rig(15.f+FMath::Sin(T*PI)*26.f,-4,AnkleZ);
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
        // Keep the handle inside the arms' reach before deriving the hands from it. The IK clamps
        // silently at full extension, so a follow-through that asked for more arm than the athlete
        // owns used to strand both hands short while the blade carried on to the target.
        const FVector Anchor=Rig(0,0,ShoulderZ+Crouch);
        const FVector Out=Grip-Anchor;
        if(const float Span=Out.Size();Span>ArmSpan*.95f)Grip=Anchor+Out/Span*(ArmSpan*.95f);
        // Both hands live on the handle: top hand high, bottom hand a fist below it.
        LH=Grip-Dir*4.f+Rig(0,-5.f,0);RH=Grip-Dir*14.f+Rig(0,5.f,0);
    }
    if(Batting&&Running){Grip=Rig(24,20,104);Dir=Rig(-.55f,.10f,.83f).GetSafeNormal();RH=Grip-Dir*14.f;}

    if(Action==EC26Action::Bowling)
    {
        // One continuous action: gather, brace the front foot, rotate the arm through the vertical
        // and follow through. The release notch lands with the hand at the top of the circle so the
        // ball genuinely leaves the fingers rather than appearing in front of the bowler.
        const float T=FMath::Clamp(ActionTime/.88f,0.f,1.f);
        const float A=FMath::DegreesToRadians(-132.f+T*372.f);
        RH=Rig(FMath::Sin(A)*60.f,20.f,152.f+FMath::Cos(A)*80.f);
        LH=Rig(-FMath::Sin(A)*44.f,-24.f,146.f-FMath::Cos(A)*46.f);
        FL=Rig(20.f+FMath::Sin(FMath::Clamp(T*2.2f,0.f,1.f)*PI)*32.f,-12,AnkleZ);
        FR=Rig(-26.f-T*22.f,14,AnkleZ+FMath::Max(0.f,FMath::Sin(T*PI*1.4f))*20.f);
        TurnRight=-26.f+T*44.f;LeanForward=6.f+T*26.f;LeanRight=-14.f+T*20.f;
        Crouch=-6.f-FMath::Sin(T*PI)*7.f;
    }
    if(Action==EC26Action::Catch||Action==EC26Action::Pickup)
    {
        FVector Take=ContactTarget.IsZero()?Rig(34,0,142):Mesh->GetComponentTransform().InverseTransformPosition(ContactTarget);
        if(Action==EC26Action::Pickup){Crouch=-46.f;LeanForward=38.f;Take.Z=FMath::Max(12.f,float(Take.Z));}
        else{LeanForward=8.f;if(Take.Z<80){Crouch=-34;LeanForward=28;}}
        LH=Take+Rig(0,-7,4);RH=Take+Rig(0,7,4);
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

    MoveBone(TEXT("Hips"),FVector(0,0,Crouch));
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
        if(F>=0){Pose[F].SetRotation(Reference[F].GetRotation());RebuildChildren(F);}
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
    UpdateUniform();
    PlaceKit(Grip,Dir,Batting,Running);
}
