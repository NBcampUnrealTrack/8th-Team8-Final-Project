#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Engine/DataTable.h"
#include "BuildingAppearance.generated.h"

class ASFBaseBuilding;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingConstructionData
{
	GENERATED_BODY()
	// 작업총량
	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Construction", meta = (ClampMin = "0.1"))
	float RequiredWork = 10.f; 

	/** 해당 티어를 최초 건설할 때 사용하는 비용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construction")
	FResources ConstructCost;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Construction")
	FResources UpgradeCost;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingStatData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Stats",	meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingVisualData
{
	GENERATED_BODY()

	/** 건설 중에만 사용하는 메시. 런타임에서 Threshold 오름차순으로 정렬한다. */
	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Visual")
	TArray<FBuildingStage> ConstructionStages;

	/** 건설 완료 상태의 기본 메시 */
	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Visual")
	TSoftObjectPtr<UStaticMesh> CompletedMesh;

	/* 가벼운 손상부터 심각한 손상 순서로 사용한다.
	* 런타임에서 Threshold 내림차순으로 정렬한다.*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TArray<FBuildingStage> DamageStages;
	
	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Visual")
	TSoftObjectPtr<UStaticMesh> DestroyedMesh;

	int32 FindConstructionStageIndex(float ProgressRatio) const
	{
		if (ConstructionStages.IsEmpty()) return INDEX_NONE;

		ProgressRatio = FMath::Clamp(ProgressRatio,0.f,1.f);
		int32 Result = 0;

		for (int32 Index = 0; Index < ConstructionStages.Num(); ++Index)
		{	// 입력된 proogressratio가 states에서 가지고 있는 float(treshold)보다 크면 index반환
			if (ProgressRatio >= ConstructionStages[Index].Threshold)
			{
				Result = Index;
			}
			else
			{
				// 크지않으면 index누적 종료
				break;
			}
		}

		return Result;
	}
	int32 FindDamageStageIndex(float HealthRatio) const
	{
		HealthRatio = FMath::Clamp(HealthRatio, 0.f, 1.f);
		
		if (HealthRatio >= 1.f)
		{	// 풀피
			return INDEX_NONE;
		}

		int32 Result = INDEX_NONE;

		for (int32 Index = 0; Index < DamageStages.Num(); ++Index)
		{
			if (HealthRatio <= DamageStages[Index].Threshold)
			{
				Result = Index;
			}
		}

		return Result;
	}
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingAudioData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> ConstructionStart;
	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> ConstructionComplete;
	
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Tier")
	FBuildingConstructionData Construction;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Tier")
	FBuildingStatData Stats;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Tier")
	FBuildingVisualData Visual;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Tier")
	FBuildingAudioData Audio;

	/**
	 * 생산 건물에서만 사용한다.
	 * 건물별 기능 데이터가 더 늘어나면 Extension 구조로 이전한다.
	 */
	UPROPERTY(EditAnywhere,	BlueprintReadOnly,Category = "Tier|Production")
	TArray<FName> SpawnableUnits;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|House")
	int32 PopulationCapacity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Mining")
	float MiningIncomeRate = 0.f; // 일단 초당 증가량으로

};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingAppearance: public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Spawn")
	TSoftClassPtr<ASFBaseBuilding> BuildingClass;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Placement",	meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Placement")
	TSoftObjectPtr<UMaterialInterface>	PreviewMaterial;

	UPROPERTY(EditAnywhere,	BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> DestroyedSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSoftObjectPtr<UNiagaraSystem> DestroyedEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier|Mining")
	EResourceType CanDigResourceType;
	
	/** 배열 인덱스 0이 게임의 1티어 */
	UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Tiers")
	TArray<FBuildingTier> Tiers;

	const FBuildingTier* FindTier(int32 TierIndex) const
	{
		return Tiers.IsValidIndex(TierIndex)
			? &Tiers[TierIndex]
			: nullptr;
	}
};
