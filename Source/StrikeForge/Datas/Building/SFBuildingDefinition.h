#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Engine/DataAsset.h"
#include "SFBuildingDefinition.generated.h"

class ASFBaseBuilding;
class FDataValidationContext;
class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;

UCLASS(BlueprintType)
class STRIKEFORGE_API USFBuildingDefinition
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType BuildingAssetType;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(
        FDataValidationContext& Context) const override;
#endif

    const FBuildingTier* FindTier(int32 TierIndex) const;

public:
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        AssetRegistrySearchable,
        Category = "Identity")
    FName BuildingId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSoftObjectPtr<UTexture2D> Icon;

    /**
     * 현재 선택 UI DataTable과 연결하기 위한 임시 값.
     * 기존 Building RowName과 같은 값을 넣는다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Legacy")
    FName SelectionUIRowName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
    TSoftClassPtr<ASFBaseBuilding> BuildingClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Placement",
        meta = (ClampMin = "1"))
    FIntPoint GridSize = FIntPoint(1, 1);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
    TSoftObjectPtr<UMaterialInterface> PreviewMaterial;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Requirement",
        meta = (ClampMin = "1"))
    int32 RequiredNexusTier = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destroyed")
    TSoftObjectPtr<USoundBase> DestroyedSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destroyed")
    TSoftObjectPtr<UNiagaraSystem> DestroyedEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tier")
    TArray<FBuildingTier> Tiers;
};