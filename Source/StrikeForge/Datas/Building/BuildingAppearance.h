#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Resource/ResourceTypes.h"
#include "BuildingAppearance.generated.h"

class ASFBaseBuilding;
class UStaticMesh;
class USoundBase;
class UMaterialInterface;

/**
 * 임계값 하나와 그때의 메시.
 * 건설 단계와 피해 단계가 같은 규칙을 쓴다.
 *
 * 정렬  : Threshold 오름차순
 * 조회  : 현재 값보다 작거나 같은 임계값 중 가장 큰 항목
 * 첫 항목의 Threshold 는 반드시 0 이어야 한다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingStage
{
	GENERATED_BODY()

	/**
	 * 이 값 "이상"일 때 적용된다 [0,1].
	 *   ConstructionStages → 건설 진행도
	 *   DamageStages       → 체력 비율
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> Mesh;
};

/** 티어 하나의 전체 정의 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingTier
{
	GENERATED_BODY()

	/** 건설 중 외형. Preview / UnderConstruction 에서 사용. 마지막 = 거의 완성된 모습 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FBuildingStage> ConstructionStages;

	/** 완성 후 체력별 외형. [0] = 파괴(잔해), 마지막 = 온전한 완성 외형 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FBuildingStage> DamageStages;

	/** 티어 업그레이드 시 이 값으로 완전 회복된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** 이 티어의 건설(업그레이드) 소요 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.1"))
	float ConstructionDuration = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> StartConstructSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> CompleteSound;

	/** 생산 가능 유닛의 행 이름 (FUnitProductData 테이블) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Units")
	TArray<FName> SpawnableUnits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	FResources ConstructCost;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost")
	FResources UpgradeCost;

};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingAppearance : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
	FText DisplayName;

	/** 카탈로그 UI 용. 지금은 미사용이지만 나중 재입력을 피하려 미리 둔다 */
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
	// TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TSoftClassPtr<ASFBaseBuilding> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TSoftObjectPtr<UMaterialInterface> PreviewMeshMaterial;

	/** 인덱스 0 = 1티어 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiers")
	TArray<FBuildingTier> BuildingMeshesByTiers;

	const FBuildingTier* GetTierInfo(int32 TierIndex) const
	{
		return BuildingMeshesByTiers.IsValidIndex(TierIndex) ? &BuildingMeshesByTiers[TierIndex] : nullptr;
	}

	bool CanUpgradeNextTier(int32 TierIndex) const
	{
		return BuildingMeshesByTiers.IsValidIndex(TierIndex + 1);
	}

	/** 최초 건설 비용. Tiers 가 비어 있으면 nullptr */
	const FResources* GetConstructBuildCost(int32 FromTierIndex) const
	{
		//return BuildingMeshesByTiers.Num() > 0 ? &BuildingMeshesByTiers[FromTierIndex].ConstructCost : nullptr;
		return BuildingMeshesByTiers.IsValidIndex(FromTierIndex) ? &BuildingMeshesByTiers[FromTierIndex].ConstructCost : nullptr;
		
	}

	/**
	 * FromTierIndex 에서 다음 티어로 올리는 비용.
	 * 다음 티어가 없으면 nullptr — 업그레이드 불가와 같은 뜻이다.
	 */
	const FResources* GetUpgradeBuildCost(int32 FromTierIndex) const
	{
		return BuildingMeshesByTiers.IsValidIndex(FromTierIndex) ? &BuildingMeshesByTiers[FromTierIndex].UpgradeCost : nullptr;
	}
	
	/**
	 * 현재 값에 해당하는 단계 인덱스를 찾는다.
	 * 건설 단계와 피해 단계가 이 함수를 공유한다.
	 * Stages 는 Threshold 오름차순으로 정렬되어 있어야 한다.
	 */
	static int32 FindStageIndex(const TArray<FBuildingStage>& InStages, float InValue)
	{
		if (InStages.Num() == 0)
		{
			return INDEX_NONE;              // ★ADD: 빈 배열에서 0 을 반환하면 호출부가 크래시한다
		}

		int32 Found = 0;
		for (int32 Index = 0; Index < InStages.Num(); ++Index)
		{
			if (InValue >= InStages[Index].Threshold) { Found = Index; }
			else { break; }
		}
		return Found;
	}
	
	static int32 FindDamageStageIndex(const TArray<FBuildingStage>& InStages, float InRatio)
	{
		// ★ADD: 체력이 가득 차 있으면 손상이 아니다.
		//        데이터에 Threshold 1.0 항목이 있어도 완성 외형을 덮지 않는다.
		if (InRatio >= 1.f)
		{
			return INDEX_NONE;
		}
		
		// 오름차순이므로 처음 만나는 항목이 조건을 만족하는 가장 낮은 단계다.
		for (int32 Index = 0; Index < InStages.Num(); ++Index)
		{
			if (InRatio <= InStages[Index].Threshold)
			{
				return Index;
			}
		}
		return INDEX_NONE;      // 어떤 단계보다도 체력이 높다 → 손상 없음
	}
//
// #if WITH_EDITOR
// 	virtual void OnDataTableChanged(const UDataTable* InTable, const FName InRowName) override;
// #endif
 };