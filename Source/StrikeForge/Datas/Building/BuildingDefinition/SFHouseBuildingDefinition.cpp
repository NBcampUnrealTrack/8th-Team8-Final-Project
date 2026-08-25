#include "SFHouseBuildingDefinition.h"

int32 USFHouseBuildingDefinition::GetTierCount() const
{
	return Tiers.Num();
}

const FSFBuildingTierData*
USFHouseBuildingDefinition::FindTier(
	int32 TierIndex) const
{
	return FindHouseTier(TierIndex);
}

const FSFHouseBuildingTierData*
USFHouseBuildingDefinition::FindHouseTier(
	int32 TierIndex) const
{
	return Tiers.IsValidIndex(TierIndex)
		? &Tiers[TierIndex]
		: nullptr;
}