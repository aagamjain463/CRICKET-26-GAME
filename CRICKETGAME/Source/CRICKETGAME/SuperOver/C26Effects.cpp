#include "C26Effects.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogC26Effects,Log,All);

AC26Effects::AC26Effects()
{
    PrimaryActorTick.bCanEverTick=false;
    Sheet=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EffectSheet"));
    RootComponent=Sheet;
    Sheet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sheet->SetCastShadow(false);
    Sheet->bUseComplexAsSimpleCollision=false;
    // Billboards are rebuilt every frame from world-space positions, so the component must never
    // let the renderer cull them against a stale local bound.
    Sheet->SetBoundsScale(12.f);
}
void AC26Effects::Clear()
{
    Puffs.Reset();
}
void AC26Effects::Emit(int Count,const FVector& At,const FVector& Bias,float Speed,float Spread,
    const FLinearColor& Colour,float Size,float Growth,float Life,float Gravity,float Drag,float Peak)
{
    for(int I=0;I<Count&&Puffs.Num()<Capacity;++I)
    {
        FC26Puff P;
        P.Position=At+Random.VRand()*Size*.45f;
        P.Velocity=(Bias+Random.VRand()*Spread).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector)
            *Speed*Random.FRandRange(.55f,1.35f);
        P.Colour=Colour;
        P.Life=Life*Random.FRandRange(.75f,1.25f);
        P.Size=Size*Random.FRandRange(.70f,1.30f);
        P.Growth=Growth;P.Gravity=Gravity;P.Drag=Drag;
        P.Peak=Peak*Random.FRandRange(.75f,1.f);
        Puffs.Add(P);
    }
}
void AC26Effects::PitchDust(const FVector& At,float Strength)
{
    // A ball pitching on a dry, prepared surface throws a low, wide, short-lived scatter of dust.
    // Anything resembling a plume immediately reads as a game effect rather than cricket.
    const float S=FMath::Clamp(Strength,0.f,1.f);
    Emit(FMath::RoundToInt(5.f+S*7.f),At+FVector(0,0,2.f),FVector(0,0,.55f),44.f+S*74.f,.85f,
        FLinearColor(.42f,.35f,.24f),5.5f+S*3.f,16.f,.40f+S*.16f,-150.f,2.1f,.30f+S*.22f);
}
void AC26Effects::FootPlant(const FVector& At,float Strength)
{
    // Heavier and slower than the ball mark: the bowler's braced front foot drives dust sideways.
    const float S=FMath::Clamp(Strength,0.f,1.f);
    Emit(FMath::RoundToInt(4.f+S*5.f),At+FVector(0,0,3.f),FVector(0,0,.30f),30.f+S*40.f,1.f,
        FLinearColor(.40f,.34f,.25f),7.f+S*3.5f,22.f,.55f,-120.f,2.6f,.24f+S*.14f);
}
void AC26Effects::TurfScuff(const FVector& At,const FVector& Along,float Strength)
{
    const float S=FMath::Clamp(Strength,0.f,1.f);
    Emit(FMath::RoundToInt(3.f+S*5.f),At+FVector(0,0,2.f),(Along.GetSafeNormal2D()*.75f+FVector(0,0,.5f)),
        60.f+S*90.f,.55f,FLinearColor(.20f,.27f,.13f),4.f+S*2.f,7.f,.34f,-260.f,1.6f,.34f);
}
void AC26Effects::StumpBurst(const FVector& At)
{
    // Bright, fast and gone inside a third of a second, timed to read under a slow-motion replay.
    Emit(14,At+FVector(0,0,26.f),FVector(0,0,.35f),210.f,1.f,
        FLinearColor(.62f,.58f,.46f),4.5f,10.f,.30f,-420.f,1.4f,.46f);
}
void AC26Effects::Advance(float Dt,const FVector& ViewRight,const FVector& ViewUp,const FVector& ViewForward)
{
    if(!Built)
    {
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cricket26/Materials/M_Particle.M_Particle"));
        if(!Base)
        {
            // Better no dust at all than a field full of opaque grey squares. Run
            // Tools/BuildEffectAssets.py to author the material, then this lights up on its own.
            UE_LOG(LogC26Effects,Warning,TEXT("M_Particle missing; effects disabled"));
            Sheet->SetVisibility(false);Built=true;return;
        }
        Paint=UMaterialInstanceDynamic::Create(Base,this);
        Sheet->SetMaterial(0,Paint);
        Built=true;
    }
    if(!Paint)return;
    for(int I=Puffs.Num()-1;I>=0;--I)
    {
        FC26Puff& P=Puffs[I];
        P.Age+=Dt;
        if(P.Age>=P.Life){Puffs.RemoveAtSwap(I,EAllowShrinking::No);continue;}
        P.Velocity.Z+=P.Gravity*Dt;
        P.Velocity*=FMath::Exp(-P.Drag*Dt);
        P.Position+=P.Velocity*Dt;
    }
    // A fixed-capacity buffer keeps the vertex count constant so the section can be updated rather
    // than recreated; spare quads collapse to a point and cover no pixels.
    TArray<FVector> Vertices,Normals;TArray<int32> Indices;TArray<FVector2D> UV;
    TArray<FLinearColor> Colours;TArray<FProcMeshTangent> Tangents;
    Vertices.SetNumUninitialized(Capacity*4);Normals.SetNumUninitialized(Capacity*4);
    UV.SetNumUninitialized(Capacity*4);Colours.SetNumUninitialized(Capacity*4);
    const FVector Facing=-ViewForward;
    for(int I=0;I<Capacity;++I)
    {
        FVector Corner[4]={FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector};
        FLinearColor Tone=FLinearColor(0,0,0,0);
        if(Puffs.IsValidIndex(I))
        {
            const FC26Puff& P=Puffs[I];
            const float T=FMath::Clamp(P.Age/FMath::Max(.001f,P.Life),0.f,1.f);
            // Snap in, then fall away: dust that fades symmetrically reads as a cross-dissolve.
            const float Fade=T<.12f?T/.12f:FMath::Pow(1.f-(T-.12f)/.88f,1.4f);
            const float Radius=P.Size+P.Growth*P.Age;
            const FVector R=ViewRight*Radius,U=ViewUp*Radius;
            Corner[0]=P.Position-R-U;Corner[1]=P.Position+R-U;
            Corner[2]=P.Position+R+U;Corner[3]=P.Position-R+U;
            Tone=FLinearColor(P.Colour.R,P.Colour.G,P.Colour.B,Fade*P.Peak);
        }
        const int V=I*4;
        for(int J=0;J<4;++J){Vertices[V+J]=Corner[J];Normals[V+J]=Facing;Colours[V+J]=Tone;}
        UV[V]=FVector2D(0,0);UV[V+1]=FVector2D(1,0);UV[V+2]=FVector2D(1,1);UV[V+3]=FVector2D(0,1);
    }
    if(Sheet->GetNumSections()==0)
    {
        Indices.Reserve(Capacity*6);
        for(int I=0;I<Capacity;++I)
        {
            const int V=I*4;
            Indices.Append({V,V+1,V+2,V,V+2,V+3});
        }
        Sheet->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colours,Tangents,false);
        Sheet->SetMaterial(0,Paint);
    }
    else Sheet->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colours,Tangents);
}
