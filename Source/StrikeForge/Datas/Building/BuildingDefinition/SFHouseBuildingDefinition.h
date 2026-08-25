#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "SFHouseBuildingDefinition.generated.h"

/**
 * 최대 인구를 제공하는 House 계열 건물의 데이터 정의.
 *
 * 부모 Definition:
 * - BuildingId
 * - DisplayName
 * - BuildingClass
 * - GridSize
 * - 공통 파괴 효과 등
 *
 * House Definition:
 * - House 전용 Tier 배열
 */
UCLASS(BlueprintType)
class STRIKEFORGE_API USFHouseBuildingDefinition
	: public USFBuildingDefinition
{
	GENERATED_BODY()

public:
	virtual int32 GetTierCount() const override;

	virtual const FSFBuildingTierData* FindTier(
		int32 TierIndex) const override;

	/**
	 * House 전용 Tier 데이터를 반환한다.
	 *
	 * BaseBuilding은 FindTier()를 사용하고,
	 * HouseBuilding은 이 함수를 통해 PopulationCapacity에 접근한다.
	 */
	const FSFHouseBuildingTierData* FindHouseTier(
		int32 TierIndex) const;

public:
	// FSFHouseBuildingTierData = Tiers를 상속해 확장한 구조체
	/** 배열 인덱스 0이 게임의 1티어다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers")
	TArray<FSFHouseBuildingTierData> Tiers;
};