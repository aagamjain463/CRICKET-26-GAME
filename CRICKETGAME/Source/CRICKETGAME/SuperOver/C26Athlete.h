#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"
#include "C26Types.h"
#include "C26Athlete.generated.h"
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CRICKETGAME_API UC26PoseMesh : public UPoseableMeshComponent
{
    GENERATED_BODY()
public:
    void ApplyComponentPose(const TArray<FTransform>& Pose);
};

UCLASS()
class CRICKETGAME_API AC26Athlete : public AActor
{
    GENERATED_BODY()
public:
    AC26Athlete();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UC26PoseMesh> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Bat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Helmet;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Peak;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GloveL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GloveR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Grill;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Uniform;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Shell;
    /** Ground contact shadow. Drawn as its own alpha-blended patch rather than relying purely on
        the cascaded shadow map: it is guaranteed on every renderer and every quality tier, it
        costs one 40-triangle fan, and it is what stops a player reading as a decal floating over
        the turf. Real CSM shadowing layers on top of it wherever the tier can afford it. */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Shade;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> ShirtNumber;
    EC26Role Role=EC26Role::Fielder;
    EC26Action Action=EC26Action::Ready;
    float ActionTime=0,MotionTime=0,ShotAngle=0;
    bool Loft=false;
    float FootworkIntent=0.f;
    float StrideIntent=0.f;
    /** Ground speed in cm/s, so the stride frequency matches the distance actually covered. */
    float MoveSpeed=0.f;
    bool Defending=false;
    EC26Delivery DeliveryStyle=EC26Delivery::Pace;
    FVector ContactTarget=FVector::ZeroVector;
    /** Optional world point for the head to track. Zero disables head aim. */
    FVector LookAt=FVector::ZeroVector;
    void Configure(EC26Role NewRole,int Team,int Number);
    void SetAction(EC26Action NewAction,bool ResetTime=true);
    void Animate(float Dt);
    void ResetAt(const FVector& Position,float Yaw);
    FVector HandPosition() const;
    void SetShotContact(const FVector& Target,float Angle,bool bLoft);
    int TeamId=0;
    /** Real-world height in centimetres the imported rig is scaled down to. */
    static constexpr float BodyHeight=185.f;
private:
    TArray<FTransform> Reference,Pose;
    // The Mixamo rig is a T-pose facing mesh +Y with mesh +X out to the character's LEFT.
    // Rig() is the only conversion used for posing: it turns (forward, right, up) in real
    // centimetres into that mesh space, so every authored target below reads as cricket
    // directions rather than raw axes.
    float ShoulderZ=0,HipZ=0,AnkleZ=0;
    /** Shoulder-to-wrist reach in centimetres. Grip targets are clamped to it so the two-bone IK
        never runs out of arm and leaves the hands short of the handle. */
    float ArmSpan=0;
    static FVector Rig(float Forward,float Right,float Up){return FVector(-Right,Forward,Up);}
    void AimHead();
    TArray<int32> Parents;
    TMap<FString,int32> Bones;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Shirt;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Trousers;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Gear;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShadeMaterial;
    int Bone(const FString& Name) const;
    void RebuildChildren(int Index);
    void Aim(const FString& Name,const FString& Child,const FVector& Target);
    void Twist(const FString& Name,float Yaw,float Pitch=0.f,float Roll=0.f);
    void Limb(const FString& Upper,const FString& Lower,const FString& End,const FVector& Target,const FVector& Bend);
    void MoveBone(const FString& Name,const FVector& Offset);
    void BuildEquipment();
    void BuildContactShadow();
    void UpdateContactShadow();
    void PlaceKit(const FVector& Grip,const FVector& Toe,bool Batting,bool Running);
    void UpdateUniform();
};
