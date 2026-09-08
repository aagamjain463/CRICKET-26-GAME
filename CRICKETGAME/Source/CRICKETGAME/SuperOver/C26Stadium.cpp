#include "C26Stadium.h"
#include "C26Types.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AC26Stadium::AC26Stadium()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("VenueRoot"));
    Bowl=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContinuousStadiumBowl"));Bowl->SetupAttachment(RootComponent);Bowl->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bowl->SetCastShadow(false);
    KeyLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FloodlightKey"));KeyLight->SetupAttachment(RootComponent);
    KeyLight->SetRelativeRotation(FRotator(-52,-34,0));KeyLight->SetIntensity(9.f);KeyLight->SetLightColor(FLinearColor(.86,.92,1));
    KeyLight->DynamicShadowDistanceMovableLight=14000;KeyLight->DynamicShadowCascades=3;
    FillLight=CreateDefaultSubobject<USkyLightComponent>(TEXT("StadiumFill"));FillLight->SetupAttachment(RootComponent);
    FillLight->SetIntensity(1.25f);FillLight->SetLightColor(FLinearColor(.46,.53,.66));FillLight->SetLowerHemisphereColor(FLinearColor(.05,.07,.09));
    Grade=CreateDefaultSubobject<UPostProcessComponent>(TEXT("BroadcastGrade"));Grade->SetupAttachment(RootComponent);Grade->bUnbound=true;
    auto& P=Grade->Settings;
    P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    P.bOverride_AutoExposureBias=true;P.AutoExposureBias=1.15f;
    P.bOverride_AutoExposureApplyPhysicalCameraExposure=true;P.AutoExposureApplyPhysicalCameraExposure=false;
    P.bOverride_BloomIntensity=true;P.BloomIntensity=.22f;
    P.bOverride_MotionBlurAmount=true;P.MotionBlurAmount=0;
    P.bOverride_VignetteIntensity=true;P.VignetteIntensity=.16f;
    P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(1.06,1.05,1.02,1);
    P.bOverride_ColorContrast=true;P.ColorContrast=FVector4(1.04,1.04,1.05,1);
    P.bOverride_ToneCurveAmount=true;P.ToneCurveAmount=.85f;
}
void AC26Stadium::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);BuildVenue();}
// Always rebuild in play. Dynamic material instances do not survive map serialization, so a venue
// restored from the package renders every tinted surface as its parent default.
void AC26Stadium::BeginPlay(){Super::BeginPlay();BuildVenue();FillLight->RecaptureSky();}
UHierarchicalInstancedStaticMeshComponent* AC26Stadium::Batch(const TCHAR* Name,UStaticMesh* Mesh,UMaterialInterface* Material)
{
    auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UHierarchicalInstancedStaticMeshComponent::StaticClass(),Name),RF_Transient);
    C->SetupAttachment(RootComponent);C->SetStaticMesh(Mesh);C->SetMaterial(0,Material);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->bAutoRebuildTreeOnInstanceChanges=false;
    C->RegisterComponent();AddInstanceComponent(C);Batches.Add(C);return C;
}
void AC26Stadium::BuildVenue()
{
    for(auto& C:Batches)if(C)C->DestroyComponent();Batches.Reset();
    for(auto& C:Signs)if(C)C->DestroyComponent();Signs.Reset();CrowdMaterials.Reset();
    Bowl->ClearAllMeshSections();
    auto Mat=[](const TCHAR* N){return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Cricket26/Materials/%s.%s"),N,N));};
    auto* Surface=Mat(TEXT("M_Surface"));if(!Surface)return;
    auto Tinted=[&](UMaterialInterface* Base,FLinearColor C,float Glow=0.f)->UMaterialInstanceDynamic*
    {if(!Base)return nullptr;auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Tint"),C);M->SetScalarParameterValue(TEXT("Glow"),Glow);return M;};
    auto Color=[&](FLinearColor C,float Glow=0.f){return Tinted(Surface,C,Glow);};
    auto* Concrete=Color(FLinearColor(.085,.108,.15));auto* Structure=Color(FLinearColor(.19,.23,.29));
    auto* Navy=Mat(TEXT("M_Navy"));auto* White=Mat(TEXT("M_White"));auto* Teal=Mat(TEXT("M_Teal"));
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Stadium/SM_CrowdVolume.SM_CrowdVolume"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Rails=Batch(TEXT("ArchitectureRails"),Cube,Structure);
    auto* Lines=Batch(TEXT("CreaseAndCircle"),Cube,White);
    auto* Seats=Batch(TEXT("IndividualSeats"),Cube,Color(FLinearColor(.023,.10,.18)));
    auto* Rope=Batch(TEXT("BoundaryRope"),Cylinder,White);
    LED=Color(FLinearColor(.015,.37,.41),1.5f);
    auto* Boards=Batch(TEXT("BoundaryLED"),Cube,LED);
    auto* Lamps=Batch(TEXT("FloodlightArrays"),Cube,Mat(TEXT("M_Light")));
    auto* Columns=Batch(TEXT("StadiumColumns"),Cylinder,Structure);
    auto Add=[](UHierarchicalInstancedStaticMeshComponent* B,FVector P,FVector S,FRotator R=FRotator::ZeroRotator){if(!B->GetStaticMesh())return;const FVector Size=B->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(1));B->AddInstance(FTransform(R,P,S/Size));};
    auto Beam=[&](UHierarchicalInstancedStaticMeshComponent* B,FVector A,FVector E,float Width)
    {Add(B,(A+E)*.5f,FVector(Width,Width,(E-A).Size()),FRotationMatrix::MakeFromZ(E-A).Rotator());};
    int Section=0;
    auto Ring=[&](float RX,float RY,float W,float Z,float Rise,UMaterialInterface* M)
    {
        TArray<FVector>V,N;TArray<int32>T;TArray<FVector2D>UV;TArray<FLinearColor>C;TArray<FProcMeshTangent>Tan;
        for(int I=0;I<=192;++I)
        {float A=2*PI*I/192.f;for(int J=0;J<2;++J){V.Add(FVector((RX+J*W)*FMath::Cos(A),(RY+J*W)*FMath::Sin(A),Z+J*Rise));N.Add(FVector(0,0,1));UV.Add(FVector2D(I/12.f,J));}}
        for(int I=0;I<192;++I){int K=I*2;T.Append({K,K+2,K+1,K+1,K+2,K+3});}
        Bowl->CreateMeshSection_LinearColor(Section,V,T,N,UV,C,Tan,false);Bowl->SetMaterial(Section++,M);
    };
    // Dense continuous rings: no radial stand-module gaps.
    Ring(7400,8050,3700,-35,0,Concrete);
    for(int Tier=0;Tier<2;++Tier)
    {
        const float RX=Tier?9750:7500,RY=Tier?10400:8150,Z=Tier?1310:180;
        const int Rows=Tier?22:23;
        for(int Row=0;Row<Rows;++Row)
        {
            const float R=Row*83.f,H=Z+Row*41.f;
            Ring(RX+R,RY+R,83,H,0,Concrete);Ring(RX+R,RY+R,0,H-41,41,Structure);
            const int Count=FMath::RoundToInt(2*PI*(RX+R)/66.f);
            for(int J=0;J<Count;++J)
            {
                const float A=2*PI*J/Count;
                if(J%41<3)continue;
                const FVector P((RX+R+40)*FMath::Cos(A),(RY+R+40)*FMath::Sin(A),H+25);
                if(Row%2==0)Add(Seats,P,FVector(44,45,10),FRotator(0,FMath::RadiansToDegrees(A)+90,0));
            }
        }
        Ring(RX,RY,0,Z+18,62,Navy);
        Ring(RX+Rows*83,RY+Rows*83,250,Z+Rows*41,0,Structure);
    }
    Ring(9500,10150,0,1120,150,Teal);Ring(9600,10250,280,1260,0,Structure);
    Ring(11800,12450,0,100,2600,Navy);
    Ring(10300,10950,1900,2810,-70,Structure);Ring(10300,10950,0,2720,90,Navy);
    // Outfield uses 26 mowing strips clipped to an ellipse, with soil microvariation.
    auto* Grass=Mat(TEXT("M_Grass"));
    Ring(0,0,7550,-8,0,Tinted(Grass,FLinearColor(.098,.232,.098)));
    for(int I=0;I<28;++I)
    {
        const float Y0=-7800+I*557.14f,Y1=Y0+557.14f;
        TArray<FVector>V,N;TArray<int32>T;TArray<FVector2D>UV;TArray<FLinearColor>C;TArray<FProcMeshTangent>Tan;
        for(int K=0;K<=10;++K)
        {
            float Y=FMath::Lerp(Y0,Y1,K/10.f),X=7200*FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(Y/7900)));
            for(int J=0;J<2;++J){V.Add(FVector(J?X:-X,Y,0));N.Add(FVector::UpVector);UV.Add(FVector2D(J,K/10.f));}
            if(K<10){int B=K*2;T.Append({B,B+1,B+2,B+1,B+3,B+2});}
        }
        auto* G=Tinted(Grass,I%2?FLinearColor(.118,.268,.113):FLinearColor(.093,.223,.094));
        Bowl->CreateMeshSection_LinearColor(Section,V,T,N,UV,C,Tan,false);Bowl->SetMaterial(Section++,G);
    }
    auto* Square=Batch(TEXT("WicketSquare"),Cube,Color(FLinearColor(.205,.219,.128)));
    Add(Square,FVector(0,0,1),FVector(1650,2650,2));
    auto* Pitch=Batch(TEXT("PreparedPitch"),Cube,Tinted(Mat(TEXT("M_Pitch")),FLinearColor(.315,.268,.183)));
    Add(Pitch,FVector(0,0,3),FVector(305,2400,4));
    // Adjacent worn practice strips sit within the prepared square.
    auto* Wear=Batch(TEXT("SquareWear"),Cube,Color(FLinearColor(.238,.226,.146)));
    Add(Wear,FVector(-400,0,2),FVector(285,2360,1));Add(Wear,FVector(400,0,2),FVector(285,2360,1));
    for(int End:{-1,1})
    {
        Add(Lines,FVector(0,End*C26Field::CreaseY,6.3f),FVector(366,5,1));
        Add(Lines,FVector(0,End*C26Field::WicketY,6.3f),FVector(264,5,1));
        for(int Side:{-1,1})Add(Lines,FVector(Side*132,End*1090,6.3f),FVector(5,412,1));
    }
    for(int I=0;I<128;++I)
    {
        FVector A=C26Field::RopePoint(I*2*PI/128),B=C26Field::RopePoint((I+1)*2*PI/128);Beam(Rope,A,B,8);
        if(I%2==0){float Ang=2*PI*I/128;FVector P(6750*FMath::Cos(Ang),7420*FMath::Sin(Ang),47);Add(Boards,P,FVector(315,16,75),FRotator(0,FMath::RadiansToDegrees(Ang)+90,0));}
        if(I%4==0){const float A0=2*PI*I/128,A1=A0+.028f;Beam(Lines,FVector(2740*FMath::Cos(A0),2740*FMath::Sin(A0),6),FVector(2740*FMath::Cos(A1),2740*FMath::Sin(A1),6),3);}
    }
    // Instanced spectators: shaded torsos and heads, distributed over both tiers.
    FRandomStream Random(2626);
    for(int Group=0;Group<6;++Group)
    {
        const FLinearColor Colors[]={FLinearColor(.06,.25,.29),FLinearColor(.56,.07,.04),FLinearColor(.62,.59,.47),FLinearColor(.12,.16,.24),FLinearColor(.3,.32,.40),FLinearColor(.39,.26,.17)};
        auto* M=UMaterialInstanceDynamic::Create(Mat(TEXT("M_Crowd")),this);M->SetVectorParameterValue(TEXT("Tint"),Colors[Group]);CrowdMaterials.Add(M);
        auto* Bodies=Batch(*FString::Printf(TEXT("CrowdGroup%d"),Group),Sphere,M);
        auto* Heads=Batch(*FString::Printf(TEXT("CrowdHeads%d"),Group),Sphere,Color(FLinearColor(.44+.035f*Group,.25+.03f*Group,.15+.025f*Group),.15f));
        for(int Tier=0;Tier<2;++Tier)for(int Row=0;Row<(Tier?22:23);++Row)
        {
            float RX=(Tier?9750:7500)+Row*83,RY=(Tier?10400:8150)+Row*83,H=(Tier?1310:180)+Row*41;
            const int Count=FMath::RoundToInt(2*PI*RX/78.f);
            for(int J=Group;J<Count;J+=6)
            {
                if(J%41<3||Random.FRand()<.035f)continue;
                const float A=2*PI*J/Count;
                const FVector P((RX+40)*FMath::Cos(A),(RY+40)*FMath::Sin(A),H+55);
                Add(Bodies,P,FVector(34,25,50),FRotator(0,FMath::RadiansToDegrees(A),0));
                Add(Heads,P+FVector(0,0,32),FVector(21,20,23));
            }
        }
    }
    for(int I=0;I<32;++I)
    {
        const float A=2*PI*I/32;
        Beam(Columns,FVector(11800*FMath::Cos(A),12450*FMath::Sin(A),0),FVector(11800*FMath::Cos(A),12450*FMath::Sin(A),2750),65);
        Beam(Rails,FVector(10280*FMath::Cos(A),10930*FMath::Sin(A),2800),FVector(12100*FMath::Cos(A),12750*FMath::Sin(A),2800),18);
    }
    for(int I=0;I<8;++I)
    {
        const float A=2*PI*(I+.5f)/8;const FVector P(9900*FMath::Cos(A),10550*FMath::Sin(A),0);
        Beam(Columns,P,P+FVector(0,0,3700),82);
        const FRotator R(0,FMath::RadiansToDegrees(A)+90,0);
        Add(Rails,P+FVector(0,0,3750),FVector(690,75,380),R);
        for(int Row=0;Row<4;++Row)for(int Col=0;Col<8;++Col)
        {FVector Offset=R.RotateVector(FVector((Col-3.5f)*78,-44,3625+Row*78));Add(Lamps,P+Offset,FVector(64,15,60),R);}
    }
    // Original venue signage and two broadcast screens.
    for(int I=0;I<12;++I)
    {
        const float A=2*PI*I/12;
        auto* Text=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);Text->SetupAttachment(RootComponent);Text->RegisterComponent();AddInstanceComponent(Text);Signs.Add(Text);
        Text->SetText(FText::FromString(I%2?TEXT("ECLIPSE OVAL"):TEXT("CRICKET 26  /  SUPER OVER")));
        Text->SetWorldSize(110);Text->SetHorizontalAlignment(EHTA_Center);Text->SetTextRenderColor(FColor(201,233,232));
        Text->SetRelativeLocation(FVector(9540*FMath::Cos(A),10190*FMath::Sin(A),1185));Text->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(A)+180,0));
    }
    for(int I:{-1,1})
    {
        Add(Boards,FVector(0,I*11600,2200),FVector(2100,40,850));
        auto* Text=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);Text->SetupAttachment(RootComponent);Text->RegisterComponent();AddInstanceComponent(Text);Signs.Add(Text);
        Text->SetText(FText::FromString(TEXT("CRICKET 26\nSUPER OVER")));Text->SetWorldSize(240);Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetRelativeLocation(FVector(0,I*11560,2400));Text->SetRelativeRotation(FRotator(0,I>0?-90:90,0));
    }
    for(auto& B:Batches){B->bAutoRebuildTreeOnInstanceChanges=true;B->BuildTreeIfOutdated(true,true);}
    UE_LOG(LogC26,Log,TEXT("Eclipse Oval built: %d instanced batches, %d sections."),Batches.Num(),Section);
}
void AC26Stadium::SetQuality(int Level)
{
    CrowdQuality=FMath::Clamp(Level,0,3);
    KeyLight->SetCastShadows(Level>0);KeyLight->DynamicShadowCascades=Level<2?2:3;
    KeyLight->DynamicShadowDistanceMovableLight=Level<2?6500:14000;
    for(auto& B:Batches)
    {
        if(B->GetName().StartsWith(TEXT("CrowdHeads")))B->SetVisibility(Level>0);
        B->SetCullDistances(0,Level==0?23000:32000);
    }
}
void AC26Stadium::React(float Intensity){CrowdReaction=Intensity;}
void AC26Stadium::UpdateAtmosphere(float Time)
{
    CrowdReaction=FMath::Max(0.f,CrowdReaction-.007f);
    if(LED)LED->SetScalarParameterValue(TEXT("Glow"),1.2f+CrowdReaction*FMath::Max(0.f,FMath::Sin(Time*3))*.8f);
    // Celebration glow through the standard Glow param (M_Crowd has no WPO chain).
    for(auto M:CrowdMaterials)if(M)M->SetScalarParameterValue(TEXT("Glow"),.15f+CrowdReaction*1.4f);
}
