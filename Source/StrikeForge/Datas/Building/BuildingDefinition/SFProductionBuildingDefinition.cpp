// Fill out your copyright notice in the Description page of Project Settings.


#include "SFProductionBuildingDefinition.h"

int32 USFProductionBuildingDefinition::GetTierCount() const
{
	return Tiers.Num();
}

const FSFBuildingTierData* USFProductionBuildingDefinition::FindTier(int32 TierIndex) const
{
	return FindProductionTier(TierIndex);
}

const FSFProductionBuildingTierData* USFProductionBuildingDefinition::FindProductionTier(int32 TierIndex) const
{
	return Tiers.IsValidIndex(TierIndex)
	? &Tiers[TierIndex]
	: nullptr;
}
