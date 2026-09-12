#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26Commentary.h"
#include "C26Stadium.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USpotLightComponent;
class UPostProcessComponent;
class UTextRenderComponent;
class UExponentialHeightFogComponent;

/** Broadcast environment profiles. Night (2) is the shipped default; day sessions reuse the
    same venue, lights and materials with a re-aimed grade rather than a second map. */
UENUM(BlueprintType)
enum class EC26EnvironmentProfile : uint8
{
    ClearDay      UMETA(DisplayName="Clear Day"),
    LateAfternoon UMETA(DisplayName="Late Afternoon"),
    Night         UMETA(DisplayName="Night")
};

// Visual crowd reaction reuses the audio director's EC26CrowdState vocabulary (Calm, Anticipation,
// Excited, Boundary, Six, Wicket, Tense, Win, Loss) so sound and the visible spectator shader can
// never disagree about what the ground is feeling.

/** Prepared-surface conditions. All four share the one baked ground texture; the condition only
    re-tints its response, so switching never pops geometry or streaming. */
UENUM(BlueprintType)
enum class EC26PitchCondition : uint8
{
    Fresh UMETA(DisplayName="Fresh"),
    Used  UMETA(DisplayName="Used"),
    Dry   UMETA(DisplayName="Dry"),
    Worn  UMETA(DisplayName="Worn")
};

UCLASS()
class CRICKETGAME_API AC26Stadium : public AActor
{
    GENERATED_BODY()
public:
    AC26Stadium();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    UFUNCTION(CallInEditor, Category="CRICKET 26") void BuildVenue();
    UPROPERTY(EditAnywhere, Category="CRICKET 26",meta=(ClampMin="0",ClampMax="3")) int32 CrowdQuality=2;
    /** Broadcast environment. Applied on top of the built venue; safe to call at any time. */
    UPROPERTY(EditAnywhere, Category="CRICKET 26") EC26EnvironmentProfile EnvironmentProfile=EC26EnvironmentProfile::Night;
    /** Prepared-surface condition for the live match. */
    UPROPERTY(EditAnywhere, Category="CRICKET 26") EC26PitchCondition PitchCondition=EC26PitchCondition::Used;
    void SetEnvironment(EC26EnvironmentProfile Profile);
    void SetPitchCondition(EC26PitchCondition Condition);
    void SetQuality(int Level);
    void React(float Intensity);
    /** Named visual reaction state for the spectator shader + LED ribbon. */
    void SetCrowdState(EC26CrowdState State);
    EC26CrowdState GetCrowdState() const { return CrowdState; }
    /** One-shot LED ribbon flash (boundary confirmation, wicket). Decays in UpdateAtmosphere. */
    void PulseLED(float Strength);
    /** Pure data tables so automation can pin the broadcast grade without a world. */
    static float CrowdTargetForState(EC26CrowdState State);
    static float CrowdRateForState(EC26CrowdState State);
    static float ExposureBiasForProfile(EC26EnvironmentProfile Profile);
    static FLinearColor PitchTintForCondition(EC26PitchCondition Condition);
    void UpdateAtmosphere(float Time);
    /** Update stadium Jumbotron displays with live match context (score bug, event wipes). */
    void UpdateJumbotron(const FString& Line1, const FString& Line2, const FLinearColor& Color = FLinearColor(0.81f, 0.93f, 0.90f));
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Bowl;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Architecture;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Sky;
    /** Additive haze cones hanging under each pylon. Six long, very faint quads sell a floodlit
        night far more cheaply than volumetric fog, and they are the element that makes the venue
        read as a night match rather than an overcast afternoon. */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Shafts;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> KeyLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> CrossLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> FillLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Haze;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Grade;
    /** Real floodlight throw from four of the six pylons; unshadowed so the cost stays flat. */
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Floods;
private:
    UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> CrowdMaterials;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Signs;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> JumbotronSigns;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LED;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LampMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShaftMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GroundMID;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SkyMaterials;
    float CrowdReaction=0;
    float CrowdTarget=0.06f;
    float CrowdRate=0.42f;
    EC26CrowdState CrowdState=EC26CrowdState::Calm;
    float LEDSpike=0.f;
    float LastAtmosphereTime=0;
    float AtmosphereUpdateAccumulator=0;
    void ConfigureLighting();
    void ApplyLightVisibility();
    void BuildLightShafts();
    UHierarchicalInstancedStaticMeshComponent* Batch(const TCHAR* Name,UStaticMesh* Mesh,UMaterialInterface* Material);
};
