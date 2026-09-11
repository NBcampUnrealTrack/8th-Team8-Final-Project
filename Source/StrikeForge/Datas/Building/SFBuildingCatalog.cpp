#include "SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"

void USFBuildingCatalog::GetDefinitionPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	auto AddPath =
	   [&OutPaths](
		   const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)
	   {
	   	if (!DefinitionRef.IsNull())
	   	{
	   		OutPaths.AddUnique(
				   DefinitionRef.ToSoftObjectPath());
	   	}
	   };

	AddPath(NexusDefinition);

	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
		: BuildableDefinitions)
	{
		AddPath(DefinitionRef);
	}
}

USFBuildingDefinition* USFBuildingCatalog::FindLoadedDefinition(FName InBuildingId) const
{
	auto FindMatching =
		[InBuildingId](
			const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)
			-> USFBuildingDefinition*
		{
			USFBuildingDefinition* Definition =
				DefinitionRef.Get();

			return IsValid(Definition) &&
				   Definition->BuildingId == InBuildingId
				? Definition
				: nullptr;
		};

	if (USFBuildingDefinition* Nexus =
		FindMatching(NexusDefinition))
	{
		return Nexus;
	}

	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
		: BuildableDefinitions)
	{
		if (USFBuildingDefinition* Found =
			FindMatching(DefinitionRef))
		{
			return Found;
		}
	}

	return nullptr;
}

USFBuildingDefinition* USFBuildingCatalog::FindLoadedDefinitionByType(ESFBuildingType InBuildingType) const
{
	if (InBuildingType == ESFBuildingType::None)
	{
		return nullptr;
	}

	auto FindMatching =
		[InBuildingType](
			const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)
			-> USFBuildingDefinition*
		{
			USFBuildingDefinition* Definition =
				DefinitionRef.Get();

			return IsValid(Definition) &&
				   Definition->BuildingType == InBuildingType
				? Definition
				: nullptr;
		};

	if (USFBuildingDefinition* Nexus =
		FindMatching(NexusDefinition))
	{
		return Nexus;
	}

	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
		: BuildableDefinitions)
	{
		if (USFBuildingDefinition* Definition =
			FindMatching(DefinitionRef))
		{
			return Definition;
		}
	}

	return nullptr;
}
