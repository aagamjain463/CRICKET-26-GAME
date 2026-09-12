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
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogC26Venue,Log,All);

namespace
{
// Eclipse Oval, in centimetres. The rope radii live in C26Field and are authoritative for
// gameplay; everything architectural is derived from them so the bowl can never drift out of
// proportion with the playing area.
constexpr float TurfRX=7270.f,TurfRY=7970.f;
constexpr float BoardRX=6920.f,BoardRY=7560.f;
constexpr float WallRX=7210.f,WallRY=7910.f;
constexpr float WallTop=155.f;
constexpr int   LowerRows=16;
constexpr float LowerDepth=90.f,LowerRise=43.f;
constexpr float UpperRX=8970.f,UpperRY=9670.f,UpperZ=1260.f;
constexpr int   UpperRows=18;
constexpr float UpperDepth=92.f,UpperRise=48.f;
constexpr float RoofInnerZ=2490.f,RoofOuterZ=2850.f;
constexpr float RoofOuterRX=11130.f,RoofOuterRY=11830.f;
constexpr int   Segments=192; // 24 seating bays, eight segments per bay.
constexpr float FloodCandelas=13000.f;

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
    Architecture=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EclipseArchitecture"));Architecture->SetupAttachment(RootComponent);
    Architecture->SetCollisionEnabled(ECollisionEnabled::NoCollision);Architecture->SetCastShadow(false);
    Sky=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NightSky"));Sky->SetupAttachment(RootComponent);
    Sky->SetCollisionEnabled(ECollisionEnabled::NoCollision);Sky->SetCastShadow(false);
    Sky->bReceivesDecals=false;
    // One shadow-casting bank plus an unshadowed cross fill. Explicit forward-shading priorities
    // stop the renderer warning about two directional lights competing for the single slot.
    KeyLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FloodlightKey"));KeyLight->SetupAttachment(RootComponent);
    KeyLight->SetRelativeRotation(FRotator(-36,-62,0));KeyLight->SetIntensity(5.6f);KeyLight->SetLightColor(FLinearColor(1,.97,.91));
    KeyLight->DynamicShadowDistanceMovableLight=9000;KeyLight->DynamicShadowCascades=3;KeyLight->ForwardShadingPriority=1;
    KeyLight->SetSpecularScale(.85f);
    CrossLight=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FloodlightCross"));CrossLight->SetupAttachment(RootComponent);
    // Deliberately shallower than the key. Floodlit athletes are the brightest readable thing on a
    // broadcast, but measured captures had the players (luma 53-69) darker than the turf (105-120):
    // a steep key rakes the horizontal ground and barely touches a vertical torso. Lowering this
    // fill toward the horizon puts light back on bodies without lifting the ground or the crowd.
    CrossLight->SetRelativeRotation(FRotator(-13,126,0));CrossLight->SetIntensity(3.30f);CrossLight->SetLightColor(FLinearColor(.91,.95,1));
    CrossLight->SetCastShadows(false);CrossLight->ForwardShadingPriority=0;
    FillLight=CreateDefaultSubobject<USkyLightComponent>(TEXT("StadiumFill"));FillLight->SetupAttachment(RootComponent);
    FillLight->SetIntensity(.95f);FillLight->SetLightColor(FLinearColor(.65,.70,.76));FillLight->SetLowerHemisphereColor(FLinearColor(.075,.082,.058));
    FillLight->bLowerHemisphereIsBlack=false;
    Haze=CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("StadiumHaze"));Haze->SetupAttachment(RootComponent);
    Haze->SetFogDensity(.000042f);Haze->SetFogHeightFalloff(.09f);Haze->SetFogInscatteringColor(FLinearColor(.055,.086,.145));
    Haze->SetFogMaxOpacity(.62f);Haze->SetStartDistance(2600.f);
    Grade=CreateDefaultSubobject<UPostProcessComponent>(TEXT("BroadcastGrade"));Grade->SetupAttachment(RootComponent);Grade->bUnbound=true;
    auto& P=Grade->Settings;
    P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    P.bOverride_AutoExposureBias=true;P.AutoExposureBias=-.45f;
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
    P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(1.06,1.06,1.04,1);
    P.bOverride_ColorContrast=true;P.ColorContrast=FVector4(1.07,1.07,1.08,1);
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
    Shafts=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FloodlightShafts"));Shafts->SetupAttachment(RootComponent);
    Shafts->SetCollisionEnabled(ECollisionEnabled::NoCollision);Shafts->SetCastShadow(false);
    Shafts->bReceivesDecals=false;Shafts->SetTranslucentSortPriority(6);
    // CreateDefaultSubobject hands back Static mobility, and a Static light is never entered into
    // the dynamic shadow pass. That is why nothing in this venue cast a shadow at any quality
    // level: the key light was static, so the athletes' shadows had nowhere to be drawn. The
    // procedural meshes are also rebuilt every BeginPlay, which Static mobility does not permit.
    for(UActorComponent* Component:GetComponents())
        if(auto* Scene=Cast<USceneComponent>(Component))Scene->SetMobility(EComponentMobility::Movable);
}
void AC26Stadium::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);BuildVenue();}
// Always rebuild in play. Dynamic material instances do not survive map serialization, so a venue
// restored from the package renders every tinted surface as its parent default.
void AC26Stadium::BeginPlay()
{
    Super::BeginPlay();ConfigureLighting();BuildVenue();BuildLightShafts();
    // The saved map serializes the prototype's component overrides over new C++ defaults, and the
    // authored profile lives on this actor: re-assert both after the venue exists.
    SetEnvironment(EnvironmentProfile);
    SetPitchCondition(PitchCondition);
    FillLight->RecaptureSky();
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("C26Scale")))
    {
        DrawDebugLine(GetWorld(),FVector(0,-C26Field::WicketY,15),FVector(0,C26Field::WicketY,15),FColor::Yellow,true,-1,0,2);
        DrawDebugString(GetWorld(),FVector(0,0,40),TEXT("20.12 m wicket to wicket / 3.05 m strip"),nullptr,FColor::White,-1);
        DrawDebugLine(GetWorld(),FVector(280,900,0),FVector(280,900,182),FColor::Cyan,true,-1,0,2);
        DrawDebugString(GetWorld(),FVector(280,900,200),TEXT("1.82 m athlete reference"),nullptr,FColor::White,-1);
        for(int I=0;I<128;++I)DrawDebugLine(GetWorld(),C26Field::RopePoint(I*2*PI/128),C26Field::RopePoint((I+1)*2*PI/128),FColor::Yellow,true);
    }
#endif
}
void AC26Stadium::BuildLightShafts()
{
    // Six shallow cones of additive haze, one hanging under each pylon head and aimed at the
    // square. Real volumetrics would cost far more than a mobile frame can spare; six two-quad
    // fans that fade to nothing at the rim read the same way from every broadcast angle and cost
    // nothing measurable. Vertex alpha does the falloff -- see Tools/BuildPresentationAssets.py.
    if(!Shafts)return;
    Shafts->ClearAllMeshSections();
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Beam.M_Beam"));
    if(!Base){UE_LOG(LogC26Venue,Warning,TEXT("M_Beam missing; floodlight shafts disabled."));return;}
    ShaftMaterial=UMaterialInstanceDynamic::Create(Base,this);
    ShaftMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.52,.63,.86));
    ShaftMaterial->SetScalarParameterValue(TEXT("Glow"),.018f);
    ShaftMaterial->SetScalarParameterValue(TEXT("Opacity"),1.f);
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;
    TArray<FLinearColor> C;TArray<FProcMeshTangent> Tan;
    for(int I=0;I<6;++I)
    {
        const float A=PI/6+2*PI*I/6;
        const FVector Head=Oval(10800,11500,A,4360);
        const FVector Aim(0,0,180);
        const FVector Down=(Aim-Head).GetSafeNormal();
        const FVector Side=FVector::CrossProduct(Down,FVector::UpVector).GetSafeNormal();
        const FVector Lift=FVector::CrossProduct(Side,Down).GetSafeNormal();
        const float Throw=(Aim-Head).Size();
        // Two crossed fans per pylon so the shaft keeps its body when the camera swings around it.
        for(int Plane=0;Plane<2;++Plane)
        {
            const FVector Across=Plane==0?Side:Lift;
            const int Base0=V.Num();
            V.Add(Head-Across*140);V.Add(Head+Across*140);
            V.Add(Head+Down*Throw+Across*1750);V.Add(Head+Down*Throw-Across*1750);
            for(int K=0;K<4;++K){N.Add(-Down);UV.Add(FVector2D(K&1,K>1));}
            // Bright at the lamp, gone by the time it reaches the turf: a shaft that landed at full
            // strength would paint a hard bright disc on the outfield.
            C.Add(FLinearColor(0,0,0,.85f));C.Add(FLinearColor(0,0,0,.85f));
            C.Add(FLinearColor(0,0,0,0.f));C.Add(FLinearColor(0,0,0,0.f));
            T.Append({Base0,Base0+1,Base0+2,Base0,Base0+2,Base0+3});
        }
    }
    Shafts->CreateMeshSection_LinearColor(0,V,T,N,UV,C,Tan,false);
    Shafts->SetMaterial(0,ShaftMaterial);
    UE_LOG(LogC26Venue,Display,TEXT("C26_SHAFT built verts=%d tris=%d visible=%d"),V.Num(),T.Num()/3,Shafts->IsVisible()?1:0);
}
float AC26Stadium::CrowdTargetForState(EC26CrowdState State)
{
    switch (State)
    {
    case EC26CrowdState::Six:          return 1.00f;
    case EC26CrowdState::Wicket:       return 0.95f;
    case EC26CrowdState::Win:          return 1.00f;
    case EC26CrowdState::Boundary:     return 0.80f;
    case EC26CrowdState::Excited:      return 0.60f;
    case EC26CrowdState::Anticipation: return 0.45f;
    case EC26CrowdState::Tense:        return 0.35f;
    case EC26CrowdState::Loss:         return 0.25f;
    case EC26CrowdState::Calm:
    default:                           return 0.06f;
    }
}
float AC26Stadium::CrowdRateForState(EC26CrowdState State)
{
    // How fast the visible excitement eases toward its target. Big moments spike fast and decay
    // through CrowdReaction falloff; tension and victory linger instead of settling.
    switch (State)
    {
    case EC26CrowdState::Six:          return 3.2f;
    case EC26CrowdState::Wicket:       return 3.0f;
    case EC26CrowdState::Win:          return 1.2f;
    case EC26CrowdState::Boundary:     return 2.6f;
    case EC26CrowdState::Excited:      return 2.0f;
    case EC26CrowdState::Anticipation: return 2.4f;
    case EC26CrowdState::Tense:        return 0.8f;
    case EC26CrowdState::Loss:         return 1.0f;
    case EC26CrowdState::Calm:
    default:                           return 1.4f;
    }
}
float AC26Stadium::ExposureBiasForProfile(EC26EnvironmentProfile Profile)
{
    switch (Profile)
    {
    case EC26EnvironmentProfile::ClearDay:      return -0.30f;
    case EC26EnvironmentProfile::LateAfternoon: return -0.38f;
    case EC26EnvironmentProfile::Night:
    default:                                    return -0.45f;
    }
}
FLinearColor AC26Stadium::PitchTintForCondition(EC26PitchCondition Condition)
{
    switch (Condition)
    {
    case EC26PitchCondition::Fresh: return FLinearColor(0.94f, 1.02f, 0.94f);
    case EC26PitchCondition::Dry:   return FLinearColor(1.06f, 1.00f, 0.90f);
    case EC26PitchCondition::Worn:  return FLinearColor(1.03f, 0.98f, 0.90f);
    case EC26PitchCondition::Used:
    default:                        return FLinearColor(1.00f, 1.00f, 1.00f);
    }
}
void AC26Stadium::SetCrowdState(EC26CrowdState State)
{
    CrowdState = State;
    CrowdTarget = CrowdTargetForState(State);
    CrowdRate = CrowdRateForState(State);
    // A spike lands instantly: the ground is up before the ease would get there.
    if (State == EC26CrowdState::Six || State == EC26CrowdState::Wicket || State == EC26CrowdState::Win)
        CrowdReaction = FMath::Max(CrowdReaction, 0.75f);
    else if (State == EC26CrowdState::Boundary)
        CrowdReaction = FMath::Max(CrowdReaction, 0.55f);
}
void AC26Stadium::PulseLED(float Strength)
{
    LEDSpike = FMath::Max(LEDSpike, FMath::Clamp(Strength, 0.f, 1.5f));
    CrowdReaction = FMath::Max(CrowdReaction, FMath::Clamp(Strength * 0.8f, 0.f, 1.f));
}
void AC26Stadium::SetPitchCondition(EC26PitchCondition Condition)
{
    PitchCondition = Condition;
    if (GroundMID) GroundMID->SetVectorParameterValue(TEXT("Tint"), PitchTintForCondition(Condition));
}
void AC26Stadium::ApplyLightVisibility()
{
    const bool bNight = EnvironmentProfile == EC26EnvironmentProfile::Night;
    for (auto& F : Floods) if (F) F->SetVisibility(bNight && CrowdQuality >= 2);
    if (Shafts) Shafts->SetVisibility(bNight && CrowdQuality >= 2);
}
void AC26Stadium::SetEnvironment(EC26EnvironmentProfile Profile)
{
    EnvironmentProfile = Profile;
    const bool bDay = Profile == EC26EnvironmentProfile::ClearDay;
    const bool bAfternoon = Profile == EC26EnvironmentProfile::LateAfternoon;
    if (bDay)
    {
        KeyLight->SetRelativeRotation(FRotator(-52, -35, 0)); KeyLight->SetIntensity(6.4f);
        KeyLight->SetLightColor(FLinearColor(1.f, .96f, .89f));
        CrossLight->SetRelativeRotation(FRotator(-20, 140, 0)); CrossLight->SetIntensity(2.2f);
        CrossLight->SetLightColor(FLinearColor(.88f, .93f, 1.f));
        FillLight->SetIntensity(1.30f); FillLight->SetLightColor(FLinearColor(.70f, .78f, .90f));
        FillLight->SetLowerHemisphereColor(FLinearColor(.18f, .20f, .16f));
        Haze->SetFogDensity(.000008f); Haze->SetFogMaxOpacity(.12f); Haze->SetStartDistance(8000.f);
        Haze->SetFogInscatteringColor(FLinearColor(.45f, .55f, .65f));
        Grade->Settings.AutoExposureBias = ExposureBiasForProfile(Profile);
        Grade->Settings.ColorSaturation = FVector4(1.05, 1.05, 1.03, 1);
        Grade->Settings.BloomIntensity = .22f; Grade->Settings.BloomThreshold = 1.8f;
    }
    else if (bAfternoon)
    {
        KeyLight->SetRelativeRotation(FRotator(-28, -78, 0)); KeyLight->SetIntensity(6.0f);
        KeyLight->SetLightColor(FLinearColor(1.f, .88f, .72f));
        CrossLight->SetRelativeRotation(FRotator(-16, 132, 0)); CrossLight->SetIntensity(2.6f);
        CrossLight->SetLightColor(FLinearColor(.98f, .90f, .80f));
        FillLight->SetIntensity(1.05f); FillLight->SetLightColor(FLinearColor(.72f, .70f, .72f));
        FillLight->SetLowerHemisphereColor(FLinearColor(.14f, .12f, .09f));
        Haze->SetFogDensity(.000010f); Haze->SetFogMaxOpacity(.15f); Haze->SetStartDistance(7000.f);
        Haze->SetFogInscatteringColor(FLinearColor(.50f, .42f, .34f));
        Grade->Settings.AutoExposureBias = ExposureBiasForProfile(Profile);
        Grade->Settings.ColorSaturation = FVector4(1.07, 1.06, 1.03, 1);
        Grade->Settings.BloomIntensity = .28f; Grade->Settings.BloomThreshold = 1.6f;
    }
    else
    {
        // Night session: the calibrated broadcast grade. Mirrors ConfigureLighting so a saved map
        // restoring stale overrides converges back to the same look.
        KeyLight->SetRelativeRotation(FRotator(-36, -62, 0)); KeyLight->SetIntensity(5.6f);
        KeyLight->SetLightColor(FLinearColor(1.f, .97f, .91f));
        CrossLight->SetRelativeRotation(FRotator(-13, 126, 0)); CrossLight->SetIntensity(3.30f);
        CrossLight->SetLightColor(FLinearColor(.91f, .95f, 1.f));
        FillLight->SetIntensity(.95f); FillLight->SetLightColor(FLinearColor(.65f, .70f, .76f));
        FillLight->SetLowerHemisphereColor(FLinearColor(.075f, .082f, .058f));
        Haze->SetFogDensity(.000012f); Haze->SetFogMaxOpacity(.18f); Haze->SetStartDistance(6200.f);
        Haze->SetFogInscatteringColor(FLinearColor(.055f, .086f, .145f));
        Grade->Settings.AutoExposureBias = ExposureBiasForProfile(Profile);
        Grade->Settings.ColorSaturation = FVector4(1.03, 1.03, 1.02, 1);
        Grade->Settings.BloomIntensity = .30f; Grade->Settings.BloomThreshold = 1.55f;
    }
    // Sky ramp per session: warm pale horizon to blue zenith by day, amber dusk band late, deep
    // navy night. Tints land on the stored section materials so no geometry rebuild is needed.
    static const FLinearColor DayRamp[6] = {
        FLinearColor(.55f,.65f,.78f),FLinearColor(.42f,.55f,.74f),FLinearColor(.28f,.44f,.70f),
        FLinearColor(.17f,.33f,.64f),FLinearColor(.10f,.24f,.58f),FLinearColor(.06f,.17f,.50f)};
    static const FLinearColor DuskRamp[6] = {
        FLinearColor(.62f,.48f,.34f),FLinearColor(.48f,.38f,.34f),FLinearColor(.32f,.28f,.34f),
        FLinearColor(.19f,.19f,.30f),FLinearColor(.10f,.11f,.24f),FLinearColor(.05f,.06f,.17f)};
    static const FLinearColor NightRamp[6] = {
        FLinearColor(.0320f,.0510f,.0880f),FLinearColor(.0225f,.0365f,.0660f),FLinearColor(.0140f,.0235f,.0460f),
        FLinearColor(.0080f,.0140f,.0305f),FLinearColor(.0042f,.0078f,.0180f),FLinearColor(.0022f,.0042f,.0105f)};
    const FLinearColor* Ramp = bDay ? DayRamp : (bAfternoon ? DuskRamp : NightRamp);
    for (int I = 0; I < SkyMaterials.Num(); ++I)
        if (SkyMaterials[I]) SkyMaterials[I]->SetVectorParameterValue(TEXT("Tint"), Ramp[I % 6]);
    ApplyLightVisibility();
    if (HasActorBegunPlay() && FillLight) FillLight->RecaptureSky();
}
void AC26Stadium::ConfigureLighting()
{
    // Apply the authored setup after map deserialization: saved component overrides from the
    // prototype otherwise restore its 9-lux key and 1.25-strength blue sky over new C++ defaults.
    KeyLight->SetMobility(EComponentMobility::Movable);
    // Measured from `premium_base`: the players were the DARKEST thing on screen (kit luma 16-50)
    // while the turf sat at 130-155. On a broadcast the athletes are the brightest readable
    // elements and the field is behind them, so this was the single biggest reason a live frame
    // read as prototype rather than televised. The cause is geometric, not a colour: a 46-degree
    // key rakes the horizontal ground and misses a vertical torso almost entirely. Bringing the
    // key down toward the horizon puts light on bodies as well as on grass.
    KeyLight->SetRelativeRotation(FRotator(-36,-62,0));KeyLight->SetIntensity(5.6f);
    KeyLight->SetLightColor(FLinearColor(1,.97,.91));KeyLight->SetSpecularScale(.5f);
    // A stadium lamp bank is a large area source, not a point. Widening the source angle gives the
    // penumbra a soft edge like a real floodlight shadow instead of a hard stencil cutout.
    KeyLight->LightSourceAngle=2.1f;
    // Screen-space contact shadows pick up the small darkenings the cascades are too coarse to
    // resolve: under a boot, between bat and glove, where the ball meets the turf.
    KeyLight->ContactShadowLength=.045f;KeyLight->ContactShadowLengthInWS=false;
    // The cross bank is the one that actually lights a standing cricketer. Near-horizontal, so it
    // lands on shirts, faces and pads instead of on the top of the grass -- this is the bank that
    // separates a player from the field he is standing on.
    CrossLight->SetRelativeRotation(FRotator(-13,126,0));CrossLight->SetIntensity(3.30f);
    CrossLight->SetLightColor(FLinearColor(.91,.95,1));
    // Sky fill lifts the shadow side so a player is readable from every camera angle rather than
    // only when he happens to face the key. Raised well above the old .44: a night ground has
    // almost no ambient of its own and the low value was leaving half of every torso black.
    FillLight->SetIntensity(.95f);FillLight->SetLightColor(FLinearColor(.65,.70,.76));
    FillLight->SetLowerHemisphereColor(FLinearColor(.075,.082,.058));
    Haze->SetFogDensity(.000012f);Haze->SetFogMaxOpacity(.18f);Haze->SetStartDistance(6200.f);
    // Exposure. Calibrated against the reference broadcast, measured off its own frames: its
    // outfield reads luma 112-115 and its pitch 138, so that is what this grade aims at. The sign
    // of this parameter is now confirmed by capture, not assumed: -0.35 rendered the field at 68
    // and +0.30 at 166, so positive is brighter and a full stop was far too much. -0.45 lands the
    // turf at roughly 125 -- close to the reference without going dull.
    Grade->Settings.AutoExposureBias=-.45f;Grade->Settings.VignetteIntensity=0.f;
    Grade->Settings.BloomIntensity=.30f;Grade->Settings.BloomThreshold=1.55f;
    // Turf under floodlights is a saturated green, and the measured capture was reading closer to
    // grey-green than grass. Held near neutral rather than pushed: the reference green is a warm
    // yellow-green (R/G 0.87, B/G 0.61), and over-saturating turns the outfield neon.
    Grade->Settings.ColorSaturation=FVector4(1.03,1.03,1.02,1);
    Grade->Settings.ColorContrast=FVector4(1.07,1.07,1.08,1);
    Grade->Settings.ColorGamma=FVector4(1,1,1,1);
    for(int I=0;I<Floods.Num();++I)
    {
        const float A=PI/6+I*PI/2;
        const FVector Head=Oval(10800,11500,A,4360);
        Floods[I]->SetRelativeLocation(Head);Floods[I]->SetRelativeRotation((FVector(0,0,80)-Head).Rotation());
        Floods[I]->SetIntensity(FloodCandelas);Floods[I]->SetLightColor(FLinearColor(1,.98,.94));
    }
    ApplyLightVisibility();
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
    SkyMaterials.Reset();GroundMID = nullptr;
    Bowl->ClearAllMeshSections();Sky->ClearAllMeshSections();Architecture->ClearAllMeshSections();
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

    auto* Ground=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Environment/Materials/M_Eclipse_PlayingSurface.M_Eclipse_PlayingSurface"));
    if(!Ground)UE_LOG(LogC26Venue,Error,TEXT("Missing Eclipse playing surface: run Tools/BuildWorldAssets.py"));
    auto* Paint     = Colour(FLinearColor(.62,.64,.59),0,.87f);
    auto* Concrete  = Colour(FLinearColor(.16,.16,.145),0,.92f);
    auto* Steel     = Colour(FLinearColor(.17,.19,.21),0,.52f);
    auto* Facade    = Colour(FLinearColor(.045,.055,.065),0,.83f);
    auto* RoofUnder = Colour(FLinearColor(.115,.127,.134),.06f,.82f);
    auto* RoofTop   = Colour(FLinearColor(.28,.29,.28),0,.72f);
    auto* Glazing   = Colour(FLinearColor(.095,.13,.145),.22f,.28f);
    auto* Screen    = Colour(FLinearColor(.009,.013,.019),0.f,.94f);

    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Mesh=[](const TCHAR* Folder,const TCHAR* Name){return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Cricket26/Environment/%s/SM_Eclipse_%s.SM_Eclipse_%s"),Folder,Name,Name));};
    auto* Person=Mesh(TEXT("Crowd"),TEXT("SeatedSpectator"));
    auto* SeatMesh=Mesh(TEXT("Stadium"),TEXT("StadiumSeat"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Rails=Batch(TEXT("ArchitectureRails"),Cube,Steel);
    auto* Marks=Batch(TEXT("CreaseAndCircle"),Cube,Paint);
    auto* SeatsA=Batch(TEXT("SeatsPrimary"),SeatMesh,Colour(FLinearColor(.035,.12,.19),0,.80f));
    auto* SeatsB=Batch(TEXT("SeatsAccent"),SeatMesh,Colour(FLinearColor(.25,.27,.26),0,.80f));
    auto* Rope=Batch(TEXT("BoundaryRope"),Cylinder,Colour(FLinearColor(.5000,.5200,.5100),0,.68f));
    // Boundary LED. On a real broadcast these are the brightest thing at ground level and the
    // sponsor colour is what frames every shot of the field. The old values were so dark
    // (.013 red) that the boards read as a gap between the turf and the stands rather than as
    // an advertising ribbon.
    LED=Colour(FLinearColor(.055,.470,.560),1.30f,.40f);
    auto* Boards=Batch(TEXT("BoundaryLED"),Cube,LED);
    auto* BoardsAlt=Batch(TEXT("BoundaryLEDAlt"),Cube,Colour(FLinearColor(.520,.075,.090),1.15f,.32f));
    auto* BoardsGold=Batch(TEXT("BoundaryLEDGold"),Cube,Colour(FLinearColor(.680,.520,.120),1.25f,.35f));
    auto* Pegs=Batch(TEXT("BoundaryPegs"),Cylinder,Steel);
    LampMaterial=Tinted(Mat(TEXT("M_Light")),FLinearColor(1,.93,.79),4.8f);
    auto* Lamps=Batch(TEXT("FloodlightArrays"),Cube,LampMaterial);
    auto* Columns=Batch(TEXT("StadiumColumns"),Cylinder,Steel);
    auto Add=[](UHierarchicalInstancedStaticMeshComponent* B,FVector P,FVector S,FRotator R=FRotator::ZeroRotator)
    {if(!B||!B->GetStaticMesh())return;const FVector Size=B->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(1));B->AddInstance(FTransform(R,P,S/Size));};
    auto Beam=[&](UHierarchicalInstancedStaticMeshComponent* B,FVector A,FVector E,float Width)
    {Add(B,(A+E)*.5f,FVector(Width,Width,(E-A).Size()),FRotationMatrix::MakeFromZ(E-A).Rotator());};

    int Section=0;bool Structure=false;
    auto Emit=[&](FC26Surface& S,UMaterialInterface* M)
    {
        if(S.IsEmpty())return;
        auto* Target=Structure?Architecture.Get():Bowl.Get();
        Target->CreateMeshSection_LinearColor(Section,S.V,S.T,S.N,S.UV,S.C,S.Tan,false);
        Target->SetMaterial(Section++,M);S=FC26Surface();
    };

    // ================= PLAYING SURFACE =================
    FC26Surface Turf,Mark;
    Turf.Band(-TurfRY,TurfRY,TurfRX,TurfRY,C26Field::SurfaceZ,128);
    // The condition re-tint lives on a venue-owned instance so switching Fresh/Used/Dry/Worn never
    // touches the shared baked asset or pops streaming.
    if (Ground) { GroundMID = UMaterialInstanceDynamic::Create(Ground, this); GroundMID->SetVectorParameterValue(TEXT("Tint"), PitchTintForCondition(PitchCondition)); }
    Emit(Turf,GroundMID ? (UMaterialInterface*)GroundMID : (UMaterialInterface*)GrassBase);
    // A 2 mm paint film is the only surface layer. Physics and rendered turf agree at Z=0.
    for(int End:{-1,1})
    {
        Mark.Plate(-183,End*C26Field::CreaseY-2.5f,183,End*C26Field::CreaseY+2.5f,.2f);
        Mark.Plate(-132,End*C26Field::WicketY-2.5f,132,End*C26Field::WicketY+2.5f,.2f);
        for(int Side:{-1,1})Mark.Plate(Side*132-2.5f,End*884,Side*132+2.5f,End*1249,.2f);
    }
    Emit(Mark,Paint);
    Structure=true;Section=0;

    // Boundary rope, hoardings and the 30-yard ring.
    for(int I=0;I<128;++I)
    {
        const FVector A=C26Field::RopePoint(I*2*PI/128),B=C26Field::RopePoint((I+1)*2*PI/128);
        Beam(Rope,A,B,9);
        if(I%4==0) Add(Pegs, A+FVector(0,0,5), FVector(5,5,16), FRotator(0,0,0));
        const float Ang=2*PI*I/128;
        const bool Sight=FMath::Abs(FMath::Cos(Ang))<.20f;
        if(!Sight)
        {
            const FVector P=Oval(BoardRX,BoardRY,Ang,48);
            UHierarchicalInstancedStaticMeshComponent* ChosenBoard = (I%12<4)?Boards:((I%12<8)?BoardsAlt:BoardsGold);
            Add(ChosenBoard,P,FVector(330,18,86),FRotator(0,FMath::RadiansToDegrees(Ang)+90,0));
        }
        if(I%4==0)
        {
            const float A0=2*PI*I/128,A1=A0+.030f;
            Beam(Marks,FVector(2740*FMath::Cos(A0),2740*FMath::Sin(A0),6),FVector(2740*FMath::Cos(A1),2740*FMath::Sin(A1),6),4);
        }
    }
    auto* Cushions=Batch(TEXT("BoundaryCushions"),Mesh(TEXT("Boundary"),TEXT("BoundaryCushion")),Colour(FLinearColor(.037,.14,.16),0,.82f));
    for(int I=0;I<128;++I)
    {
        const float A=2*PI*(I+.5f)/128;
        Cushions->AddInstance(FTransform(FRotator(0,FMath::RadiansToDegrees(A)+90,0),Oval(C26Field::RadiusX+35,C26Field::RadiusY+35,A,0)));
    }
    // Sight screens behind each bowler's arm; the ball needs a clean background to read against.
    for(int End:{-1,1})
    {
        auto* ScreenBatch=Batch(TEXT("SightScreen"),Cube,Screen);
        Add(ScreenBatch,FVector(0,End*7780,355),FVector(1350,28,620));
        Add(Rails,FVector(0,End*7810,28),FVector(1500,64,56));
        for(int Post:{-1,1})Beam(Columns,FVector(Post*710,End*7810,0),FVector(Post*710,End*7810,710),14);
    }

    auto MakeSign=[&](const FString& Body,const FVector& At,const FRotator& Facing,float Size,FColor Glyph) -> UTextRenderComponent*
    {
        auto* Text=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transient);Text->SetupAttachment(RootComponent);
        Text->RegisterComponent();AddInstanceComponent(Text);Signs.Add(Text);
        Text->SetText(FText::FromString(Body));Text->SetWorldSize(Size);Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetVerticalAlignment(EVRTA_TextCenter);Text->SetTextRenderColor(Glyph);
        Text->SetRelativeLocation(At);Text->SetRelativeRotation(Facing);Text->SetCastShadow(false);
        return Text;
    };

    // ================= LOWER BOWL =================
    FC26Surface Con,Stl,Fac,Glass;
    Fac.Sweep(WallRX,WallRY,0.f,WallRX,WallRY,WallTop,false,Segments,48,1);
    Con.Sweep(WallRX,WallRY,WallTop,WallRX+40,WallRY+40,WallTop,true);        // Wall capping.
    for(int Row=0;Row<LowerRows;++Row)
    {
        const float R=Row*LowerDepth,Z=WallTop+15.f+Row*LowerRise;
        const bool Entrance=Row>=4&&Row<9;
        Con.Sweep(WallRX+R,WallRY+R,Z,WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z,true,Segments,Entrance?8:-1,1);
        Stl.Sweep(WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z,WallRX+R+LowerDepth,WallRY+R+LowerDepth,Z+LowerRise,false,Segments,Entrance?8:-1,1);
    }
    const float LowerBackRX=WallRX+LowerRows*LowerDepth,LowerBackRY=WallRY+LowerRows*LowerDepth;
    const float LowerBackZ=WallTop+15.f+LowerRows*LowerRise;
    // Concourse: solid band, glazed hospitality ribbon, then the upper bowl above it.
    Fac.Sweep(LowerBackRX,LowerBackRY,LowerBackZ,LowerBackRX,LowerBackRY,LowerBackZ+120.f,false);
    Glass.Sweep(LowerBackRX+100,LowerBackRY+100,LowerBackZ+120.f,LowerBackRX+100,LowerBackRY+100,LowerBackZ+300.f,false);
    Fac.Sweep(LowerBackRX,LowerBackRY,LowerBackZ+300.f,UpperRX,UpperRY,UpperZ,false);
    Con.Sweep(LowerBackRX,LowerBackRY,LowerBackZ,LowerBackRX+320,LowerBackRY+320,LowerBackZ,true);

    // ================= UPPER BOWL =================
    for(int Row=0;Row<UpperRows;++Row)
    {
        const float R=Row*UpperDepth,Z=UpperZ+Row*UpperRise;
        Con.Sweep(UpperRX+R,UpperRY+R,Z,UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z,true);
        Stl.Sweep(UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z,UpperRX+R+UpperDepth,UpperRY+R+UpperDepth,Z+UpperRise,false);
    }
    const float UpperBackRX=UpperRX+UpperRows*UpperDepth,UpperBackRY=UpperRows*UpperDepth;
    const float UpperBackZ=UpperZ+UpperRows*UpperRise;
    Fac.Sweep(UpperBackRX,UpperBackRY,UpperBackZ,UpperBackRX+260,UpperBackRY+260,UpperBackZ+520,false);

    // ================= ROOF =================
    FC26Surface Under,Top;
    for(int I=0;I<Segments;++I)
    {
        const float A=2*PI*I/Segments,B=2*PI*(I+1)/Segments;
        const float Fold0=FMath::Sin((I%8)*PI/8)*95,Fold1=FMath::Sin(((I%8)+1)*PI/8)*95;
        const FVector P0=Oval(UpperRX-320,UpperRY-320,A,RoofInnerZ+Fold0),P1=Oval(UpperRX-320,UpperRY-320,B,RoofInnerZ+Fold1);
        const FVector Q0=Oval(RoofOuterRX,RoofOuterRY,A,RoofOuterZ),Q1=Oval(RoofOuterRX,RoofOuterRY,B,RoofOuterZ);
        Under.Quad(P0,P1,Q1,Q0,FVector(0,0,-1),4.f,1.f);
        Top.Quad(P0+FVector(0,0,55),P1+FVector(0,0,55),Q1+FVector(0,0,55),Q0+FVector(0,0,55),FVector::UpVector,4.f,1.f);
    }
    Emit(Under,RoofUnder);Emit(Top,RoofTop);
    // Pale canopy lip and a thin original teal identity ribbon, with restrained emission.
    FC26Surface Fascia;
    Fascia.Sweep(UpperRX-320,UpperRY-320,RoofInnerZ-55,UpperRX-320,UpperRY-320,RoofInnerZ,false);
    Emit(Fascia,RoofTop);
    Emit(Con,Concrete);Emit(Stl,Steel);Emit(Fac,Facade);Emit(Glass,Glazing);

    // Roof trusses only, sitting on top of the canopy. An outer ring of ground-to-roof columns
    // reads as a forest of poles through the seating gaps from every camera, so there is none.
    for(int I=0;I<24;++I)
    {
        const float A=2*PI*I/24;
        Beam(Rails,Oval(UpperRX-260,UpperRY-260,A,RoofInnerZ+70),Oval(RoofOuterRX-140,RoofOuterRY-140,A,RoofOuterZ+70),22);
        Beam(Rails,Oval(UpperRX-260,UpperRY-260,A,RoofInnerZ-95),Oval(RoofOuterRX-140,RoofOuterRY-140,A,RoofOuterZ-95),14);
        for(int K=0;K<5;++K)
        {
            const float T=K/5.f,U=(K+1)/5.f;
            Beam(Rails,Oval(FMath::Lerp(UpperRX-260,RoofOuterRX-140,T),FMath::Lerp(UpperRY-260,RoofOuterRY-140,T),A,FMath::Lerp(RoofInnerZ,RoofOuterZ,T)+70),
                Oval(FMath::Lerp(UpperRX-260,RoofOuterRX-140,U),FMath::Lerp(UpperRY-260,RoofOuterRY-140,U),A,FMath::Lerp(RoofInnerZ,RoofOuterZ,U)-95),8);
        }
        // Glazing mullions articulate the recessed circulation ribbon.
        Add(Rails,Oval(LowerBackRX+50,LowerBackRY+50,A,LowerBackZ+205),FVector(34,34,290));
    }

    // Aisles and Vomitories: Real architectural portals with concourse depth, safety handrails and warm interior lighting.
    auto* Aisles=Batch(TEXT("ConcreteAisleSteps"),Cube,Colour(FLinearColor(.24,.235,.208),0,.95f));
    auto* ConcourseGlow=Batch(TEXT("ConcourseInteriorGlow"),Cube,Colour(FLinearColor(.38,.28,.16),0.95f,.75f));
    auto* PortalWalls=Batch(TEXT("VomitoryPortalWalls"),Cube,Concrete);
    auto* PortalTrim=Batch(TEXT("VomitoryLintelTrim"),Cube,Steel);

    for(int Tier=0;Tier<2;++Tier)for(int Bay=0;Bay<24;++Bay)
    {
        const float A=2*PI*(Bay+.0625f)/24;
        const float RX=Tier?UpperRX:WallRX,RY=Tier?UpperRY:WallRY;
        const float Z0=Tier?UpperZ:WallTop+15,Depth=Tier?UpperDepth:LowerDepth,Rise=Tier?UpperRise:LowerRise;
        const int Rows=Tier?UpperRows:LowerRows;
        const FRotator R(0,FMath::RadiansToDegrees(A),0);
        for(int S=0;S<Rows*2;++S)
        {
            if(!Tier&&S>=8&&S<18)continue;
            Add(Aisles,Oval(RX+(S+.5f)*Depth*.5f,RY+(S+.5f)*Depth*.5f,A,Z0+(S+.5f)*Rise*.5f),FVector(Depth*.5f,142,Rise*.5f),R);
            if(S%6==0)
            {
                const FVector P=Oval(RX+S*Depth*.5f,RY+S*Depth*.5f,A,Z0+S*Rise*.5f);
                Beam(Rails,P+R.RotateVector(FVector(0,80,0)),P+R.RotateVector(FVector(0,80,95)),5);
            }
        }
        const FVector Front=Oval(RX,RY,A,Z0+95),Back=Oval(RX+Rows*Depth,RY+Rows*Depth,A,Z0+Rows*Rise+95);
        Beam(Rails,Front+R.RotateVector(FVector(0,80,0)),Back+R.RotateVector(FVector(0,80,0)),5);
        if(!Tier)
        {
            const FVector PortalCenter = Oval(RX+9*Depth,RY+9*Depth,A,Z0+6*Rise);
            // Warm illuminated concourse back wall (eliminates dark black void)
            Add(ConcourseGlow, PortalCenter + R.RotateVector(FVector(25, 0, 0)), FVector(18, 230, 240), R);
            // Side concrete walls
            Add(PortalWalls, PortalCenter + R.RotateVector(FVector(0, -118, 0)), FVector(360, 20, 245), R);
            Add(PortalWalls, PortalCenter + R.RotateVector(FVector(0, 118, 0)), FVector(360, 20, 245), R);
            // Overhead lintel
            Add(PortalTrim, PortalCenter + R.RotateVector(FVector(-80, 0, 125)), FVector(160, 256, 18), R);
            // Railings & stairs
            for(int Side:{-1,1})Add(Rails,Oval(RX+7*Depth,RY+7*Depth,A,Z0+6*Rise)+R.RotateVector(FVector(0,Side*118,0)),FVector(400,18,245),R);
            Add(Aisles,Oval(RX+7*Depth,RY+7*Depth,A,Z0+9*Rise+18),FVector(460,256,35),R);
            // Luminous Gate Sign above tunnel
            MakeSign(FString::Printf(TEXT("GATE %02d"), Bay + 1), PortalCenter + R.RotateVector(FVector(-85, 0, 150)), FRotator(0, FMath::RadiansToDegrees(A) + 180, 0), 28, FColor(240, 230, 200));
        }
    }

    // ================= SEATING AND CROWD =================
    const int Density=FMath::Clamp(CrowdQuality,0,3);
    const float Pitch2=Density>=3?108.f:Density==2?142.f:Density==1?165.f:220.f;
    const int RowStep=Density>=2?1:2;
    FRandomStream Random(2626);
    // A crowd is the most colourful thing in a cricket ground, and at broadcast distance it is
    // what tells a viewer the stands are full.
    const FLinearColor Shirts[6]={
        FLinearColor(.085,.235,.470),FLinearColor(.560,.130,.095),FLinearColor(.520,.500,.440),
        FLinearColor(.105,.320,.175),FLinearColor(.450,.290,.055),FLinearColor(.300,.170,.420)};
    auto* CrowdBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Environment/Materials/M_Eclipse_Crowd.M_Eclipse_Crowd"));
    TArray<UHierarchicalInstancedStaticMeshComponent*> Bodies;
    for(int G=0;G<6;++G)
    {
        auto* M=Tinted(CrowdBase?CrowdBase:Surface,Shirts[G],0,.95f);CrowdMaterials.Add(M);
        M->SetVectorParameterValue(TEXT("Accent"),Shirts[(G+1)%6]);
        Bodies.Add(Batch(*FString::Printf(TEXT("CrowdGroup%d"),G),Person,M));
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
                const float A=2*PI*(J+Random.FRandRange(-.17f,.17f))/Count;
                const float BayFrac=FMath::Frac(A/(2*PI)*24);
                if(BayFrac<.135f||BayFrac>.985f)continue;
                const int32 BayIdx=FMath::FloorToInt(FMath::Frac(A/(2*PI))*24)%24;
                // Pavilion replaces a small portion of the lower seating at square leg.
                if(RX==WallRX&&FMath::Abs(FMath::UnwindRadians(A))<.14f&&Row>6)continue;
                const FRotator Face(0,FMath::RadiansToDegrees(A)+90.f,0);
                const FVector Seat=Oval(RadX+Depth*.5f,RadY+Depth*.5f,A,Z);
                if(Row%2==0)(int(A*24/(2*PI))%3?SeatsA:SeatsB)->AddInstance(FTransform(Face,Seat));
                if(Random.FRand()<.065f)continue;
                // Clustered demographic selection:
                int G = 0;
                if (BayIdx == 0 || BayIdx == 1 || BayIdx == 2 || BayIdx == 22 || BayIdx == 23)
                {
                    // Home supporter bay: heavily weighted to Championship Navy & Gold
                    G = (Random.FRand() < 0.65f) ? 0 : (Random.FRand() < 0.5f ? 2 : 4);
                }
                else if (BayIdx == 10 || BayIdx == 11 || BayIdx == 12 || BayIdx == 13 || BayIdx == 14)
                {
                    // Away supporter bay: heavily weighted to Cricket Crimson & White
                    G = (Random.FRand() < 0.60f) ? 1 : (Random.FRand() < 0.5f ? 2 : 5);
                }
                else
                {
                    // Neutral / family bays: organic general mix
                    G = Random.RandRange(0, 5);
                }
                const float Size=Random.FRandRange(.88f,1.08f);
                Bodies[G]->AddInstance(FTransform(FRotator(Random.FRandRange(-2.5f,2.5f),Face.Yaw+Random.FRandRange(-10.f,10.f),0),Seat,FVector(Size)));
            }
        }
    };
    PopulateTier(WallRX,WallRY,WallTop+15.f,LowerRows,LowerDepth,LowerRise);
    PopulateTier(UpperRX,UpperRY,UpperZ,UpperRows,UpperDepth,UpperRise);

    // ================= FLOODLIGHT PYLONS =================
    auto* Towers=Batch(TEXT("FloodlightLatticeTowers"),Mesh(TEXT("Floodlights"),TEXT("FloodlightTower")),Steel);
    auto* Housings=Batch(TEXT("FloodlightLampHousings"),Mesh(TEXT("Floodlights"),TEXT("LampHousing")),Facade);
    auto* Arrays=Batch(TEXT("FloodlightLampArrays"),Mesh(TEXT("Floodlights"),TEXT("LampArray")),LampMaterial);
    Towers->SetCastShadow(Density>=2);
    for(int I=0;I<6;++I)
    {
        const float A=PI/6+2*PI*I/6;
        const FTransform T(FRotator(0,FMath::RadiansToDegrees(A)+90,0),Oval(10800,11500,A,0));
        Towers->AddInstance(T);Housings->AddInstance(T);Arrays->AddInstance(T);
    }
    // Continuous roof-edge lighting rig: a bright, even wash that reads across the whole bowl.
    for(int I=0;I<Segments;I+=4)
    {
        const float A=2*PI*I/Segments;
        Add(Lamps,Oval(UpperRX-330,UpperRY-330,A,RoofInnerZ-235),FVector(150,26,34),FRotator(18,FMath::RadiansToDegrees(A)+90,0));
    }

    // ================= SIGNAGE AND SCREENS =================
    // Square-leg pavilion: recessed glazing, projecting balconies and a slatted media crown.
    auto* PavilionStone=Batch(TEXT("PavilionStone"),Cube,Colour(FLinearColor(.30,.285,.245),0,.88f));
    auto* PavilionGlass=Batch(TEXT("PavilionGlass"),Cube,Glazing);
    for(int Floor=0;Floor<3;++Floor)
    {
        const float Z=680+Floor*310;
        Add(PavilionGlass,FVector(8310,0,Z),FVector(45,2760,260));
        Add(PavilionStone,FVector(8440,0,Z+148),FVector(940,3180,52));
        Add(Rails,FVector(7945,0,Z+20),FVector(14,3000,24));
        for(int J=-5;J<=5;++J)
        {
            Add(PavilionStone,FVector(8280,J*255,Z),FVector(100,26,275));
            Beam(Rails,FVector(7935,J*270,Z-95),FVector(7935,J*270,Z+28),5);
        }
    }
    Add(PavilionStone,FVector(8440,0,1750),FVector(1220,3400,95));
    for(int J=-15;J<=15;++J)Add(Rails,FVector(7990,J*105,1580),FVector(44,24,200));
    MakeSign(TEXT("ECLIPSE PAVILION"),FVector(7800,0,1700),FRotator(0,180,0),110,FColor(231,222,195));

    // Player access at the pavilion end, kept outside the sight-screen corridor.
    Add(PortalWalls,FVector(1270,-7970,150),FVector(310,360,300));
    Add(PavilionStone,FVector(1270,-7780,315),FVector(400,60,55));
    for(int Side:{-1,1})Add(PavilionStone,FVector(1270+Side*190,-7780,145),FVector(45,60,290));
    MakeSign(TEXT("PLAYERS"),FVector(1270,-7740,310),FRotator(0,90,0),49,FColor(214,226,221));
    auto* Technical=Batch(TEXT("TechnicalAreaCanopies"),Cube,RoofTop);
    for(int Side:{-1,1})
    {
        const float X=Side*2090;
        Add(Technical,FVector(X,-7460,255),FVector(660,285,22));
        for(int Post:{-1,1})Beam(Rails,FVector(X+Post*306,-7550,0),FVector(X+Post*306,-7550,255),12);
        Add(Rails,FVector(X,-7490,48),FVector(530,55,24));
    }
    // ================= STADIUM LIFE =================
    // Background life where the broadcast cameras actually look. All static instanced geometry on
    // existing meshes/materials: photographers crouched behind the rope at both ends, raised camera
    // platforms on the diagonals, benches + seated team groups under the dugout canopies, hi-vis
    // stewards spaced along the wall. No ticking actors, no skeletal meshes.
    {
        auto* Crew = Batch(TEXT("MediaCrew"), Person, Colour(FLinearColor(.05f,.06f,.08f),0,.9f));
        auto* Vests = Batch(TEXT("Stewards"), Person, Colour(FLinearColor(.55f,.60f,.12f),0,.85f));
        auto* Subs = Batch(TEXT("DugoutGroups"), Person, Colour(FLinearColor(.10f,.16f,.24f),0,.9f));
        auto* Benches = Batch(TEXT("DugoutBenches"), Cube, Colour(FLinearColor(.10f,.11f,.12f),0,.8f));
        auto* Decks = Batch(TEXT("CameraPlatforms"), Cube, Steel);
        for (int End : {-1, 1})
            for (int K = -3; K <= 3; ++K)
            {
                const float A = End * PI * .5f + K * .09f;
                const FRotator Face(0, FMath::RadiansToDegrees(A) + 90.f, 0);
                const FVector At = Oval(C26Field::RadiusX + 130, C26Field::RadiusY + 130, A, 0);
                if (Crew && Crew->GetStaticMesh()) Crew->AddInstance(FTransform(Face, At, FVector(.72f)));
            }
        for (int K = 0; K < 4; ++K)
        {
            const float A = PI / 4 + K * PI / 2;
            const FVector Base = Oval(8200, 8900, A, 0);
            Add(Decks, Base + FVector(0, 0, 420), FVector(260, 260, 26));
            Add(Decks, Base + FVector(0, 0, 478), FVector(60, 44, 44));
            for (int Leg : {-1, 1})
                for (int Leg2 : {-1, 1})
                    Beam(Rails, Base + FVector(Leg * 115, Leg2 * 115, 0), Base + FVector(Leg * 115, Leg2 * 115, 420), 10);
        }
        for (int Side : {-1, 1})
        {
            const float X = Side * 2090;
            Add(Benches, FVector(X - 150, -7360, 40), FVector(300, 60, 45));
            Add(Benches, FVector(X + 150, -7360, 40), FVector(300, 60, 45));
            for (int K = 0; K < 5; ++K)
            {
                const FRotator Face(0, -90.f, 0);
                if (Subs && Subs->GetStaticMesh())
                {
                    Subs->AddInstance(FTransform(Face, FVector(X - 260 + K * 105, -7360, 62), FVector(.95f)));
                    Subs->AddInstance(FTransform(Face, FVector(X - 260 + K * 105, -7240, 0), FVector(1.f)));
                }
            }
        }
        for (int I = 0; I < 16; ++I)
        {
            const float A = 2 * PI * I / 16 + .19f;
            const FRotator Face(0, FMath::RadiansToDegrees(A) + 90.f, 0);
            if (Vests && Vests->GetStaticMesh())
                Vests->AddInstance(FTransform(Face, Oval(WallRX - 60, WallRY - 60, A, 0), FVector(1.1f)));
        }
    }
    for(int I=0;I<24;++I)
    {
        const float A=2*PI*(I+.45f)/24;
        MakeSign(I%3==0?TEXT("CRICKET 26"):I%3==1?TEXT("ECLIPSE OVAL"):TEXT("THE NIGHT SESSION"),Oval(BoardRX-18,BoardRY-18,A,55),
            FRotator(0,FMath::RadiansToDegrees(A)+180,0),39,FColor(213,225,216));
        const float ExitA=2*PI*(I+.0625f)/24;
        MakeSign(FString::Printf(TEXT("%02d"),I+1),Oval(WallRX+520,WallRY+520,ExitA,WallTop+9*LowerRise+70),
            FRotator(0,FMath::RadiansToDegrees(ExitA)+180,0),40,FColor(234,222,183));
    }
    for(int I=0;I<10;++I)
    {
        const float A=2*PI*I/10+.31f;
        MakeSign(I%2?TEXT("ECLIPSE OVAL"):TEXT("CRICKET 26"),Oval(LowerBackRX-24,LowerBackRY-24,A,LowerBackZ+210.f),
            FRotator(0,FMath::RadiansToDegrees(A)+180,0),76.f,FColor(196,210,207));
    }
    // A live video wall is one of the few things in a night ground that is brighter than the field.
    JumbotronSigns.Reset();
    auto* Panels=Batch(TEXT("BroadcastScreens"),Cube,Colour(FLinearColor(.026,.070,.092),2.10f,.25f));
    for(int I:{-1,1})
    {
        const FVector At(I*1780.f,I*(UpperRY+750.f),UpperZ+1470.f);
        Add(Panels,At,FVector(1920,60,850));
        Add(Rails,At+FVector(0,0,-454),FVector(2020,90,70));
        auto* ScreenSign = MakeSign(TEXT("CRICKET 26\nECLIPSE OVAL"),At+FVector(0,I*-38.f,0),FRotator(0,I>0?-90.f:90.f,0),155.f,FColor(206,236,229));
        if (ScreenSign) JumbotronSigns.Add(ScreenSign);
    }

    // ================= NIGHT SKY =================
    // Layered dome: warm city glow at the horizon fading to deep navy overhead, so the roof line
    // and the pylons separate cleanly instead of dissolving into a flat black void. Lifted from
    // the original near-black ramp: the horizon band was reading as a hard cut against the roof.
    const FLinearColor SkyRamp[6]={
        FLinearColor(.0320,.0510,.0880),FLinearColor(.0225,.0365,.0660),FLinearColor(.0140,.0235,.0460),
        FLinearColor(.0080,.0140,.0305),FLinearColor(.0042,.0078,.0180),FLinearColor(.0022,.0042,.0105)};
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
        auto* SkyMID = Tinted(Mat(TEXT("M_Sky"))?Mat(TEXT("M_Sky")):Surface,SkyRamp[Layer],1.f,.98f);
        Sky->SetMaterial(Layer,SkyMID);
        SkyMaterials.Add(SkyMID);
    }
    (void)White;(void)VenueTeal;(void)Navy;(void)GrassBase;(void)PitchBase;
    for(auto& B:Batches){B->bAutoRebuildTreeOnInstanceChanges=true;B->BuildTreeIfOutdated(true,true);}
    int Instances=0;for(auto& B:Batches)Instances+=B->GetInstanceCount();
    int CrowdCount=0;for(auto& B:Bodies)CrowdCount+=B->GetInstanceCount();
    UE_LOG(LogC26Venue,Display,TEXT("C26_WORLD scale: wicket_spacing=%.1f strip=%.1fx%.1f stump=%.1f width=%.2f ball=%.1f boundary=%.1fx%.1fm"),
        C26Field::WicketY*2,C26Field::PitchWidth,C26Field::PitchStripLength,C26Field::StumpHeight,C26Field::WicketWidth,C26Field::BallDiameter,C26Field::RadiusX/100,C26Field::RadiusY/100);
    UE_LOG(LogC26Venue,Display,TEXT("C26_WORLD quality=%d crowd=%d towers=%d ground_sections=%d architecture_sections=%d"),CrowdQuality,CrowdCount,Towers->GetInstanceCount(),Bowl->GetNumSections(),Architecture->GetNumSections());
    UE_LOG(LogC26Venue,Display,TEXT("Eclipse Oval built: %d mesh sections, %d batches, %d instances."),Section,Batches.Num(),Instances);
    UE_LOG(LogC26Venue,Display,TEXT("Venue surfaces: bowl=%s sections=%d materials=%d sky=%s sections=%d"),
        *Bowl->GetName(),Bowl->GetNumSections(),Bowl->GetNumMaterials(),*Sky->GetName(),Sky->GetNumSections());
}
void AC26Stadium::SetQuality(int Level)
{
    const int Wanted=FMath::Clamp(Level,0,3);
    const bool Rebuild=Wanted!=CrowdQuality;
    CrowdQuality=Wanted;
    // Athlete shadows are the difference between a player standing on the ground and a sprite
    // pasted over it, so they survive all the way down to the lowest tier -- what scales instead is
    // the cascade count and the distance the cascades reach.
    KeyLight->SetCastShadows(true);KeyLight->DynamicShadowCascades=Level<2?2:3;
    KeyLight->DynamicShadowDistanceMovableLight=Level<2?4500:9000;
    KeyLight->ContactShadowLength=Level>0?.045f:0.f;
    if(CrossLight)CrossLight->SetVisibility(true);
    // Far seating treads are smaller than a cascade texel and self-shadow into moire.
    // Model their recesses in geometry/material; reserve dynamic maps for the playing area.
    if(Architecture)Architecture->SetCastShadow(false);
    ApplyLightVisibility();
    if(Rebuild&&HasActorBegunPlay()){BuildVenue();BuildLightShafts();SetEnvironment(EnvironmentProfile);SetPitchCondition(PitchCondition);}
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
    // Named states ease the visible excitement toward their own target instead of only decaying:
    // a six holds the ground on its feet, tension lingers, calm settles quickly.
    if (CrowdReaction < CrowdTarget) CrowdReaction = FMath::FInterpTo(CrowdReaction, CrowdTarget, Step, CrowdRate);
    else CrowdReaction = FMath::Max(CrowdTarget, CrowdReaction - Step * .42f);
    const float Pulse=FMath::Max(0.f,FMath::Sin(Time*4.1f));
    LEDSpike = FMath::Max(0.f, LEDSpike - Step * .55f);
    if(LED)LED->SetScalarParameterValue(TEXT("Glow"),.42f+CrowdReaction*Pulse*.18f+LEDSpike*.9f);
    // Hand the same reaction level to the crowd shader. The spectators' rise is vertex motion with
    // a per-instance random phase, so this one scalar is the whole cost of a ground that gets to
    // its feet for a six and settles again over the next couple of seconds.
    for(auto& M:CrowdMaterials)if(M)M->SetScalarParameterValue(TEXT("Excitement"),CrowdReaction);
    // Lamp exposure and crowd albedo remain stable during score events.
}

void AC26Stadium::UpdateJumbotron(const FString& Line1, const FString& Line2, const FLinearColor& Color)
{
    const FString Combined = Line2.IsEmpty() ? Line1 : FString::Printf(TEXT("%s\n%s"), *Line1, *Line2);
    const FColor RenderColor = Color.ToFColor(true);
    for (auto& Sign : JumbotronSigns)
    {
        if (Sign)
        {
            Sign->SetText(FText::FromString(Combined));
            Sign->SetTextRenderColor(RenderColor);
        }
    }
}

