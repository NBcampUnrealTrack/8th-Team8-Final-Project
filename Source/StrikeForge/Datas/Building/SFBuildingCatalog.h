#pragma once

#include "CoreMinimal.h"
#include "SFBuildingType.h"
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
	// USFBuildingDefinition* FindDefinition(FName InBuildingId) const;
	//
	// USFBuildingDefinition* FindDefinitionByBuildingType(ESFBuildingType InBuildingType) const;
	/** 맵에 배치되는 해당 종족의 넥서스 Definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog|Nexus")
	TSoftObjectPtr<USFBuildingDefinition> NexusDefinition;

	// Async
	/** Catalog에 등록된 Nexus와 건설 가능 Definition 경로를 수집한다. HandleCatalogLoaded에서 호출되며 Definition 비동기 로드로 이어진다. */
	void GetDefinitionPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** 이미 로드된 Definition을 BuildingId로 찾는다. 건설 요청에서 호출되며 Tier 런타임 리소스 요청으로 이어진다. */
	USFBuildingDefinition* FindLoadedDefinition(FName InBuildingId) const;

	/** 이미 로드된 Definition을 BuildingType으로 찾는다. Anchor 처리에서 호출되며 Tier 0 로드·생성으로 이어진다. */
	USFBuildingDefinition* FindLoadedDefinitionByType(ESFBuildingType InBuildingType) const;
	
	/**
	 * 플레이어가 직접 건설할 수 있는 건물 목록.
	 *
	 * NexusDefinition은 이 배열에 넣지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly, Category = "Catalog|Buildable")
	TArray<TSoftObjectPtr<USFBuildingDefinition>> BuildableDefinitions;
};