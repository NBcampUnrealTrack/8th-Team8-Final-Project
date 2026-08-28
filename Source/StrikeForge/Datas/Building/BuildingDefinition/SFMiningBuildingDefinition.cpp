#include "SFMiningBuildingDefinition.h"

int32 USFMiningBuildingDefinition::GetTierCount() const
{
	return Tiers.Num();
}

const FSFBuildingTierData* USFMiningBuildingDefinition::FindTier(int32 TierIndex) const
{
	return FindMiningTier(TierIndex);
}

const FSFMiningBuildingTierData* USFMiningBuildingDefinition::FindMiningTier(int32 TierIndex) const
{
	return Tiers.IsValidIndex(TierIndex)
	? &Tiers[TierIndex]
	: nullptr;
}
