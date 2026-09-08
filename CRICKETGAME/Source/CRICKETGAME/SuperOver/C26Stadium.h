#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "C26Stadium.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UPostProcessComponent;
class UTextRenderComponent;

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
    void SetQuality(int Level);
    void React(float Intensity);
    void UpdateAtmosphere(float Time);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Bowl;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> KeyLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> FillLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Grade;
private:
    UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> CrowdMaterials;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Signs;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LED;
    float CrowdReaction=0;
    UHierarchicalInstancedStaticMeshComponent* Batch(const TCHAR* Name,UStaticMesh* Mesh,UMaterialInterface* Material);
};
