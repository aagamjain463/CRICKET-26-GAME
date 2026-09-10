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
    /** Photorealistic hero scan mesh representing the authentic athlete model */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeroMesh;
    /** Authored hero kit from /Game/Cricket26/Equipment, built in ArtSource/Blender/Equipment.
        Every one of these was a procedural ring-loft generated in this file until Milestone 2:
        a ten-sided bat, an engine sphere for a helmet, tubes for pads. The authored meshes carry
        the shapes that actually identify cricket equipment -- a blade with a spine and blunt
        edges, a grille, three pad bolsters, segmented finger rolls -- which no amount of material
        work on a ten-sided ellipse can produce. Each is posed in exactly the local frame it was
        authored in, documented at the top of its build script. */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Headwear;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Grill;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PadR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GloveL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GloveR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ShoeL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ShoeR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Uniform;
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
    bool NonStriker=false;
    float Trigger=0.f;
    float GaitPhase=0.f;
    int SkipPhase=0;
    FVector ReceivingPosition() const;
    EC26Delivery DeliveryStyle=EC26Delivery::Pace;
    FVector ContactTarget=FVector::ZeroVector;
    /** Optional world point for the head to track. Zero disables head aim. */
    FVector LookAt=FVector::ZeroVector;
    void Configure(EC26Role NewRole,int Team,int Number);
    /** Presentation budget for this athlete. Hero is the striker, the bowler, the keeper and any
        fielder inside the working area of the shot; everything else drops detail the camera
        cannot resolve. Set every frame from the live view point, so a fielder who runs into the
        play is promoted rather than staying cheap for the whole over. */
    enum class EDetail : uint8 { Hero, Mid, Distant };
    EDetail Detail=EDetail::Hero;
    void UpdateDetail(const FVector& ViewPoint);
    void SetAction(EC26Action NewAction,bool ResetTime=true);
    void Animate(float Dt);
    void ResetAt(const FVector& Position,float Yaw);
    FVector HandPosition() const;
    void SetShotContact(const FVector& Target,float Angle,bool bLoft);
    int TeamId=0;
    /** Squad number driving shirt text and deterministic kit variation. */
    int32 SquadNumber=0;
    /** True while this athlete shows a baked hero scan; false renders the animated team kit. */
    bool bHeroVisual=false;
    /** Enforce exactly one visible body: hero scan or animated kit, never both, never none. */
    void ApplyVisualRole();
    /** Real-world height in centimetres the imported rig is scaled down to. */
    static constexpr float BodyHeight=185.f;
    /** Distance in centimetres beyond which an athlete stops being hero quality, then stops
        carrying small kit at all. A delivery is watched from about 900 cm behind the striker. */
    static constexpr float HeroRange=2600.f;
    static constexpr float MidRange=6000.f;
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
    float PalmReach=11.f;
    bool AuthoredKit=false;
    FVector Palm(bool Right) const;
    static FVector Rig(float Forward,float Right,float Up){return FVector(-Right,Forward,Up);}
    void AimHead();
    TArray<int32> Parents;
    TMap<FString,int32> Bones;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Shirt;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Trousers;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Gear;
    /** Readable skin tone. The imported Bodymat renders as a dark mass under the night rig; a
        flat M_Surface tone in a mid-brown keeps the face, neck and forearms readable at every
        camera distance. Eyes share the Body slot, so they take the same tone at this fidelity. */
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Skin;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShadeMaterial;
    int Bone(const FString& Name) const;
    void RebuildChildren(int Index);
    void Aim(const FString& Name,const FString& Child,const FVector& Target);
    void Twist(const FString& Name,float Yaw,float Pitch=0.f,float Roll=0.f);
    void Limb(const FString& Upper,const FString& Lower,const FString& End,const FVector& Target,const FVector& Bend);
    void MoveBone(const FString& Name,const FVector& Offset);
    void BuildContactShadow();
    void ApplyDetail();
    void UpdateContactShadow();
    void PlaceKit(const FVector& Grip,const FVector& Toe,bool Batting,bool Running);
    /** Bind a dynamic instance to every slot on a static mesh whose name contains Key. Slot
        order is decided by the importer, not by the authoring script -- Unreal drops material
        slots no triangle references -- so equipment materials are looked up by name and never
        by index. */
    void Dress(UStaticMeshComponent* Part,const TCHAR* Key,UMaterialInstanceDynamic* M);
    EDetail Wanted=EDetail::Hero;
    void UpdateUniform();
};
