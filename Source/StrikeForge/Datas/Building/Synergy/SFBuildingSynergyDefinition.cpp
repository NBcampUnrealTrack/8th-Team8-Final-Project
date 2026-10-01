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
	const int32 LevelIndex = FindLevelIndex(SourceBuildingCount);
	return Levels.IsValidIndex(LevelIndex) ? &Levels[LevelIndex] : nullptr;
}

// 요구 건물 수를 넘지 않는 단계 중 요구 수가 가장 큰 단계의 인덱스를 찾습니다.
// FindLevel과 USFBuildingSynergyBadgeWidget::SetProgressInfo(레벨 번호 표시)가 사용합니다.
int32 USFBuildingSynergyDefinition::FindLevelIndex(int32 SourceBuildingCount) const
{
	int32 BestIndex = INDEX_NONE;

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const FSFBuildingSynergyLevel& Candidate = Levels[Index];
		if (Candidate.MinSourceBuildingCount > SourceBuildingCount)
		{
			continue;
		}

		if (BestIndex == INDEX_NONE ||
			Candidate.MinSourceBuildingCount > Levels[BestIndex].MinSourceBuildingCount)
		{
			BestIndex = Index;
		}
	}

	return BestIndex;
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
