#include "SFBuildingCatalog.h"

#include "Datas/Building/SFBuildingDefinition.h"

USFBuildingDefinition*
USFBuildingCatalog::FindDefinition(
	FName InBuildingId) const
{
	if (InBuildingId.IsNone())
	{
		return nullptr;
	}

	/**
	 * 현재 단계에서는 동기 로딩한다.
	 *
	 * 추후 비동기 로딩으로 전환하더라도
	 * 카탈로그 외부의 호출 코드는 최대한 유지할 수 있다.
	 */
	auto FindMatchingDefinition =
		[InBuildingId](
			const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)
		-> USFBuildingDefinition*
		{
			if (DefinitionRef.IsNull())
			{
				return nullptr;
			}

			USFBuildingDefinition* Definition =
				DefinitionRef.LoadSynchronous();

			if (!IsValid(Definition))
			{
				return nullptr;
			}

			return Definition->BuildingId == InBuildingId
				? Definition
				: nullptr;
		};

	// 맵 배치 넥서스도 BuildingId로 조회할 수 있어야 한다.
	if (USFBuildingDefinition* Nexus =
		FindMatchingDefinition(NexusDefinition))
	{
		return Nexus;
	}

	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
		: BuildableDefinitions)
	{
		if (USFBuildingDefinition* Definition =
			FindMatchingDefinition(DefinitionRef))
		{
			return Definition;
		}
	}

	return nullptr;
}

USFBuildingDefinition* USFBuildingCatalog::FindDefinitionByBuildingType(ESFBuildingType InBuildingType) const
{
	if (InBuildingType == ESFBuildingType::None)
	{
		return nullptr;
	}
	
	// 리턴값이 있는 람다 정의
	auto FindMatchingDefinition = [InBuildingType](const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)->USFBuildingDefinition*
	{
		USFBuildingDefinition* Definition =
				DefinitionRef.LoadSynchronous();

		if (!IsValid(Definition))
		{
			return nullptr;
		}

		return Definition->BuildingType == InBuildingType
			? Definition
			: nullptr;
	};
	
	// 건설가능 DA배열에 넥서스는 빠져있으므로 따로 검증
	if (USFBuildingDefinition* Nexus = FindMatchingDefinition(NexusDefinition))
	{
		return Nexus;
	}
	
	// TArray for문으로 돌면서 있는지 람다식으로 검증
	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef	: BuildableDefinitions)
	{
		if (USFBuildingDefinition* Definition =	FindMatchingDefinition(DefinitionRef))
		{
			return Definition;
		}
	}

	return nullptr;
}
