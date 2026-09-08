#include "C26Athlete.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Player(TEXT("/Game/Cricket26/Characters/SK_Cricketer.SK_Cricketer"));
    if(Player.Succeeded())Mesh->SetSkinnedAssetAndUpdate(Player.Object);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(true);
    Bat=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WillowBat"));Bat->SetupAttachment(RootComponent);
    Bat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Grill=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HelmetGrill"));Grill->SetupAttachment(RootComponent);
    Grill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Helmet=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Helmet"));Helmet->SetupAttachment(RootComponent);
    PadL=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPad"));PadL->SetupAttachment(RootComponent);
    PadR=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPad"));PadR->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for(auto* P:{Helmet.Get(),PadL.Get(),PadR.Get()}){P->SetStaticMesh(Sphere.Object);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
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
        // Mixamo Remy imports at ~378 cm; the venue is real-world scale. Shrink the
        // reference pose about the ground origin to ~182 cm so every authored IK
        // target, equipment size and contact point (tuned for a real-scale rig)
        // lines up. Rotations are untouched; skinning follows the bones.
        for(auto& T:Reference)T.SetLocation(T.GetLocation()*0.48f);
        Pose=Reference;
        static bool Once=false;
        if(!Once)
        {
            Once=true;
            UE_LOG(LogC26,Display,TEXT("C26_RIG bones=%d mesh=%s"),R.GetNum(),*S->GetName());
            for(const TCHAR* N:{TEXT("Hips"),TEXT("Spine2"),TEXT("Head"),TEXT("Neck"),TEXT("LeftHand"),TEXT("RightHand"),
                TEXT("LeftFoot"),TEXT("RightFoot"),TEXT("LeftArm"),TEXT("RightArm"),TEXT("LeftUpLeg"),TEXT("LeftToeBase")})
            {
                const int I=Bone(N);
                if(I<0){UE_LOG(LogC26,Display,TEXT("C26_RIG %s MISSING"),N);continue;}
                const FVector L=Reference[I].GetLocation();
                UE_LOG(LogC26,Display,TEXT("C26_RIG %s = %.1f, %.1f, %.1f"),N,L.X,L.Y,L.Z);
            }
            FString All;for(int I=0;I<R.GetNum();++I){All+=R.GetBoneName(I).ToString();All+=TEXT(" ");}
            UE_LOG(LogC26,Display,TEXT("C26_RIG_NAMES %s"),*All);
        }
    }
    BuildEquipment();
}
void AC26Athlete::BuildEquipment()
{
    // Shaped willow blade: tapered shoulders, rounded toe, a raised spine and integrated grip.
    TArray<FVector> V;TArray<int32>T;TArray<FVector>N;TArray<FVector2D>UV;TArray<FLinearColor>C;TArray<FProcMeshTangent>Tan;
    const float Z[]={0,-12,-22,-30,-70,-80,-83};const float W[]={1.35f,1.35f,2.f,5.3f,5.4f,4.8f,2.8f};
    for(int R=0;R<7;++R)for(int J=0;J<8;++J)
    {const float A=2*PI*J/8;V.Add(FVector(FMath::Cos(A)*W[R],FMath::Sin(A)*(R<2?1.35f:2.1f),Z[R]));N.Add(FVector(FMath::Cos(A),FMath::Sin(A),0));UV.Add(FVector2D(J/8.f,R/6.f));}
    for(int R=0;R<6;++R)for(int J=0;J<8;++J){const int A=R*8+J,B=R*8+(J+1)%8;T.Append({A,B,A+8,B,B+8,A+8});}
    Bat->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
    // Lift the willow with a small emissive boost so it reads as wood under floodlights.
    if(auto* WillowBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Willow.M_Willow")))
    {auto* WillowMID=UMaterialInstanceDynamic::Create(WillowBase,this);WillowMID->SetScalarParameterValue(TEXT("Glow"),.38f);Bat->SetMaterial(0,WillowMID);}
    auto* White=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_White.M_White"));
    PadL->SetMaterial(0,White);PadR->SetMaterial(0,White);
    PadL->SetRelativeScale3D(FVector(.12,.24,.49));PadR->SetRelativeScale3D(FVector(.12,.24,.49));
    // Helmet shell must fully enclose the skull: the head bone sits at head centre,
    // so a head-height shell centred only +9 cm stays buried inside the hair.
    Helmet->SetRelativeScale3D(FVector(.40,.36,.34));
    // Four slim steel bars across the face, merged in one component.
    V.Reset();T.Reset();N.Reset();UV.Reset();
    for(int Bar=0;Bar<4;++Bar)
    {
        const int K=V.Num();float H=-3.f-Bar*3.2f;
        V.Append({FVector(12,-12,H),FVector(13,-12,H+.7f),FVector(13,12,H+.7f),FVector(12,12,H)});
        T.Append({K,K+1,K+2,K,K+2,K+3});
        for(int J=0;J<4;++J){N.Add(FVector(1,0,0));UV.Add(FVector2D::ZeroVector);}
    }
    Grill->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);Grill->SetMaterial(0,White);
}
void AC26Athlete::Configure(EC26Role NewRole,int Team,int Number)
{
    Role=NewRole;TeamId=Team;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Surface.M_Surface"));
    Kit=UMaterialInstanceDynamic::Create(Base,this);
    const FLinearColor Color=Role==EC26Role::Umpire?FLinearColor(.62,.018,.06):Team==0?FLinearColor(.008,.26,.30):FLinearColor(.78,.07,.035);
    Kit->SetVectorParameterValue(TEXT("Tint"),Color);
    Kit->SetScalarParameterValue(TEXT("Roughness"),.86f);
    if(auto* S=Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
    {
        for(int I=0;I<S->GetMaterials().Num();++I)
        {
            const FString Name=S->GetMaterials()[I].MaterialSlotName.ToString();
            if(Name.Contains(TEXT("Top"))||Name.Contains(TEXT("Bottom")))Mesh->SetMaterial(I,Kit);
        }
    }
    Helmet->SetMaterial(0,Kit);
    const bool Pads=Role==EC26Role::Batter||Role==EC26Role::Keeper;
    Bat->SetVisibility(Role==EC26Role::Batter);Helmet->SetVisibility(Pads);PadL->SetVisibility(Pads);PadR->SetVisibility(Pads);Grill->SetVisibility(Pads);
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
{SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));MotionTime=0;SetAction(EC26Action::Ready);Animate(0);}
void AC26Athlete::SetShotContact(const FVector& Target,float Angle,bool bLoft)
{ContactTarget=Target;ShotAngle=Angle;Loft=bLoft;SetAction(EC26Action::Batting);}
FVector AC26Athlete::HandPosition() const
{int I=Bone(TEXT("RightHand"));return I>=0&&Pose.IsValidIndex(I)?Mesh->GetComponentTransform().TransformPosition(Pose[I].GetLocation()):GetActorLocation()+FVector(0,0,210);}
void AC26Athlete::Animate(float Dt)
{
    if(Reference.IsEmpty())return;
    ActionTime+=Dt;MotionTime+=Dt;Pose=Reference;
    const bool Running=Action==EC26Action::Running;
    const bool Batting=Role==EC26Role::Batter;
    const bool Keeping=Role==EC26Role::Keeper;
    const float Gait=FMath::Sin(MotionTime*12.f);
    float Crouch=Keeping?-38.f:Batting?-11.f:-5.f;
    if(Running)Crouch=-4.f+3*FMath::Abs(Gait);
    if(Action==EC26Action::Pickup)Crouch=-44*FMath::Sin(FMath::Clamp(ActionTime/.55f,0.f,1.f)*PI);
    MoveBone(TEXT("Hips"),FVector(0,0,Crouch));
    const int FootL=Bone(TEXT("LeftFoot")),FootR=Bone(TEXT("RightFoot"));
    FVector FL=Reference[FootL>=0?FootL:0].GetLocation(),FR=Reference[FootR>=0?FootR:0].GetLocation();
    if(Running){FL+=FVector(Gait*43,0,FMath::Max(0.f,Gait)*23);FR+=FVector(-Gait*43,0,FMath::Max(0.f,-Gait)*23);}
    else{FL.Y-=Batting?9:Keeping?18:6;FR.Y+=Batting?9:Keeping?18:6;}
    FVector LH(28,-24,100+Crouch),RH(28,24,100+Crouch);
    if(Running){LH=FVector(-Gait*36,-23,116);RH=FVector(Gait*36,23,116);}
    FVector Grip(34,0,83),Blade(39,0,8);
    if(Batting&&!Running)
    {
        LH=Grip+FVector(0,-4,7);RH=Grip+FVector(0,4,-3);
        if(Action==EC26Action::Batting)
        {
            const FVector Contact=GetActorTransform().InverseTransformPosition(ContactTarget);
            const float T=FMath::Clamp(ActionTime/.65f,0.f,1.f);
            const float Swing=FMath::SmoothStep(0.f,1.f,FMath::Clamp(T/.38f,0.f,1.f));
            const float Follow=FMath::SmoothStep(0.f,1.f,FMath::Clamp((T-.38f)/.62f,0.f,1.f));
            const FVector Back(-23,34,150);
            Blade=FMath::Lerp(Back,Contact,Swing);
            Blade=FMath::Lerp(Blade,FVector(95,FMath::Sin(FMath::DegreesToRadians(ShotAngle))*55,Loft?180:134),Follow);
            const FVector Axis=FMath::Lerp(FVector(-.2f,.1f,.98f),FVector(-.7f,0,.7f),Follow).GetSafeNormal();
            Grip=Blade+Axis*57.f;
            LH=Grip+FVector(0,-3,6);RH=Grip+FVector(0,3,-4);
            FL.X+=FMath::Sin(T*PI)*31;
        }
    }
    if(Action==EC26Action::Bowling)
    {
        const float T=FMath::Clamp(ActionTime/.85f,0.f,1.f);
        const float A=FMath::DegreesToRadians(-110+T*340);
        RH=FVector(FMath::Sin(A)*55,23,147+FMath::Cos(A)*70);
        LH=FVector(-FMath::Sin(A)*45,-25,139-FMath::Cos(A)*40);
        FL.X+=33*FMath::Sin(T*PI);FR.X-=24*FMath::Sin(T*PI);
    }
    if(Keeping&&Action==EC26Action::Ready){LH=FVector(42,-14,43);RH=FVector(42,14,43);}
    if(Action==EC26Action::Catch){LH=FVector(40,-8,145);RH=FVector(40,8,145);}
    if(Action==EC26Action::Throw){RH=FVector(32+FMath::Sin(ActionTime*8)*40,25,160+FMath::Cos(ActionTime*8)*28);}
    if(Action==EC26Action::Celebrate||Action==EC26Action::SignalSix){LH=FVector(2,-24,208);RH=FVector(2,24,208);}
    if(Action==EC26Action::SignalOut){RH=FVector(0,22,210);}
    if(Action==EC26Action::SignalFour||Action==EC26Action::SignalWide){LH=FVector(0,-80,143);RH=FVector(0,80,143);}
    Limb(TEXT("LeftUpLeg"),TEXT("LeftLeg"),TEXT("LeftFoot"),FL,FVector(1,0,0));
    Limb(TEXT("RightUpLeg"),TEXT("RightLeg"),TEXT("RightFoot"),FR,FVector(1,0,0));
    Limb(TEXT("LeftArm"),TEXT("LeftForeArm"),TEXT("LeftHand"),LH,FVector(0,-1,-.5f));
    Limb(TEXT("RightArm"),TEXT("RightForeArm"),TEXT("RightHand"),RH,FVector(0,1,-.5f));
    Mesh->ApplyComponentPose(Pose);
    if(Batting)
    {
        if(Running){Grip=RH;Blade=Grip+FVector(-25,0,-65);}
        const FVector Axis=(Grip-Blade).GetSafeNormal();
        Bat->SetRelativeLocation(Grip);Bat->SetRelativeRotation(FRotationMatrix::MakeFromZ(Axis).Rotator());
    }
    const int Head=Bone(TEXT("Head"));if(Head>=0){FVector H=Pose[Head].GetLocation()+FVector(0,0,18);Helmet->SetRelativeLocation(H);Grill->SetRelativeLocation(H);}
    auto PlacePad=[this](UStaticMeshComponent* Pad,const FString& Leg,const FString& Foot)
    {int L=Bone(Leg),F=Bone(Foot);if(L<0||F<0)return;Pad->SetRelativeLocation((Pose[L].GetLocation()+Pose[F].GetLocation())*.5f+FVector(6,0,1));Pad->SetRelativeRotation(FRotationMatrix::MakeFromZ(Pose[L].GetLocation()-Pose[F].GetLocation()).Rotator());};
    PlacePad(PadL,TEXT("LeftLeg"),TEXT("LeftFoot"));PlacePad(PadR,TEXT("RightLeg"),TEXT("RightFoot"));
}
