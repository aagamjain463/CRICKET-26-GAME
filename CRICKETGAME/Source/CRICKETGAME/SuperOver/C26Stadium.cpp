#include "C26Stadium.h"
#include "C26Types.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogC26Venue,Log,All);

namespace
{
// Eclipse Oval, in centimetres. The rope radii live in C26Field and are authoritative for
// gameplay; everything architectural is derived from them so the bowl can never drift out of
// proportion with the playing area.
constexpr float TurfRX=8250.f,TurfRY=8950.f;      // Grass runs well past the rope to the wall.
constexpr float BoardRX=7050.f,BoardRY=7720.f;    // Perimeter advertising ring.
constexpr float WallRX=7350.f,WallRY=8050.f;      // Face of the lower stand.
constexpr float WallTop=345.f;
constexpr int   LowerRows=22;
constexpr float LowerDepth=94.f,LowerRise=47.f;
constexpr float UpperRX=9760.f,UpperRY=10480.f,UpperZ=1660.f;
constexpr int   UpperRows=18;
constexpr float UpperDepth=98.f,UpperRise=54.f;
constexpr float RoofInnerZ=2980.f,RoofOuterZ=3520.f;
constexpr float RoofOuterRX=11950.f,RoofOuterRY=12700.f;
constexpr int   Segments=176;                     // Bowl tessellation around the full ring.
constexpr float FloodCandelas=26000.f;            // Per-pylon spot output, in candelas.

FVector Oval(float RX,float RY,float Angle,float Z){return FVector(RX*FMath::Cos(Angle),RY*FMath::Sin(Angle),Z);}

/** One material's worth of accumulated geometry.
    Quad() always emits the winding that makes the shading normal face Want. The previous venue
    wound its outfield the opposite way from its stands, and because every generated material is
    two-sided the renderer flipped the ground normal to face down: the whole field was lit only by
    the sky's lower hemisphere, which is why the grass read as flat near-black next to a blown-out
    pitch. Facing is no longer left to the order the corners happen to be written in. */
struct FC26Surface
{
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;
    TArray<FLinearColor> C;TArray<FProcMeshTangent> Tan;
    void Quad(const FVector& A,const FVector& B,const FVector& D,const FVector& E,const FVector& Want,float US=1.f,float VS=1.f)
    {
        const FVector Face=FVector::CrossProduct(B-D,A-D);
        const int32 Base=V.Num();
        if(FVector::DotProduct(Face,Want)<0.f)V.Append({E,D,B,A});else V.Append({A,B,D,E});
        UV.Append({FVector2D(0,0),FVector2D(US,0),FVector2D(US,VS),FVector2D(0,VS)});
        for(int I=0;I<4;++I)N.Add(Want);
        T.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
    }
    /** Flat horizontal strip clipped to the turf ellipse, used for mowing bands. */
    void Band(float Y0,float Y1,float RX,float RY,float Z,int Steps=14)
    {
        auto Edge=[&](float Y){return RX*FMath::Sqrt(FMath::Max(0.f,1.f-FMath::Square(Y/RY)));};
        for(int I=0;I<Steps;++I)
        {
            const float A=FMath::Lerp(Y0,Y1,I/float(Steps)),B=FMath::Lerp(Y0,Y1,(I+1)/float(Steps));
            const float EA=Edge(A),EB=Edge(B);
            if(EA<1.f&&EB<1.f)continue;
            Quad(FVector(-EA,A,Z),FVector(EA,A,Z),FVector(EB,B,Z),FVector(-EB,B,Z),FVector::UpVector,8.f,1.f);
        }
    }
    void Plate(float X0,float Y0,float X1,float Y1,float Z,float US=1.f,float VS=1.f)
    {Quad(FVector(X0,Y0,Z),FVector(X1,Y0,Z),FVector(X1,Y1,Z),FVector(X0,Y1,Z),FVector::UpVector,US,VS);}
    /** Ring of quads swept between two ellipse profiles; the normal is derived per segment. */
    void Sweep(float RX0,float RY0,float Z0,float RX1,float RY1,float Z1,bool Up,int Steps=Segments,int Skip=-1,int SkipWidth=0)
    {
        for(int I=0;I<Steps;++I)
        {
            if(Skip>0&&(I%Skip)<SkipWidth)continue;
            const float A=2*PI*I/Steps,B=2*PI*(I+1)/Steps;
            const FVector P0=Oval(RX0,RY0,A,Z0),P1=Oval(RX0,RY0,B,Z0);
            const FVector Q0=Oval(RX1,RY1,A,Z1),Q1=Oval(RX1,RY1,B,Z1);
            FVector Want=Up?FVector::UpVector:-FVector(FMath::Cos(A)/FMath::Max(1.f,RX0),FMath::Sin(A)/FMath::Max(1.f,RY0),0).GetSafeNormal();
            if(!Up&&Want.IsNearlyZero())Want=FVector(-FMath::Cos(A),-FMath::Sin(A),0);
            Quad(P0,P1,Q1,Q0,Want,3.f,1.f);
        }
    }
    void Box(const FVector& Centre,const FVector& Extent,float Yaw)
    {
        const FRotator R(0,Yaw,0);
        const FVector X=R.RotateVector(FVector(Extent.X,0,0)),Y=R.RotateVector(FVector(0,Extent.Y,0)),Z(0,0,Extent.Z);
        auto P=[&](float A,float B,float Cc){return Centre+X*A+Y*B+Z*Cc;};
        Quad(P(-1,-1,1),P(1,-1,1),P(1,1,1),P(-1,1,1),FVector::UpVector);
        Quad(P(-1,-1,-1),P(1,-1,-1),P(1,-1,1),P(-1,-1,1),-Y.GetSafeNormal());
        Quad(P(-1,1,-1),P(1,1,-1),P(1,1,1),P(-1,1,1),Y.GetSafeNormal());
        Quad(P(-1,-1,-1),P(-1,1,-1),P(-1,1,1),P(-1,-1,1),-X.GetSafeNormal());
        Quad(P(1,-1,-1),P(1,1,-1),P(1,1,1),P(1,-1,1),X.GetSafeNormal());
    }
    bool IsEmpty() const{return T.Num()==0;}
};
}

AC26Stadium::AC26Stadium()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("VenueRoot"));
    // Serialized native subobject identity must stay stable. Renaming this orphaned the map's
    // Bowl reference into TRASH_ProceduralMeshComponent while building an invisible venue.
    Bowl=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContinuousStadiumBowl"));Bowl->SetupAttachment(RootComponent);
    Bowl->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bowl->SetCastShadow(false);
    Sky=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NightSky"));Sky->SetupAttachment(RootComponent);
    Sky->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sky->SetCastShadow(false);
    Sky->bReceivesDecals=false;
    // One shadow-casting bank plus an unshadowed cross fill. Explicit forward-shading priorities
    // stop the renderer warning about two directional lights competing for the single slot.
    KeyLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FloodlightKey"));KeyLight->SetupAttachment(RootComponent);
    KeyLight->SetRelativeRotation(FRotator(-58,-38,0));KeyLight->SetIntensity(2.35f);KeyLight->SetLightColor(FLinearColor(.88,.93,1));
    KeyLight->DynamicShadowDistanceMovableLight=9000;KeyLight->DynamicShadowCascades=3;KeyLight->ForwardShadingPriority=1;
    KeyLight->SetSpecularScale(.85f);
    CrossLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FloodlightCross"));CrossLight->SetupAttachment(RootComponent);
    CrossLight->SetRelativeRotation(FRotator(-51,132,0));CrossLight->SetIntensity(1.05f);CrossLight->SetLightColor(FLinearColor(.80,.87,1));
    CrossLight->SetCastShadows(false);CrossLight->ForwardShadingPriority=0;
    FillLight=CreateDefaultSubobject<USkyLightComponent>(TEXT("StadiumFill"));FillLight->SetupAttachment(RootComponent);
    FillLight->SetIntensity(.42f);FillLight->SetLightColor(FLinearColor(.40,.50,.66));FillLight->SetLowerHemisphereColor(FLinearColor(.020,.030,.042));
    FillLight->bLowerHemisphereIsBlack=false;
    Haze=CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("StadiumHaze"));Haze->SetupAttachment(RootComponent);
    Haze->SetFogDensity(.000042f);Haze->SetFogHeightFalloff(.09f);Haze->SetFogInscatteringColor(FLinearColor(.055,.086,.145));
    Haze->SetFogMaxOpacity(.62f);Haze->SetStartDistance(2600.f);
    Grade=CreateDefaultSubobject<UPostProcessComponent>(TEXT("BroadcastGrade"));Grade->SetupAttachment(RootComponent);Grade->bUnbound=true;
    auto& P=Grade->Settings;
    P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    P.bOverride_AutoExposureBias=true;P.AutoExposureBias=1.15f;
    P.bOverride_AutoExposureApplyPhysicalCameraExposure=true;P.AutoExposureApplyPhysicalCameraExposure=false;
    // Broadcast grade: restrained bloom around the floodlights, no grain, no chromatic aberration,
    // clarity preserved for the ball. Motion blur stays off during play.
    P.bOverride_BloomMethod=true;P.BloomMethod=EBloomMethod::BM_SOG;
    P.bOverride_BloomIntensity=true;P.BloomIntensity=.30f;
    P.bOverride_BloomThreshold=true;P.BloomThreshold=1.55f;
    P.bOverride_MotionBlurAmount=true;P.MotionBlurAmount=0;
    P.bOverride_SceneFringeIntensity=true;P.SceneFringeIntensity=0;
    P.bOverride_FilmGrainIntensity=true;P.FilmGrainIntensity=0;
    P.bOverride_VignetteIntensity=true;P.VignetteIntensity=.22f;
    P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(1.05,1.05,1.02,1);
    P.bOverride_ColorContrast=true;P.ColorContrast=FVector4(1.06,1.06,1.08,1);
    P.bOverride_ColorGamma=true;P.ColorGamma=FVector4(1,1,1.01,1);
    P.bOverride_ToneCurveAmount=true;P.ToneCurveAmount=.92f;
    P.bOverride_AmbientOcclusionIntensity=true;P.AmbientOcclusionIntensity=.42f;
    P.bOverride_AmbientOcclusionRadius=true;P.AmbientOcclusionRadius=120.f;
    P.bOverride_DepthOfFieldFocalDistance=true;P.DepthOfFieldFocalDistance=0;
    for(int I=0;I<4;++I)
    {
        auto* S=CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("PylonFlood%d"),I));
        S->SetupAttachment(RootComponent);S->SetCastShadows(false);S->SetIntensityUnits(ELightUnits::Candelas);
        // Calibrated against the 2.35 lux key bank: each pylon adds a soft directional lift over
        // the square without ever becoming the dominant source. Candela falloff over 100 m is
        // steep, so this reads as shaping rather than a second sun.
        S->SetIntensity(FloodCandelas);S->SetAttenuationRadius(26000.f);S->SetInnerConeAngle(13.f);S->SetOuterConeAngle(30.f);
        S->SetLightColor(FLinearColor(.90,.95,1));S->SetVolumetricScatteringIntensity(.08f);
        S->SetVisibility(false);
        const float A=PI/6.f+I*PI/2.f;
        const FVector Head=Oval(10250,10980,A,4250);
        S->SetRelativeLocation(Head);S->SetRelativeRotation((FVector(0,0,120)-Head).Rotation());
        Floods.Add(S);
    }
}
void AC26Stadium::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);BuildVenue();}
// Always rebuild in play. Dynamic material instances do not survive map serialization, so a venue
// restored from the package renders every tinted surface as its parent default.
void AC26Stadium::BeginPlay(){Super::BeginPlay();ConfigureLighting();BuildVenue();FillLight->RecaptureSky();}
void AC26Stadium::ConfigureLighting()
{
    // Apply the authored setup after map deserialization: saved component overrides from the
    // prototype otherwise restore its 9-lux key and 1.25-strength blue sky over new C++ defaults.
    KeyLight->SetRelativeRotation(FRotator(-58,-38,0));KeyLight->SetIntensity(2.35f);
    KeyLight->SetLightColor(FLinearColor(.96,.97,1));KeyLight->SetSpecularScale(.6f);
    CrossLight->SetIntensity(1.05f);CrossLight->SetLightColor(FLinearColor(.87,.93,1));
    FillLight->SetIntensity(.30f);FillLight->SetLightColor(FLinearColor(.70,.77,.86));
    Haze->SetFogDensity(.000018f);Haze->SetFogMaxOpacity(.28f);Haze->SetStartDistance(5000.f);
    Grade->Settings.AutoExposureBias=-.25f;Grade->Settings.VignetteIntensity=.12f;
}
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
    Bowl->ClearAllMeshSections();Sky->ClearAllMeshSections();
    auto Mat=[](const TCHAR* N){return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Cricket26/Materials/%s.%s"),N,N));};
    auto* Surface=Mat(TEXT("M_Surface"));if(!Surface)return;
    auto* GrassBase=Mat(TEXT("M_Grass"));auto* PitchBase=Mat(TEXT("M_Pitch"));
    auto* Navy=Mat(TEXT("M_Navy"));auto* White=Mat(TEXT("M_White"));auto* VenueTeal=Mat(TEXT("M_Teal"));
    auto Tinted=[&](UMaterialInterface* Base,FLinearColor C,float Glow=0.f,float Rough=-1.f)->UMaterialInstanceDynamic*
    {
        if(!Base)return nullptr;auto* M=UMaterialInstanceDynamic::Create(Base,this);
        M->SetVectorParameterValue(TEXT("Tint"),C);M->SetScalarParameterValue(TEXT("Glow"),Glow);
        if(Rough>=0)M->SetScalarParameterValue(TEXT("Roughness"),Rough);return M;
    };
    auto Colour=[&](FLinearColor C,float Glow=0.f,float Rough=-1.f){return Tinted(Surface,C,Glow,Rough);};

    // ---- Broadcast palette. Field bright, architecture mid, crowd deliberately darker so the
    // ---- athletes and the ball stay the brightest readable things on screen.
    // M_Grass and M_Pitch carry a procedural noise chain that never resolves on this renderer, so
    // anything drawn with them falls back to the default grey material. That fallback -- not the
    // authored colours -- is what produced the flat outfield and the blown-out white pitch. The
    // ground is drawn with the plain surface material instead and takes its variation from real
    // geometry: mowing bands, a watered collar, five prepared strips and worn landing areas.
    if(!GrassBase||!PitchBase)UE_LOG(LogC26Venue,Warning,TEXT("Turf detail materials unavailable; using surface fallback."));
    const FLinearColor MowTone[6]={
        FLinearColor(.0430,.1180,.0450),FLinearColor(.0600,.1620,.0575),
        FLinearColor(.0465,.1265,.0480),FLinearColor(.0565,.1530,.0545),
        FLinearColor(.0410,.1105,.0430),FLinearColor(.0625,.1690,.0600)};
    TArray<UMaterialInstanceDynamic*> Mow;
    for(int I=0;I<6;++I)Mow.Add(Tinted(GrassBase?GrassBase:Surface,MowTone[I],0,.95f));
    auto* MowRing   = Colour(FLinearColor(.0330,.0900,.0350),0,.96f);
    auto* Apron     = Colour(FLinearColor(.0700,.0870,.0470),0,.95f);
    auto* SquareMat = Tinted(GrassBase?GrassBase:Surface,FLinearColor(.075,.125,.040),0,.94f);
    auto* PitchMat  = Tinted(PitchBase?PitchBase:Surface,FLinearColor(.270,.220,.135),0,.92f);
    auto* WornMat   = Tinted(PitchBase?PitchBase:Surface,FLinearColor(.235,.190,.115),0,.94f);
    auto* RestingMat= Tinted(GrassBase?GrassBase:Surface,FLinearColor(.067,.110,.039),0,.96f);
    auto* Paint     = Colour(FLinearColor(.3800,.3950,.3750),0,.72f);
    auto* Concrete  = Colour(FLinearColor(.0480,.0560,.0700),0,.88f);
    auto* Steel     = Colour(FLinearColor(.0950,.1080,.1300),0,.55f);
    auto* Facade    = Colour(FLinearColor(.0250,.0330,.0470),0,.72f);
    auto* RoofUnder = Colour(FLinearColor(.0300,.0355,.0450),0,.80f);
    auto* RoofTop   = Colour(FLinearColor(.0760,.0850,.0980),0,.62f);
    auto* Glazing   = Colour(FLinearColor(.0140,.0470,.0620),.55f,.22f);
    auto* Screen    = Colour(FLinearColor(.009,.013,.019),0.f,.94f);

    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Person=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Cricket26/Stadium/SM_CrowdVolume.SM_CrowdVolume"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Rails=Batch(TEXT("ArchitectureRails"),Cube,Steel);
    auto* Marks=Batch(TEXT("CreaseAndCircle"),Cube,Paint);
    auto* SeatsA=Batch(TEXT("SeatsPrimary"),Cube,Colour(FLinearColor(.0120,.0420,.0570),0,.80f));
    auto* SeatsB=Batch(TEXT("SeatsAccent"),Cube,Colour(FLinearColor(.0180,.0210,.0380),0,.80f));
    auto* Rope=Batch(TEXT("BoundaryRope"),Cylinder,Colour(FLinearColor(.5000,.5200,.5100),0,.68f));
    LED=Colour(FLinearColor(.0150,.2600,.3000),1.05f,.30f);
    auto* Boards=Batch(TEXT("BoundaryLED"),Cube,LED);
    auto* BoardsAlt=Batch(TEXT("BoundaryLEDAlt"),Cube,Colour(FLinearColor(.0180,.0300,.0700),.55f,.30f));
    LampMaterial=Tinted(Mat(TEXT("M_Light")),FLinearColor(.78,.88,1),7.5f);
    auto* Lamps=Batch(TEXT("FloodlightArrays"),Cube,LampMaterial);
    auto* Columns=Batch(TEXT("StadiumColumns"),Cylinder,Steel);
    auto Add=[](UHierarchicalInstancedStaticMeshComponent* B,FVector P,FVector S,FRotator R=FRotator::ZeroRotator)
    {if(!B||!B->GetStaticMesh())return;const FVector Size=B->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(1));B->AddInstance(FTransform(R,P,S/Size));};
    auto Beam=[&](UHierarchicalInstancedStaticMeshComponent* B,FVector A,FVector E,float Width)
    {Add(B,(A+E)*.5f,FVector(Width,Width,(E-A).Size()),FRotationMatrix::MakeFromZ(E-A).Rotator());};

    int Section=0;
    auto Emit=[&](FC26Surface& S,UMaterialInterface* M)
    {
        if(S.IsEmpty())return;
        Bowl->CreateMeshSection_LinearColor(Section,S.V,S.T,S.N,S.UV,S.C,S.Tan,false);
        Bowl->SetMaterial(Section++,M);S=FC26Surface();
    };

    // ================= PLAYING SURFACE =================
    FC26Surface Ring,Ap,Sq,Pit,Worn,Mark,Resting;
    TArray<FC26Surface> Turf;Turf.SetNum(6);
    // 36 mowing bands across the full turf ellipse, perpendicular to the pitch, cycling through six
    // tones so the roller pattern reads as real cut grass rather than two alternating stripes. From
    // behind the striker they recede as banded perspective lines and give the ground its length.
    const int Bands=36;
    for(int I=0;I<Bands;++I)
    {
        const float Y0=-TurfRY+I*(2*TurfRY/Bands),Y1=Y0+2*TurfRY/Bands;
        static const int Cycle[6]={0,1,2,3,4,5};
        Turf[Cycle[I%6]].Band(Y0,Y1,TurfRX,TurfRY,0.f);
    }
    for(int I=0;I<6;++I)Emit(Turf[I],Mow[I]);
    // Darker watered collar just inside the rope, and a dry apron between rope and hoardings.
    for(int I=0;I<Segments;++I)
    {
        const float A=2*PI*I/Segments,B=2*PI*(I+1)/Segments;
        Ring.Quad(Oval(6060,6660,A,2.f),Oval(6060,6660,B,2.f),Oval(C26Field::RadiusX,C26Field::RadiusY,B,2.f),Oval(C26Field::RadiusX,C26Field::RadiusY,A,2.f),FVector::UpVector,6.f,1.f);
        Ap.Quad(Oval(C26Field::RadiusX+40,C26Field::RadiusY+40,A,2.5f),Oval(C26Field::RadiusX+40,C26Field::RadiusY+40,B,2.5f),Oval(TurfRX,TurfRY,B,2.5f),Oval(TurfRX,TurfRY,A,2.5f),FVector::UpVector,6.f,1.f);
    }
    Emit(Ring,MowRing);Emit(Ap,Apron);
    // The square is laid flat into the turf, not stacked on it: no floating slab edge anywhere.
    Sq.Plate(-920,-1220,920,1220,1.0f,6,6);
    Emit(Sq,SquareMat);
    // Five prepared strips inside the square; the middle one is today's pitch (3.05 m x 20.12 m).
    for(int Strip=-2;Strip<=2;++Strip)
    {
        if(Strip==0)continue;
        Resting.Plate(Strip*365-145,-1160,Strip*365+145,1160,1.8f,2,10);
    }
    Emit(Resting,RestingMat);
    Pit.Plate(-152.5f,-1170,152.5f,1170,2.2f,2,12);
    Emit(Pit,PitchMat);
    // Wear: bowler landing areas, footmarks outside off, and the batting creases.
    for(int End:{-1,1})
    {
        Worn.Plate(-75,End*885,10,End*1010,2.5f);
        Worn.Plate(-70,End*820,32,End*920,2.6f);
    }
    Emit(Worn,WornMat);
    // Crease paint, flat on the strip.
    for(int End:{-1,1})
    {
        Mark.Plate(-183,End*C26Field::CreaseY-2.5f,183,End*C26Field::CreaseY+2.5f,2.9f);
        Mark.Plate(-132,End*C26Field::WicketY-2.5f,132,End*C26Field::WicketY+2.5f,2.9f);
        for(int Side:{-1,1})Mark.Plate(Side*132-2.5f,End*884,Side*132+2.5f,End*1128,2.9f);
    }
    Emit(Mark,Paint);

    // Boundary rope, hoardings and the 30-yard ring.
    for(int I=0;I<128;++I)
    {
        const FVector A=C26Field::RopePoint(I*2*PI/128),B=C26Field::RopePoint((I+1)*2*PI/128);
        Beam(Rope,A+FVector(0,0,4),B+FVector(0,0,4),9);
        const float Ang=2*PI*I/128;
        const bool Sight=FMath::Abs(FMath::Cos(Ang))<.20f;
        if(I%2==0&&!Sight)
        {
            const FVector P=Oval(BoardRX,BoardRY,Ang,48);
            Add(I%8==0?BoardsAlt:Boards,P,FVector(330,18,96),FRotator(0,FMath::RadiansToDegrees(Ang)+90,0));
        }
        if(I%4==0)
        {
            const float A0=2*PI*I/128,A1=A0+.030f;
            Beam(Marks,FVector(2740*FMath::Cos(A0),2740*FMath::Sin(A0),6),FVector(2740*FMath::Cos(A1),2740*FMath::Sin(A1),6),4);
        }
    }
    // Sight screens behind each bowler's arm; the ball needs a clean background to read against.
    for(int End:{-1,1})
    {
        auto* ScreenBatch=Batch(TEXT("SightScreen"),Cube,Screen);
        Add(ScreenBatch,FVector(0,End*7620,330),FVector(1450,28,560));
        Add(Rails,FVector(0,End*7660,60),FVector(1900,60,120));
        for(int Post:{-1,1})Beam(Columns,FVector(Post*860,End*7665,0),FVector(Post*860,End*7665,900),26);
    }

    // ================= LOWER BOWL =================
    FC26Surface Con,Stl,Fac,Glass;
    Fac.Sweep(WallRX,WallRY,0.f,WallRX,WallRY,WallTop,false);                 // Perimeter wall.
    Con.Sweep(WallRX,WallRY,WallTop,WallRX+40,WallRY+40,WallTop,true);        // Wall capping.
    for(int Row=0;Row<LowerRows;++Row)
    {
        const float R=Row*LowerDepth,Z=WallTop+15.f+Row*LowerRise;
        Con.Sweep(WallRX+R,WallRY+R,Z,WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z,true);
        Stl.Sweep(WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z,WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z+LowerRise,false);
    }
    const float LowerBackRX=WallRX+LowerRows*LowerDepth,LowerBackRY=WallRY+LowerRows*LowerDepth;
    const float LowerBackZ=WallTop+15.f+LowerRows*LowerRise;
    // Concourse: solid band, glazed hospitality ribbon, then the upper bowl above it.
    Fac.Sweep(LowerBackRX,LowerBackRY,LowerBackZ,LowerBackRX,LowerBackRY,LowerBackZ+120.f,false);
    Glass.Sweep(LowerBackRX,LowerBackRY,LowerBackZ+120.f,LowerBackRX,LowerBackRY,LowerBackZ+300.f,false);
    Fac.Sweep(LowerBackRX,LowerBackRY,LowerBackZ+300.f,UpperRX,UpperRY,UpperZ,false);
    Con.Sweep(LowerBackRX,LowerBackRY,LowerBackZ,LowerBackRX+320,LowerBackRY+320,LowerBackZ,true);

    // ================= UPPER BOWL =================
    for(int Row=0;Row<UpperRows;++Row)
    {
        const float R=Row*UpperDepth,Z=UpperZ+Row*UpperRise;
        Con.Sweep(UpperRX+R,UpperRY+R,Z,UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z,true);
        Stl.Sweep(UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z,UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z+UpperRise,false);
    }
    const float UpperBackRX=UpperRX+UpperRows*UpperDepth,UpperBackRY=UpperRY+UpperRows*UpperDepth;
    const float UpperBackZ=UpperZ+UpperRows*UpperRise;
    Fac.Sweep(UpperBackRX,UpperBackRY,UpperBackZ,UpperBackRX+260,UpperBackRY+260,UpperBackZ+520,false);

    // ================= ROOF =================
    FC26Surface Under,Top;
    Under.Sweep(UpperRX-320,UpperRY-320,RoofInnerZ,RoofOuterRX,RoofOuterRY,RoofOuterZ,false,Segments);
    for(int I=0;I<Segments;++I)
    {
        const float A=2*PI*I/Segments,B=2*PI*(I+1)/Segments;
        const FVector P0=Oval(UpperRX-320,UpperRY-320,A,RoofInnerZ),P1=Oval(UpperRX-320,UpperRY-320,B,RoofInnerZ);
        const FVector Q0=Oval(RoofOuterRX,RoofOuterRY,A,RoofOuterZ),Q1=Oval(RoofOuterRX,RoofOuterRY,B,RoofOuterZ);
        Under.Quad(P0,P1,Q1,Q0,FVector(0,0,-1),4.f,1.f);
        Top.Quad(P0+FVector(0,0,55),P1+FVector(0,0,55),Q1+FVector(0,0,55),Q0+FVector(0,0,55),FVector::UpVector,4.f,1.f);
    }
    Emit(Under,RoofUnder);Emit(Top,RoofTop);
    // Illuminated fascia band on the roof lip: the strongest night-stadium read from pitch level.
    FC26Surface Fascia;
    Fascia.Sweep(UpperRX-360,UpperRY-360,RoofInnerZ-165,UpperRX-360,UpperRY-360,RoofInnerZ,false);
    Emit(Fascia,LED);
    Emit(Con,Concrete);Emit(Stl,Steel);Emit(Fac,Facade);Emit(Glass,Glazing);

    // Roof trusses only, sitting on top of the canopy. An outer ring of ground-to-roof columns
    // reads as a forest of poles through the seating gaps from every camera, so there is none.
    for(int I=0;I<28;++I)
    {
        const float A=2*PI*I/28;
        Beam(Rails,Oval(UpperRX-260,UpperRY-260,A,RoofInnerZ+70),Oval(RoofOuterRX-140,RoofOuterRY-140,A,RoofOuterZ+70),22);
    }

    // ================= SEATING AND CROWD =================
    const int Density=FMath::Clamp(CrowdQuality,0,3);
    const float Pitch2=Density>=3?68.f:Density==2?86.f:Density==1?135.f:205.f;
    const int RowStep=Density>=2?1:2;
    FRandomStream Random(2626);
    const FLinearColor Shirts[8]={
        FLinearColor(.0180,.0620,.0760),FLinearColor(.1250,.0180,.0140),FLinearColor(.1400,.1330,.1080),
        FLinearColor(.0300,.0380,.0560),FLinearColor(.0700,.0740,.0880),FLinearColor(.0900,.0540,.0290),
        FLinearColor(.0230,.0900,.0700),FLinearColor(.1600,.1000,.0300)};
    TArray<UHierarchicalInstancedStaticMeshComponent*> Bodies,Heads;
    for(int G=0;G<8;++G)
    {
        auto* M=Tinted(Mat(TEXT("M_Crowd")),Shirts[G],.02f,.95f);CrowdMaterials.Add(M);
        Bodies.Add(Batch(*FString::Printf(TEXT("CrowdGroup%d"),G),Person,M));
        if(Density>=2)Heads.Add(Batch(*FString::Printf(TEXT("CrowdHeads%d"),G),Person,
            Colour(FLinearColor(.115f+.028f*G,.072f+.020f*G,.050f+.014f*G),0,.85f)));
    }
    auto PopulateTier=[&](float RX,float RY,float BaseZ,int Rows,float Depth,float Rise)
    {
        for(int Row=0;Row<Rows;Row+=RowStep)
        {
            const float R=Row*Depth,Z=BaseZ+Row*Rise;
            const float RadX=RX+R,RadY=RY+R;
            const int Count=FMath::Max(24,FMath::RoundToInt(2*PI*RadX/Pitch2));
            for(int J=0;J<Count;++J)
            {
                const float A=2*PI*J/Count;
                if(J%18==0)continue;                                // One-seat-wide radial aisles.
                const FRotator Face(0,FMath::RadiansToDegrees(A)+90.f,0);
                const FVector Seat=Oval(RadX+Depth*.55f,RadY+Depth*.55f,A,Z+11.f);
                if(Row%2==0)Add((J/7)%2?SeatsA:SeatsB,Seat,FVector(46,44,22),Face);
                if(Random.FRand()<.07f)continue;                     // A handful of empty seats.
                const int G=Random.RandRange(0,7);
                const float Lean=Random.FRandRange(-7.f,7.f),Size=Random.FRandRange(.88f,1.12f);
                const FVector P=Oval(RadX+Depth*.5f,RadY+Depth*.5f,A,Z+42.f*Size);
                Add(Bodies[G],P,FVector(37,27,54)*Size,FRotator(Lean,FMath::RadiansToDegrees(A)+Random.FRandRange(-14.f,14.f),0));
                if(Heads.Num())Add(Heads[G],P+FVector(0,0,35*Size),FVector(20,19,22)*Size,Face);
            }
        }
    };
    PopulateTier(WallRX,WallRY,WallTop+15.f,LowerRows,LowerDepth,LowerRise);
    PopulateTier(UpperRX,UpperRY,UpperZ,UpperRows,UpperDepth,UpperRise);

    // ================= FLOODLIGHT PYLONS =================
    for(int I=0;I<6;++I)
    {
        const float A=2*PI*I/6;
        const FVector Foot=Oval(10250,10980,A,0);
        Beam(Columns,Foot+FVector(0,0,2450),Foot+FVector(0,0,4180),62);
        const FRotator R(0,FMath::RadiansToDegrees(A)+90,0);
        Add(Rails,Foot+FVector(0,0,4240),FVector(940,110,44),R);
        for(int Row=0;Row<5;++Row)for(int Col=0;Col<10;++Col)
        {
            const FVector Offset=R.RotateVector(FVector((Col-4.5f)*88,-52,4110+Row*86));
            Add(Lamps,Foot+Offset,FVector(74,17,68),R);
        }
    }
    // Continuous roof-edge lighting rig: a bright, even wash that reads across the whole bowl.
    for(int I=0;I<Segments;I+=2)
    {
        const float A=2*PI*I/Segments;
        Add(Lamps,Oval(UpperRX-330,UpperRY-330,A,RoofInnerZ-235),FVector(150,26,34),FRotator(18,FMath::RadiansToDegrees(A)+90,0));
    }

    // ================= SIGNAGE AND SCREENS =================
    auto MakeSign=[&](const FString& Body,const FVector& At,const FRotator& Facing,float Size,FColor Ink)
    {
        auto* Text=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);Text->SetupAttachment(RootComponent);
        Text->RegisterComponent();AddInstanceComponent(Text);Signs.Add(Text);
        Text->SetText(FText::FromString(Body));Text->SetWorldSize(Size);Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetVerticalAlignment(EVRTA_TextCenter);Text->SetTextRenderColor(Ink);
        Text->SetRelativeLocation(At);Text->SetRelativeRotation(Facing);Text->SetCastShadow(false);
    };
    for(int I=0;I<10;++I)
    {
        const float A=2*PI*I/10+.31f;
        MakeSign(I%2?TEXT("ECLIPSE OVAL"):TEXT("CRICKET 26"),Oval(LowerBackRX-24,LowerBackRY-24,A,LowerBackZ+210.f),
            FRotator(0,FMath::RadiansToDegrees(A)+180,0),132.f,FColor(150,224,220));
    }
    // A live video wall is one of the few things in a night ground that is brighter than the field.
    // At a glow of .30 over a near-black tint these read as holes cut out of the stand.
    auto* Panels=Batch(TEXT("BroadcastScreens"),Cube,Colour(FLinearColor(.026,.070,.092),2.10f,.25f));
    for(int I:{-1,1})
    {
        const FVector At(I*1100.f,I*(UpperRY+700.f),UpperZ+1500.f);
        Add(Panels,At,FVector(2600,60,1180),FRotator(0,I>0?-90.f:90.f,0));
        Add(Rails,At+FVector(0,0,-640),FVector(2700,90,110),FRotator(0,I>0?-90.f:90.f,0));
        MakeSign(TEXT("CRICKET 26\nSUPER OVER"),At+FVector(I*-40.f,I*-46.f,0),FRotator(0,I>0?-90.f:90.f,0),240.f,FColor(196,240,238));
    }

    // ================= NIGHT SKY =================
    // Layered dome: warm city glow at the horizon fading to deep navy overhead, so the roof line
    // and the pylons separate cleanly instead of dissolving into a flat black void.
    const FLinearColor SkyRamp[6]={
        FLinearColor(.0130,.0225,.0400),FLinearColor(.0092,.0160,.0305),FLinearColor(.0058,.0102,.0212),
        FLinearColor(.0034,.0060,.0140),FLinearColor(.0019,.0034,.0088),FLinearColor(.0011,.0020,.0056)};
    for(int Layer=0;Layer<6;++Layer)
    {
        FC26Surface Dome;
        const float P0=Layer/6.f,P1=(Layer+1)/6.f;
        const float E0=FMath::Lerp(-.06f,PI/2.f,P0),E1=FMath::Lerp(-.06f,PI/2.f,P1);
        for(int I=0;I<64;++I)
        {
            const float A=2*PI*I/64,B=2*PI*(I+1)/64;
            auto Point=[&](float Ang,float Elev){return FVector(34000*FMath::Cos(Elev)*FMath::Cos(Ang),34000*FMath::Cos(Elev)*FMath::Sin(Ang),34000*FMath::Sin(Elev));};
            Dome.Quad(Point(A,E0),Point(B,E0),Point(B,E1),Point(A,E1),-Point((A+B)*.5f,(E0+E1)*.5f).GetSafeNormal());
        }
        Sky->CreateMeshSection_LinearColor(Layer,Dome.V,Dome.T,Dome.N,Dome.UV,Dome.C,Dome.Tan,false);
        Sky->SetMaterial(Layer,Tinted(Mat(TEXT("M_Sky"))?Mat(TEXT("M_Sky")):Surface,SkyRamp[Layer],1.f,.98f));
    }
    (void)White;(void)VenueTeal;(void)Navy;(void)GrassBase;(void)PitchBase;
    for(auto& B:Batches){B->bAutoRebuildTreeOnInstanceChanges=true;B->BuildTreeIfOutdated(true,true);}
    int Instances=0;for(auto& B:Batches)Instances+=B->GetInstanceCount();
    UE_LOG(LogC26Venue,Display,TEXT("Eclipse Oval built: %d mesh sections, %d batches, %d instances."),Section,Batches.Num(),Instances);
    UE_LOG(LogC26Venue,Display,TEXT("Venue surfaces: bowl=%s sections=%d materials=%d sky=%s sections=%d"),
        *Bowl->GetName(),Bowl->GetNumSections(),Bowl->GetNumMaterials(),*Sky->GetName(),Sky->GetNumSections());
}
void AC26Stadium::SetQuality(int Level)
{
    const int Wanted=FMath::Clamp(Level,0,3);
    const bool Rebuild=Wanted!=CrowdQuality;
    CrowdQuality=Wanted;
    KeyLight->SetCastShadows(Level>0);KeyLight->DynamicShadowCascades=Level<2?2:3;
    KeyLight->DynamicShadowDistanceMovableLight=Level<2?5000:9000;
    if(CrossLight)CrossLight->SetVisibility(Level>0);
    for(auto& F:Floods)if(F)F->SetVisibility(Level>=3);
    if(Rebuild&&HasActorBegunPlay())BuildVenue();
    for(auto& B:Batches)
    {
        if(B->GetName().StartsWith(TEXT("CrowdHeads")))B->SetVisibility(Level>0);
        B->SetCullDistances(0,Level==0?26000:42000);
    }
}
void AC26Stadium::React(float Intensity){CrowdReaction=FMath::Max(CrowdReaction,Intensity);}
void AC26Stadium::UpdateAtmosphere(float Time)
{
    // Fixed-step so the celebration decay does not depend on frame rate.
    AtmosphereUpdateAccumulator+=FMath::Clamp(Time-LastAtmosphereTime,0.f,.25f);LastAtmosphereTime=Time;
    if(AtmosphereUpdateAccumulator<.05f)return;
    const float Step=AtmosphereUpdateAccumulator;AtmosphereUpdateAccumulator=0;
    CrowdReaction=FMath::Max(0.f,CrowdReaction-Step*.42f);
    const float Pulse=FMath::Max(0.f,FMath::Sin(Time*4.1f));
    if(LED)LED->SetScalarParameterValue(TEXT("Glow"),1.05f+CrowdReaction*Pulse*1.5f);
    if(LampMaterial)LampMaterial->SetScalarParameterValue(TEXT("Glow"),7.5f+CrowdReaction*2.5f);
    for(auto M:CrowdMaterials)if(M)M->SetScalarParameterValue(TEXT("Glow"),.02f+CrowdReaction*.30f);
}
