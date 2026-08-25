#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SFBuildingCatalog.generated.h"

class USFBuildingDefinition;

UCLASS(BlueprintType)
class STRIKEFORGE_API USFBuildingCatalog
	: public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * BuildingId가 일치하는 Definition을 반환한다.
	 *
	 * NexusDefinition과 BuildableDefinitions를 모두 조회한다.
	 * 현재는 동기 로딩을 사용한다.
	 */
	USFBuildingDefinition* FindDefinition(FName InBuildingId) const;

	/** 맵에 배치되는 해당 종족의 넥서스 Definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog|Nexus")
	TSoftObjectPtr<USFBuildingDefinition> NexusDefinition;

	/**
	 * 플레이어가 직접 건설할 수 있는 건물 목록.
	 *
	 * NexusDefinition은 이 배열에 넣지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly, Category = "Catalog|Buildable")
	TArray<TSoftObjectPtr<USFBuildingDefinition>> BuildableDefinitions;
};