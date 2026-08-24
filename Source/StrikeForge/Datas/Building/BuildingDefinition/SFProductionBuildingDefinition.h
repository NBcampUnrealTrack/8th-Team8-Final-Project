#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "SFProductionBuildingDefinition.generated.h"

UCLASS(BlueprintType)
class STRIKEFORGE_API USFProductionBuildingDefinition : public USFBuildingDefinition
{
	GENERATED_BODY()
	
public:
	virtual int32 GetTierCount() const override;

	virtual const FSFBuildingTierData* FindTier(
		int32 TierIndex) const override;

	/**
	 * Production 전용 Tier 데이터를 반환한다.
	 *
	 * BaseBuilding은 FindTier()를 사용하고,
	 * ProductionBuilding은 이 함수를 통해 SpawnableUnits에 접근한다.
	 */
	const FSFProductionBuildingTierData* FindProductionTier(
		int32 TierIndex) const;

public:
	// FSFProductionBuildingTierData = Tiers를 상속해 확장한 구조체
	/** 배열 인덱스 0이 게임의 1티어다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers")
	TArray<FSFProductionBuildingTierData> Tiers;
};
