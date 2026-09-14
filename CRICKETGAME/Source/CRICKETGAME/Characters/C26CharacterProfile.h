#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SuperOver/C26Types.h"
#include "C26CharacterProfile.generated.h"

class UAnimSequence;
class USkeletalMesh;
class USkeleton;
class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EC26VisualRole : uint8 { Batter, NonStriker, Bowler, Fielder, Keeper, Umpire };
UENUM(BlueprintType)
enum class EC26EquipmentSlot : uint8
{
    Helmet, Headwear, BattingPadL, BattingPadR, KeeperPadL, KeeperPadR,
    BattingGloveL, BattingGloveR, KeeperGloveL, KeeperGloveR, Bat, Ball, Accessory
};
UENUM(BlueprintType)
enum class EC26CharacterQuality : uint8 { Low, Medium, High, Cinematic };

/** Roles are independent of distance/LOD. The ball is the match's existing authoritative actor. */
namespace C26Character
{
    CRICKETGAME_API bool Allows(EC26VisualRole Role, EC26EquipmentSlot Slot);
    CRICKETGAME_API bool Requires(EC26VisualRole Role, EC26EquipmentSlot Slot);
    /** Authored stroke clips every batter profile must carry (each with one BatContact notify). */
    CRICKETGAME_API const TArray<FString>& ShotClips();
    /** Match shot label -> authored clip key, e.g. "EXTRA-COVER DRIVE" -> COVERDRIVE_R. */
    CRICKETGAME_API FName ShotKey(const FString& MatchLabel, bool LeftHanded);
    CRICKETGAME_API FName BowlingKey(EC26Delivery Delivery, bool LeftHanded);
    CRICKETGAME_API float MapEventTime(float ActionTime, float MatchEventTime, float ClipEventTime, float ClipLength);
}

USTRUCT(BlueprintType)
struct FC26EquipmentDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) EC26EquipmentSlot Slot = EC26EquipmentSlot::Bat;
    UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere) FName Socket;
    UPROPERTY(EditAnywhere) FName LeftHandedSocket;
    UPROPERTY(EditAnywhere) FTransform LeftHandedOffset;
    /** Authored once in the socket's local frame, never placed independently per frame. */
    UPROPERTY(EditAnywhere) FTransform Offset;
    /** Both attachment values must use the same handedness fallback. */
    FName ResolveSocket(bool LeftHanded) const;
    const FTransform& ResolveOffset(bool LeftHanded) const;
};

USTRUCT(BlueprintType)
struct FC26CricketClip
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Sequence;
    /** BatContact, BallRelease, Pickup, Catch or ThrowRelease. Stored on the actual asset. */
    UPROPERTY(EditAnywhere) FName Event;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.02", ClampMax="0.4")) float BlendSeconds = .12f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) float GroundSpeed = 450.f;
    UPROPERTY(EditAnywhere) bool Loop = false;
    /** Mesh-space hand travel from stance to the contact frame (measured offline
        per shot). Lets the contact warp predict where the authored hands land. */
    UPROPERTY(EditAnywhere) FVector ContactDelta = FVector::ZeroVector;
    float EventTime() const;
};

USTRUCT(BlueprintType)
struct FC26PlayerAppearance
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlayerID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BodyPreset = TEXT("Athletic");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 JerseyNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool LeftHandedBat = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool LeftArmBowl = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AnimationStyle = TEXT("Professional");
};

/** Import/approval boundary. Complete skinned clothing/body and shoes, no baked cricket equipment.
    All role meshes and clips share this skeleton. Source asset paths are never auto-guessed. */
UCLASS(BlueprintType)
class CRICKETGAME_API UC26CharacterProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) TObjectPtr<USkeleton> Skeleton;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> Body;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> UmpireBody;
    /** One import-frame adapter: current source faces +Y, gameplay faces +X. Scale stays 1. */
    UPROPERTY(EditAnywhere) FRotator MeshToGameplayRotation = FRotator(0,-90,0);
    UPROPERTY(EditAnywhere) TMap<FName, FC26CricketClip> Clips;
    UPROPERTY(EditAnywhere) TArray<FC26EquipmentDefinition> Equipment;
    UPROPERTY(EditAnywhere) FName LeftHandSocket = TEXT("BallHand_L");
    UPROPERTY(EditAnywhere) FName RightHandSocket = TEXT("BallHand_R");
    UPROPERTY(EditAnywhere) FName LeftFootBone = TEXT("foot_l");
    UPROPERTY(EditAnywhere) FName RightFootBone = TEXT("foot_r");
    UPROPERTY(EditAnywhere) FName JerseyMaterialSlot = TEXT("Jersey");
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<UMaterialInterface>> TeamMaterials;
    /** Optional body/head/clothing material assignments by imported slot name. Empty preserves
        asset defaults. TeamMaterials is applied last so team identification retains precedence. */
    UPROPERTY(EditAnywhere) TMap<FName, TObjectPtr<UMaterialInterface>> MaterialOverrides;
    /** Separate files for changes in height; no arbitrary nested runtime scale. */
    UPROPERTY(EditAnywhere) TMap<FName, TObjectPtr<USkeletalMesh>> BodyPresets;
    UPROPERTY(EditAnywhere) bool ApprovedForMatch = false;
    UPROPERTY(EditAnywhere, meta=(MultiLine=true)) FString SourceAndLicense;
    UPROPERTY(EditAnywhere, meta=(MultiLine=true)) FString VisualReviewEvidence;

    UFUNCTION(BlueprintCallable, Category="C26|Validation")
    static TArray<FString> InspectBody(USkeletalMesh* Candidate);
    UFUNCTION(BlueprintCallable, Category="C26|Validation")
    static TMap<FName,FTransform> BindPose(USkeletalMesh* Candidate);
    UFUNCTION(BlueprintCallable, Category="C26|Validation")
    static TMap<int32,FString> ExportMaterialMap(USkeletalMesh* Candidate, int32 Lod);
    /** A staged review validates the complete selected role, not the whole production roster. */
    UFUNCTION(BlueprintCallable, Category="C26|Validation")
    TArray<FString> InspectRole(EC26VisualRole Role) const;
    UFUNCTION(BlueprintCallable, Category="C26|Authoring")
    static bool SetEquipmentSocket(USkeletalMesh* Candidate, FName Name, FName Bone, FTransform Local);
    bool Validate(TArray<FString>& Errors, bool RequireApproval = true) const;
    /** Central role/preset selection; head and clothing currently remain part of the body mesh. */
    USkeletalMesh* ResolveBody(EC26VisualRole Role, FName Preset) const;
    bool ValidateMaterialOverrides(TArray<FString>& Errors) const;
    static bool AuditBody(USkeletalMesh* Candidate, USkeleton* Expected, TArray<FString>& Errors);
    const FC26CricketClip* FindClip(FName Key, FName Fallback = NAME_None) const;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="C26|Validation")
    bool ValidateForMatch() const;
};
