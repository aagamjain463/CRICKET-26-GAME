#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"
#include "C26Types.h"
#include "C26Athlete.generated.h"
class UProceduralMeshComponent;
class UStaticMeshComponent;

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
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Grill;
    EC26Role Role=EC26Role::Fielder;
    EC26Action Action=EC26Action::Ready;
    float ActionTime=0,MotionTime=0,ShotAngle=0;
    bool Loft=false;
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
    // The Mixamo import is ~378 cm tall and faces +Y. Unit converts real centimetres into rig
    // units; Rig() converts (forward, right, up) in real centimetres into rig-space positions.
    float Unit=1.f,ShoulderZ=0,HipZ=0,AnkleZ=0,StanceYaw=0;
    FVector Rig(float Forward,float Right,float Up) const { return FVector(-Right,Forward,Up)*Unit; }
    void AimHead();
    TArray<int32> Parents;
    TMap<FString,int32> Bones;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Kit;
    int Bone(const FString& Name) const;
    void RebuildChildren(int Index);
    void Aim(const FString& Name,const FString& Child,const FVector& Target);
    void Limb(const FString& Upper,const FString& Lower,const FString& End,const FVector& Target,const FVector& Bend);
    void MoveBone(const FString& Name,const FVector& Offset);
    void BuildEquipment();
    void BuildBat();
    void BuildPads();
};
