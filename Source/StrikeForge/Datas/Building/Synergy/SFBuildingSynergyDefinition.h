#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/SFBuildingType.h"
#include "Datas/Building/Synergy/SFBuildingSynergyTypes.h"
#include "Engine/DataAsset.h"
#include "SFBuildingSynergyDefinition.generated.h"

/**
 * 여러 건물 Definition이 공유하는 건물 배치 시너지 규칙입니다.
 * 원천 건물 종류는 이 에셋을 참조하는 건물 Definition이 결정합니다.
 */
UCLASS(BlueprintType)
class STRIKEFORGE_API USFBuildingSynergyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType SynergyAssetType;

	/** AssetManager가 이 시너지 Definition을 고유 PrimaryAsset으로 식별할 ID를 반환합니다. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	/** 로드된 에셋의 셀 크기를 유효한 전체 크기 값으로 정규화합니다. */
	virtual void PostLoad() override;

#if WITH_EDITOR
	/** 디자이너가 값을 수정하면 모든 셀 크기를 1 이상으로 즉시 보정합니다. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** 현재 원천 건물 수에 적용할 가장 높은 단계를 반환합니다. */
	const FSFBuildingSynergyLevel* FindLevel(int32 SourceBuildingCount) const;

	/** 전달된 건물 분류가 이 시너지의 생산력 보너스 대상인지 확인합니다. */
	bool CanAffectBuildingType(ESFBuildingType BuildingType) const;
	/** 명시된 SynergyId를 사용하고, 비어 있으면 에셋 이름을 대체 ID로 반환합니다. */
	FName GetEffectiveSynergyId() const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Identity")
	FName SynergyId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** 원천 건물의 로컬 X/Y 기준 링크 전체 셀 수이며, 건물 회전에 따라 함께 회전합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Clustering", meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint ClusterLinkSizeCells = FIntPoint(11, 11);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TArray<FSFBuildingSynergyLevel> Levels;

	/** 생산력 보너스를 받을 수 있는 건물 분류입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
	TArray<ESFBuildingType> TargetBuildingTypes;

private:
	/** 연결 범위와 단계별 효과 범위를 1 이상의 전체 셀 크기로 보정합니다. */
	void NormalizeCellSizes();
};
