#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"
#include "C26Types.h"
#include "C26Athlete.generated.h"
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UAnimSequence;

UCLASS()
class CRICKETGAME_API UC26PoseMesh : public UPoseableMeshComponent
{
    GENERATED_BODY()
public:
    void ApplyComponentPose(const TArray<FTransform>& Pose);
    /** Push an already parent-relative pose straight at the skin. The athlete's smoothing filter
        works in local space -- component-space interpolation changes bone lengths mid-blend -- so
        it holds the displayed pose in that form and there is nothing left to convert. */
    void ApplyLocalPose(const TArray<FTransform>& Local);
};

UCLASS()
class CRICKETGAME_API AC26Athlete : public AActor
{
    GENERATED_BODY()
public:
    AC26Athlete();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UC26PoseMesh> Mesh;
    /** Legacy component retained for serialized levels; live players use the skinned mesh. */
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
    /** Point Mesh at this role's photoreal hero scan and give it that scan's own baked material.
        Called from Configure, so an innings change re-runs it and no stale body survives. */
    void ApplyHeroScan();
    /** True when a candidate body's geometry actually occupies its own rig's bind pose. A mesh
        that fails this cannot be animated -- its bones are outside its own geometry -- so binding
        it produces a body that tears, freezes, or sinks through the ground however correct the
        posing code is. Measured from the asset, not assumed. */
    static bool MatchesBindPose(USkeletalMesh* Candidate);
    /** The scan currently bound to Mesh, kept alive against GC and used to detect a no-op rebind. */
    UPROPERTY() TObjectPtr<USkeletalMesh> ScanAsset;
    /** True once ApplyHeroScan has bound a scan. Distinguishes the scanned body from the
        fallback Mixamo kit mesh, which is what Mesh carries if the scans are unavailable. */
    bool bScanVisual=false;
    /** Real-world height in centimetres the imported rig is scaled down to. */
    static constexpr float BodyHeight=185.f;
    /** Distance in centimetres beyond which an athlete stops being hero quality, then stops
        carrying small kit at all. A delivery is watched from about 900 cm behind the striker. */
    static constexpr float HeroRange=2600.f;
    static constexpr float MidRange=6000.f;
private:
    TArray<FTransform> Reference,Pose;
    /** The pose actually on screen, parent-relative, carried between frames. Every pose in this
        class is authored as an instantaneous target -- a stance, a contact, a gather -- and until
        this existed the skin was snapped onto whichever target the current action named, so every
        action change was a one-frame jump and every LOD-skipped frame was a freeze. The displayed
        pose now chases the target with a time constant instead, which is what turns a list of
        authored positions into motion. */
    TArray<FTransform> Shown;
    /** Ground speed as the legs see it. The match code can change MoveSpeed instantly; a stride
        length that changes instantly is a skate. */
    float ShownSpeed=0.f;
    /** How long the displayed pose is given to catch the authored target, in seconds. Actions
        whose timing is load-bearing -- the bat arriving at the ball, the ball leaving the hand --
        get a short one so the authored instant is still the authored instant. */
    float PoseLag() const;
    /** The pose the solver last asked for, parent-relative. Held separately from the displayed
        pose so a frame that skips the solve still has something to move toward. */
    TArray<FTransform> Goal;
    void SmoothPose(float Dt,bool Solved);
    /** Rotate the collarbone a fraction of the way toward a hand target before the arm solves.
        A shoulder socket that never moves is why overhead reaches look like a doll's: the real
        joint contributes most of the last 20 degrees of reach. */
    void ShoulderReach(const FString& Side,const FVector& Target,float Amount);
    /** Close the finger chains by rotating them into the palm. Bare hands left in the imported
        open-hand bind pose are the most visible remaining tell on a fielder at hero detail. */
    void CurlFingers(const FString& Side,float Amount);
    UPROPERTY() TObjectPtr<UAnimSequence> RunClip;
    UPROPERTY() TObjectPtr<UAnimSequence> IdleClip;
    /** Authored one-shot cricket actions, keyed on this same 67-bone rig by
        ArtSource/Blender/Animation/c26_anim_author.py and imported by Tools/ImportAnimations.py.
        Both are bound to SK_Cricketer_KitBase_Skeleton -- the skeleton SK_Cricketer_Match skins to
        -- so they drive this mesh directly with no retargeting step. See Docs/AUTHORED_ANIMATION.md. */
    UPROPERTY() TObjectPtr<UAnimSequence> BattingClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BowlingClip;
    /** The shot library. Every batting clip shares the drive's frame layout (36 frames,
        contact at 23) and every bowling clip the pace layout (46 frames, release at 31),
        so the same time-warp constants pin them all to the match's own timing. These are
        loaded lazily on first use (not via ConstructorHelpers) so a checkout that has not
        run Tools/ImportAnimations.py yet still boots; a null clip falls back to its
        family's base clip, then to the procedural action. */
    UPROPERTY() TObjectPtr<UAnimSequence> BattingPullClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingCutClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingSweepClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingDefenceClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingHookClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingLoftedDriveClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BattingGlanceClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BowlingOffSpinClip;
    UPROPERTY() TObjectPtr<UAnimSequence> BowlingLegSpinClip;
    UPROPERTY() TObjectPtr<UMaterialInterface> TexturedSkin;
    void ApplyRecordedMotion(bool Running, bool Batting);
    /** Sample a whole-body authored action over Pose. This is the same bind-by-name retarget
        ApplyRecordedMotion uses, with two differences that matter for a one-shot action: the clip
        is allowed to carry the body fore and aft (the batter's weight transfer, the bowler's bound
        over the braced foot are the action), and only the bones the clip genuinely keys are taken
        from it, so the procedural pass keeps everything the clip does not author. */
    void ApplyAuthoredClip(UAnimSequence* Clip,const TSet<int32>& Driven,float Time,float Weight);
    /** Which bones a clip actually moves, measured by sampling it. A clip that does not key a bone
        hands back that bone's reference pose, and blending that in would silently erase whatever
        the procedural pass authored there -- the fingers closed around the handle, the head aim.
        Measured once and cached: the answer never changes, and testing per frame would flicker
        every time a bone passed through its own rest pose. */
    void GatherDrivenBones(UAnimSequence* Clip,TSet<int32>& Out);
    /** Driven-bone sets per clip pointer, so the shot library shares one cache. */
    UPROPERTY(Transient) TMap<TObjectPtr<UAnimSequence>,TSet<int32>> ClipDriven;
    /** True once the shot library has had its one load attempt. */
    bool ShotLibraryLoaded=false;
    /** Pick the batting clip for the shot the simulation actually played: defence first,
        then the shot angle (leg side vs off side) crossed with the stride intent (weight
        back vs forward). Never null: falls back to the drive, then the caller's null check
        falls back to the procedural stroke. */
    UAnimSequence* SelectBattingClip();
    /** Pick the bowling action for the delivery the match asked for: finger-spin types get
        the low round-arm release, wrist-spin types the tall whippy one, seam/swing types
        the pace action. */
    UAnimSequence* SelectBowlingClip();
    /** Lazy load of the six library clips (see the block above for why not the ctor). */
    void LoadShotLibrary();
    TSet<int32>& DrivenFor(UAnimSequence* Clip);
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
    /** Head materials. The mesh has always had separate Bodymat, Eyesmat and Eyelashmat slots;
        only Bodymat was ever dressed, which is why every face in the game was a blank brown
        volume. Face carries the avatar's real 2048x2048 body atlas where it resolved, and falls
        back to the flat Skin tone where it did not. */
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FaceMat;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Eyes;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Lash;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShadeMaterial;
    int Bone(const FString& Name) const;
    void RebuildReference();
    void RebuildChildren(int Index);    void Aim(const FString& Name,const FString& Child,const FVector& Target);
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
