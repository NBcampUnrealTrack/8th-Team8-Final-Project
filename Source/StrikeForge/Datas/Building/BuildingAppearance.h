#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "BuildingAppearance.generated.h"

class USoundBase;
class UStaticMesh;
//GC
class UGeometryCollection;

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
struct STRIKEFORGE_API FBuildingDestruction
{
	GENERATED_BODY()

	/** 이 체력 비율 이하에서 해당 파괴 단계에 진입한다. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (
			ClampMin = "0.0",
			ClampMax = "1.0"))
	float HealthThreshold = 0.5f;

	/** GC Damage Threshold를 넘기기 위한 External Strain. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (ClampMin = "0.0"))
	float Strain = 550000.f;

	/** GC 로컬 Bounds를 기준으로 계산할 Field 반경 배율. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (ClampMin = "0.1"))
	float RadiusScale = 1.f;

	/** GC 로컬 Bounds 중심으로부터의 Field 위치 오프셋. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction")
	FVector LocalOffset = FVector::ZeroVector;

	/** 분리된 파편에 적용할 힘. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (ClampMin = "0.0"))
	float ImpulseStrength = 0.f;

	/** Strain 이후 Impulse를 적용하기 전 대기 시간. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (
			ClampMin = "0.0",
			ClampMax = "0.2"))
	float ImpulseDelay = 0.05f;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FBuildingSurfaceFireStep
{
	GENERATED_BODY()

	/** 이 체력 비율 이하에서 이 구간에 진입한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceFire",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThreshold = 0.9f;

	/** 이 구간에서 건물에 붙어 있어야 할 불꽃 개수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SurfaceFire",
		meta = (ClampMin = "1"))
	int32 FireCount = 1;
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

#pragma region GC로대체
	/* 가벼운 손상부터 심각한 손상 순서로 사용한다.
	* 런타임에서 Threshold 내림차순으로 정렬한다.*/
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	// TArray<FBuildingStage> DamageStages;
	//
	// UPROPERTY(EditAnywhere,	BlueprintReadOnly,	Category = "Visual")
	// TSoftObjectPtr<UStaticMesh> DestroyedMesh;
#pragma endregion 
	/**
	* 해당 Tier의 파괴 연출에 사용할 GC.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Destruction")
	TSoftObjectPtr<UGeometryCollection> DestructionCollection;

	/**
	 * 체력에 따른 GC 파괴 단계.
	 *
	 * HealthThreshold 내림차순으로 정렬한다.
	 * 마지막 원소는 반드시 HealthThreshold 0인 최종 파괴 단계다.
	 *
	 * [0] → GC Level 1
	 * [1] → GC Level 2
	 * [2] → GC Level 3
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Destruction")
	TArray<FBuildingDestruction> DestructionStages;

	/**
	 * 불꽃이 하나씩 추가되는 체력 비율.
	 * 내림차순으로 넣는다. 예: 0.9, 0.75, 0.6, 0.45, 0.3
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Destruction")
	TArray<FBuildingSurfaceFireStep> SurfaceFireSteps;
	
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
#pragma region GC로대체
	// int32 FindDamageStageIndex(float HealthRatio) const
	// {
	// 	HealthRatio = FMath::Clamp(HealthRatio, 0.f, 1.f);
	// 	
	// 	if (HealthRatio >= 1.f)
	// 	{	// 풀피
	// 		return INDEX_NONE;
	// 	}
	//
	// 	int32 Result = INDEX_NONE;
	//
	// 	for (int32 Index = 0; Index < DamageStages.Num(); ++Index)
	// 	{
	// 		if (HealthRatio <= DamageStages[Index].Threshold)
	// 		{
	// 			Result = Index;
	// 		}
	// 	}
	//
	// 	return Result;
	// }
#pragma endregion 
	int32 FindDestructionStageIndex(float HealthRatio) const
	{
		HealthRatio = FMath::Clamp(
			HealthRatio,
			0.f,
			1.f);

		// 체력 0은 HandleHealthZero에서 처리한다.
		if (HealthRatio <= 0.f || HealthRatio >= 1.f)
		{
			return INDEX_NONE;
		}

		int32 Result = INDEX_NONE;

		for (int32 Index = 0; Index < DestructionStages.Num(); ++Index)
		{
			if (HealthRatio <=	DestructionStages[Index].HealthThreshold)
			{
				Result = Index;
			}
		}

		return Result;
	}
	/** 현재 체력 비율에 해당하는 불꽃 구간 인덱스를 반환한다. */
	int32 FindSurfaceFireStepIndex(float HealthRatio) const
	{
		HealthRatio = FMath::Clamp(HealthRatio, 0.f, 1.f);

		int32 Result = INDEX_NONE;

		for (int32 Index = 0; Index < SurfaceFireSteps.Num(); ++Index)
		{
			if (HealthRatio <= SurfaceFireSteps[Index].HealthThreshold)
			{
				Result = Index;
			}
		}

		return Result;
	}

	const FBuildingSurfaceFireStep* GetSurfaceFireStep(const int32 StepIndex) const
	{
		return SurfaceFireSteps.IsValidIndex(StepIndex)
			? &SurfaceFireSteps[StepIndex]
			: nullptr;
	}
	
	const FBuildingDestruction* GetDestructionStage(const int32 StageIndex) const
	{
		return DestructionStages.IsValidIndex(StageIndex)
			? &DestructionStages[StageIndex]
			: nullptr;
	}
	const FBuildingDestruction* GetFinalDestructionStage() const
	{
		if (DestructionStages.IsEmpty())
		{
			return nullptr;
		}

		const FBuildingDestruction& FinalStage =
			DestructionStages.Last();

		return FMath::IsNearlyZero(
			FinalStage.HealthThreshold)
				? &FinalStage
				: nullptr;
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
