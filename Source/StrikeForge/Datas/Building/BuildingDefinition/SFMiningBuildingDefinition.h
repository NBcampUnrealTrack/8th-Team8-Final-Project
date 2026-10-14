#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFMiningBuildingDefinition.generated.h"

/**
 * 채굴 기능을 제공하는 Mining 계열 건물의 데이터 정의.
 
 * 부모 Definition:
 * - BuildingId
 * - DisplayName
 * - BuildingClass
 * - GridSize
 * - 공통 파괴 효과 등
 *
 * Mining Definition:
 * - Mining 전용 Tier 배열
 */
UCLASS(BlueprintType)
class STRIKEFORGE_API USFMiningBuildingDefinition
	: public USFBuildingDefinition
{
	GENERATED_BODY()

public:
	virtual int32 GetTierCount() const override;

	virtual const FSFBuildingTierData* FindTier(
		int32 TierIndex) const override;

	/**
	 * Mining 전용 Tier 데이터를 반환한다.
	 *
	 * BaseBuilding은 FindTier()를 사용하고,
	 * MiningBuilding은 이 함수를 통해 채굴 작업량과 획득량에 접근한다.
	 */
	const FSFMiningBuildingTierData* FindMiningTier(
		int32 TierIndex) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	EResourceType CanDigResourceType = EResourceType::Mineral;
	
	/**
	 * 건물 점유 격자 안에서 자원을 검색할 중심 영역입니다.
	 * 현재는 별도의 Offset 없이 건물 중심에 배치합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Mining", meta = (ClampMin = "1"))
	FIntPoint ResourceSearchGridSize = FIntPoint(1, 1);
	
	// FSFMiningBuildingTierData = Tiers를 상속해 확장한 구조체
	/** 배열 인덱스 0이 게임의 1티어다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tiers")
	TArray<FSFMiningBuildingTierData> Tiers;
};