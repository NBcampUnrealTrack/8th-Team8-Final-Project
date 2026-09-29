#include "SFBuildingSynergyDefinition.h"

const FPrimaryAssetType USFBuildingSynergyDefinition::SynergyAssetType(
	TEXT("BuildingSynergy"));

FPrimaryAssetId USFBuildingSynergyDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(SynergyAssetType, GetEffectiveSynergyId());
}

void USFBuildingSynergyDefinition::PostLoad()
{
	Super::PostLoad();
	NormalizeCellSizes();
}

#if WITH_EDITOR
void USFBuildingSynergyDefinition::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	NormalizeCellSizes();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

const FSFBuildingSynergyLevel*
USFBuildingSynergyDefinition::FindLevel(int32 SourceBuildingCount) const
{
	const FSFBuildingSynergyLevel* BestLevel = nullptr;

	for (const FSFBuildingSynergyLevel& Candidate : Levels)
	{
		if (Candidate.MinSourceBuildingCount > SourceBuildingCount)
		{
			continue;
		}

		if (!BestLevel ||
			Candidate.MinSourceBuildingCount > BestLevel->MinSourceBuildingCount)
		{
			BestLevel = &Candidate;
		}
	}

	return BestLevel;
}

const FSFBuildingSynergyLevel*
USFBuildingSynergyDefinition::FindNextLevel(int32 SourceBuildingCount) const
{
	const FSFBuildingSynergyLevel* NextLevel = nullptr;

	for (const FSFBuildingSynergyLevel& Candidate : Levels)
	{
		if (Candidate.MinSourceBuildingCount <= SourceBuildingCount)
		{
			continue;
		}

		if (!NextLevel ||
			Candidate.MinSourceBuildingCount < NextLevel->MinSourceBuildingCount)
		{
			NextLevel = &Candidate;
		}
	}

	return NextLevel;
}

int32 USFBuildingSynergyDefinition::GetMaxRequiredBuildingCount() const
{
	int32 MaxRequiredBuildingCount = 0;

	for (const FSFBuildingSynergyLevel& Level : Levels)
	{
		MaxRequiredBuildingCount = FMath::Max(
			MaxRequiredBuildingCount,
			Level.MinSourceBuildingCount);
	}

	return MaxRequiredBuildingCount;
}

bool USFBuildingSynergyDefinition::CanAffectBuildingType(
	ESFBuildingType BuildingType) const
{
	return TargetBuildingTypes.Contains(BuildingType);
}

FName USFBuildingSynergyDefinition::GetEffectiveSynergyId() const
{
	return SynergyId.IsNone() ? GetFName() : SynergyId;
}

void USFBuildingSynergyDefinition::NormalizeCellSizes()
{
	ClusterLinkSizeCells.X = FMath::Max(1, ClusterLinkSizeCells.X);
	ClusterLinkSizeCells.Y = FMath::Max(1, ClusterLinkSizeCells.Y);

	for (FSFBuildingSynergyLevel& Level : Levels)
	{
		Level.EffectSizeCells.X = FMath::Max(1, Level.EffectSizeCells.X);
		Level.EffectSizeCells.Y = FMath::Max(1, Level.EffectSizeCells.Y);
	}
}
