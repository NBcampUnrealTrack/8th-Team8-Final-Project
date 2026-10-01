#include "SFBuildingSynergySubsystem.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/Synergy/SFBuildingSynergyDefinition.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Interfaces/Building/SFProductionRateModifiable.h"
#include "Interfaces/Units/Targetable.h"

namespace
{
	const FName BuildingSynergyModifierId(TEXT("BuildingSynergy"));

	struct FSFSynergySourceEntry
	{
		TWeakObjectPtr<ASFBaseBuilding> Building;
		TObjectPtr<USFBuildingSynergyDefinition> Definition = nullptr;
		FBuildingGridInfo GridInfo;
		FIntPoint ClusterLinkGridSize = FIntPoint(1, 1);
	};

	// DataAsset의 로컬 셀 크기를 실제 건물 회전에 맞는 월드 격자 X/Y 크기로 변환합니다.
	FIntPoint GetRotatedGridSizeForBuilding(
		const FIntPoint& LocalGridSize,
		const ASFBaseBuilding* SourceBuilding)
	{
		const FIntPoint SafeLocalGridSize(
			FMath::Max(1, LocalGridSize.X),
			FMath::Max(1, LocalGridSize.Y));

		return IsValid(SourceBuilding)
			? USFBuildingGridSubsystem::GetRotatedGridSize(
				SafeLocalGridSize,
				SourceBuilding->GetActorRotation())
			: SafeLocalGridSize;
	}

	// 기존 GridSubsystem의 짝수·홀수 배치 계산을 재사용하여 점유 영역 중심에 새 영역을 배치합니다.
	FBuildingGridInfo MakeCenteredGridInfo(
		const USFBuildingGridSubsystem* Grid,
		const FBuildingGridInfo& CenterGridInfo,
		const FIntPoint& DesiredGridSize)
	{
		const FIntPoint SafeGridSize(
			FMath::Max(1, DesiredGridSize.X),
			FMath::Max(1, DesiredGridSize.Y));

		return Grid->WorldCenterToGridInfo(
			Grid->GridInfoToWorldCenter(CenterGridInfo),
			SafeGridSize);
	}

	// 두 건물 중 어느 한쪽의 링크 영역에 상대 건물 중심이 포함되면 무방향 연결로 처리합니다.
	bool AreBuildingsLinked(
		const USFBuildingGridSubsystem* Grid,
		const FBuildingGridInfo& Left,
		const FIntPoint& LeftLinkSize,
		const FBuildingGridInfo& Right,
		const FIntPoint& RightLinkSize)
	{
		if (!IsValid(Grid))
		{
			return false;
		}

		return MakeCenteredGridInfo(Grid, Left, LeftLinkSize).ContainsCenterOf(Right) ||
			MakeCenteredGridInfo(Grid, Right, RightLinkSize).ContainsCenterOf(Left);
	}

	// 클러스터의 전체 점유 격자 중앙과 가장 높은 건물 상단을 조합해 UI 배지 기준점을 계산합니다.
	FVector CalculateClusterBadgeWorldLocation(
		const USFBuildingGridSubsystem* Grid,
		const TArray<FSFSynergySourceEntry>& SourceEntries,
		const TArray<int32>& ClusterIndices,
		float HeightOffset)
	{
		if (!IsValid(Grid))
		{
			return FVector::ZeroVector;
		}

		bool bHasBounds = false;
		FIntPoint MinCell = FIntPoint::ZeroValue;
		FIntPoint MaxExclusive = FIntPoint::ZeroValue;
		float HighestBuildingTop = 0.f;

		for (const int32 SourceIndex : ClusterIndices)
		{
			if (!SourceEntries.IsValidIndex(SourceIndex))
			{
				continue;
			}

			const FSFSynergySourceEntry& Source = SourceEntries[SourceIndex];
			const ASFBaseBuilding* SourceBuilding = Source.Building.Get();
			if (!IsValid(SourceBuilding) || !Source.GridInfo.IsValid())
			{
				continue;
			}

			const FIntPoint SourceMaxExclusive = Source.GridInfo.GetMaxExclusive();
			const FBoxSphereBounds BuildBounds = SourceBuilding->GetBuildBounds();
			const FVector LocalBuildingTop(
				BuildBounds.Origin.X,
				BuildBounds.Origin.Y,
				BuildBounds.Origin.Z + BuildBounds.BoxExtent.Z);
			const float BuildingTop = SourceBuilding->GetActorTransform()
				.TransformPosition(LocalBuildingTop).Z;

			if (!bHasBounds)
			{
				MinCell = Source.GridInfo.OriginCell;
				MaxExclusive = SourceMaxExclusive;
				HighestBuildingTop = BuildingTop;
				bHasBounds = true;
				continue;
			}

			MinCell.X = FMath::Min(MinCell.X, Source.GridInfo.OriginCell.X);
			MinCell.Y = FMath::Min(MinCell.Y, Source.GridInfo.OriginCell.Y);
			MaxExclusive.X = FMath::Max(MaxExclusive.X, SourceMaxExclusive.X);
			MaxExclusive.Y = FMath::Max(MaxExclusive.Y, SourceMaxExclusive.Y);
			HighestBuildingTop = FMath::Max(HighestBuildingTop, BuildingTop);
		}

		if (!bHasBounds)
		{
			return FVector::ZeroVector;
		}

		FBuildingGridInfo ClusterGridInfo;
		ClusterGridInfo.OriginCell = MinCell;
		ClusterGridInfo.GridSize = MaxExclusive - MinCell;

		FVector BadgeWorldLocation = Grid->GridInfoToWorldCenter(ClusterGridInfo);
		BadgeWorldLocation.Z = HighestBuildingTop + HeightOffset;
		return BadgeWorldLocation;
	}
}

USFBuildingSynergySubsystem* USFBuildingSynergySubsystem::Get(
	const UObject* WorldContext)
{
	if (!WorldContext || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContext,
		EGetWorldErrorMode::ReturnNull);

	return IsValid(World)
		? World->GetSubsystem<USFBuildingSynergySubsystem>()
		: nullptr;
}

void USFBuildingSynergySubsystem::RegisterBuilding(ASFBaseBuilding* Building)
{
	if (!IsValid(Building))
	{
		return;
	}

	const TWeakObjectPtr<ASFBaseBuilding> WeakBuilding(Building);
	RegisteredBuildings.Add(WeakBuilding);
	RecalculateSynergies();
}

void USFBuildingSynergySubsystem::UnregisterBuilding(ASFBaseBuilding* Building)
{
	if (!Building)
	{
		return;
	}

	const TWeakObjectPtr<ASFBaseBuilding> WeakBuilding(Building);
	RegisteredBuildings.Remove(WeakBuilding);

	// EndPlay 중인 대상에는 제거 콜백을 보내지 않고 기록만 정리합니다.
	AppliedMultipliers.Remove(WeakBuilding);

	RecalculateSynergies();
}

void USFBuildingSynergySubsystem::NotifyBuildingChanged(ASFBaseBuilding* Building)
{
	if (!IsValid(Building) ||
		!RegisteredBuildings.Contains(TWeakObjectPtr<ASFBaseBuilding>(Building)))
	{
		return;
	}

	RecalculateSynergies();
}

TArray<FSFBuildingSynergyZoneInfo>
USFBuildingSynergySubsystem::GetSynergyZonesForTeam(int32 TeamIndex) const
{
	return SynergyZones.FilterByPredicate(
		[TeamIndex](const FSFBuildingSynergyZoneInfo& Zone)
		{
			return Zone.TeamIndex == TeamIndex;
		});
}

TArray<FSFBuildingSynergyClusterProgressInfo>
USFBuildingSynergySubsystem::GetClusterProgressInfosForTeam(
	int32 TeamIndex) const
{
	return ClusterProgressInfos.FilterByPredicate(
		[TeamIndex](const FSFBuildingSynergyClusterProgressInfo& ProgressInfo)
		{
			return ProgressInfo.TeamIndex == TeamIndex;
		});
}

// 팀·시너지가 같은 완성 원천 건물마다 클러스터 연결 판정용 링크 격자를 RecalculateSynergies와 같은 방식으로 계산합니다.
// UBuildingPlacement::CollectExistingLinkRangeCells가 하늘색 링크 범위를 그릴 때 호출합니다.
void USFBuildingSynergySubsystem::CollectClusterLinkGridInfos(
	int32 TeamIndex,
	const USFBuildingSynergyDefinition* SynergyDefinition,
	TArray<FBuildingGridInfo>& OutLinkGridInfos) const
{
	OutLinkGridInfos.Reset();

	const USFBuildingGridSubsystem* Grid =
		GetWorld() ? GetWorld()->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;

	if (!IsValid(Grid) || !IsValid(SynergyDefinition))
	{
		return;
	}

	// RecalculateSynergies와 같은 조건의 원천 건물만 사용해야 표시와 실제 연결 판정이 일치합니다.
	for (const TWeakObjectPtr<ASFBaseBuilding>& WeakBuilding : RegisteredBuildings)
	{
		const ASFBaseBuilding* Building = WeakBuilding.Get();
		if (!IsValid(Building) ||
			!Building->IsTierActivated() ||
			Building->GetTeamIndex() != TeamIndex)
		{
			continue;
		}

		const USFBuildingDefinition* BuildingDefinition =
			Building->GetBuildingDefinition();

		const bool bGrantsSynergy =
			IsValid(BuildingDefinition) &&
			BuildingDefinition->GrantedSynergies.ContainsByPredicate(
				[SynergyDefinition](const FSFGrantedBuildingSynergy& GrantedSynergy)
				{
					return GrantedSynergy.Definition == SynergyDefinition;
				});

		FBuildingGridInfo RegisteredGridInfo;
		if (!bGrantsSynergy ||
			!Building->TryGetRegisteredGridInfo(RegisteredGridInfo))
		{
			continue;
		}

		OutLinkGridInfos.Add(MakeCenteredGridInfo(
			Grid,
			RegisteredGridInfo,
			GetRotatedGridSizeForBuilding(
				SynergyDefinition->ClusterLinkSizeCells,
				Building)));
	}
}

float USFBuildingSynergySubsystem::GetProductionBonusPercent(
	const ASFBaseBuilding* Building) const
{
	if (!IsValid(Building))
	{
		return 0.f;
	}

	const float* Multiplier = AppliedMultipliers.Find(Building);
	return Multiplier
		? FMath::Max(0.f, (*Multiplier - 1.f) * 100.f)
		: 0.f;
}

void USFBuildingSynergySubsystem::Deinitialize()
{
	for (const TPair<TWeakObjectPtr<ASFBaseBuilding>, float>& Applied :
		AppliedMultipliers)
	{
		if (ASFBaseBuilding* Building = Applied.Key.Get())
		{
			RemoveProductionModifier(Building);
		}
	}

	AppliedMultipliers.Reset();
	RegisteredBuildings.Reset();
	SynergyZones.Reset();
	ClusterProgressInfos.Reset();
	Super::Deinitialize();
}

void USFBuildingSynergySubsystem::RecalculateSynergies()
{
	if (bIsRecalculating)
	{
		return;
	}

	TGuardValue<bool> RecalculationGuard(bIsRecalculating, true);

	USFBuildingGridSubsystem* Grid =
		GetWorld() ? GetWorld()->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;
	if (!IsValid(Grid)) return;

	for (auto It = RegisteredBuildings.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	TArray<ASFBaseBuilding*> ActiveBuildings;
	TArray<FSFSynergySourceEntry> SourceEntries;

	for (const TWeakObjectPtr<ASFBaseBuilding>& WeakBuilding : RegisteredBuildings)
	{
		ASFBaseBuilding* Building = WeakBuilding.Get();
		if (!IsValid(Building) ||
			!Building->IsTierActivated() ||
			Building->GetTeamIndex() == Targetable::NeutralTeam)
		{
			continue;
		}

		const USFBuildingDefinition* BuildingDefinition =
			Building->GetBuildingDefinition();

		if (!IsValid(BuildingDefinition))
		{
			continue;
		}

		FBuildingGridInfo RegisteredGridInfo;
		if (!Building->TryGetRegisteredGridInfo(RegisteredGridInfo))
		{
			continue;
		}

		ActiveBuildings.Add(Building);
		TSet<const USFBuildingSynergyDefinition*> SeenSynergyDefinitions;

		for (const FSFGrantedBuildingSynergy& GrantedSynergy :
			BuildingDefinition->GrantedSynergies)
		{
			if (!IsValid(GrantedSynergy.Definition) ||
				SeenSynergyDefinitions.Contains(GrantedSynergy.Definition))
			{
				continue;
			}
			SeenSynergyDefinitions.Add(GrantedSynergy.Definition);

			FSFSynergySourceEntry& Source = SourceEntries.AddDefaulted_GetRef();
			Source.Building = Building;
			Source.Definition = GrantedSynergy.Definition;
			Source.GridInfo = RegisteredGridInfo;
			Source.ClusterLinkGridSize = GetRotatedGridSizeForBuilding(
				GrantedSynergy.Definition->ClusterLinkSizeCells,
				Building);
		}
	}

	SynergyZones.Reset();
	ClusterProgressInfos.Reset();
	TSet<int32> VisitedSourceIndices;

	for (int32 SeedIndex = 0; SeedIndex < SourceEntries.Num(); ++SeedIndex)
	{
		if (VisitedSourceIndices.Contains(SeedIndex))
		{
			continue;
		}

		const FSFSynergySourceEntry& Seed = SourceEntries[SeedIndex];
		ASFBaseBuilding* SeedBuilding = Seed.Building.Get();
		USFBuildingSynergyDefinition* SynergyDefinition =
			Seed.Definition.Get();

		if (!IsValid(SeedBuilding) || !IsValid(SynergyDefinition))
		{
			VisitedSourceIndices.Add(SeedIndex);
			continue;
		}

		const int32 TeamIndex = SeedBuilding->GetTeamIndex();
		TArray<int32> PendingIndices;
		TArray<int32> ClusterIndices;
		PendingIndices.Add(SeedIndex);
		VisitedSourceIndices.Add(SeedIndex);

		while (!PendingIndices.IsEmpty())
		{
			const int32 CurrentIndex = PendingIndices.Pop(EAllowShrinking::No);
			ClusterIndices.Add(CurrentIndex);
			const FSFSynergySourceEntry& Current = SourceEntries[CurrentIndex];

			for (int32 CandidateIndex = 0;
				 CandidateIndex < SourceEntries.Num();
				 ++CandidateIndex)
			{
				if (VisitedSourceIndices.Contains(CandidateIndex))
				{
					continue;
				}

				const FSFSynergySourceEntry& Candidate = SourceEntries[CandidateIndex];
				const ASFBaseBuilding* CandidateBuilding = Candidate.Building.Get();

				if (!IsValid(CandidateBuilding) ||
					Candidate.Definition != SynergyDefinition ||
					CandidateBuilding->GetTeamIndex() != TeamIndex)
				{
					continue;
				}

				if (AreBuildingsLinked(
					Grid,
					Current.GridInfo,
					Current.ClusterLinkGridSize,
					Candidate.GridInfo,
					Candidate.ClusterLinkGridSize))
				{
					VisitedSourceIndices.Add(CandidateIndex);
					PendingIndices.Add(CandidateIndex);
				}
			}
		}

		// TSet 순회 순서와 무관하게 구역 배열 순서를 유지하도록 셀 위치로 정렬합니다.
		ClusterIndices.Sort(
			[&SourceEntries](const int32 LeftIndex, const int32 RightIndex)
			{
				const FIntPoint& LeftOrigin =
					SourceEntries[LeftIndex].GridInfo.OriginCell;
				const FIntPoint& RightOrigin =
					SourceEntries[RightIndex].GridInfo.OriginCell;

				return LeftOrigin.Y != RightOrigin.Y
					? LeftOrigin.Y < RightOrigin.Y
					: LeftOrigin.X < RightOrigin.X;
			});

		const int32 SourceCount = ClusterIndices.Num();

		// 활성 단계가 없는 1개짜리 클러스터도 다음 단계 안내 UI에 전달합니다.
		FSFBuildingSynergyClusterProgressInfo& ProgressInfo =
			ClusterProgressInfos.AddDefaulted_GetRef();
		ProgressInfo.Definition = SynergyDefinition;
		ProgressInfo.TeamIndex = TeamIndex;
		ProgressInfo.SourceBuildingCount = SourceCount;
		ProgressInfo.BadgeWorldLocation = CalculateClusterBadgeWorldLocation(
			Grid, SourceEntries, ClusterIndices,
			SynergyDefinition->BadgeHeightOffset);

		const FSFBuildingSynergyLevel* ActiveLevel =
			SynergyDefinition->FindLevel(SourceCount);

		if (!ActiveLevel)
		{
			continue;
		}

		// 활성 클러스터의 각 원천 건물을 중심으로 독립된 효과 격자를 하나씩 생성합니다.
		for (const int32 SourceIndex : ClusterIndices)
		{
			const FSFSynergySourceEntry& Source = SourceEntries[SourceIndex];
			const ASFBaseBuilding* SourceBuilding = Source.Building.Get();
			if (!IsValid(SourceBuilding))
			{
				continue;
			}

			const FIntPoint EffectSize = GetRotatedGridSizeForBuilding(
				ActiveLevel->EffectSizeCells,
				SourceBuilding);

			FSFBuildingSynergyZoneInfo& Zone =
				SynergyZones.AddDefaulted_GetRef();
			Zone.Definition = SynergyDefinition;
			Zone.TeamIndex = TeamIndex;
			Zone.SourceBuildingCount = SourceCount;
			// 방금 추가한 이 클러스터의 진행 정보 번호로 같은 클러스터 구역을 묶습니다.
			Zone.ClusterIndex = ClusterProgressInfos.Num() - 1;
			Zone.SourceBuilding = Source.Building;
			Zone.EffectGridInfo = MakeCenteredGridInfo(
				Grid, Source.GridInfo, EffectSize);
			Zone.WorldCenter = Grid->GridInfoToWorldCenter(Zone.EffectGridInfo);
			Zone.WorldCenter.Z = SourceBuilding->GetActorLocation().Z;
		}
	}

	TMap<TWeakObjectPtr<ASFBaseBuilding>, float> DesiredMultipliers;

	for (const FSFBuildingSynergyZoneInfo& Zone : SynergyZones)
	{
		const USFBuildingSynergyDefinition* SynergyDefinition = Zone.Definition;
		if (!IsValid(SynergyDefinition))
		{
			continue;
		}

		const FSFBuildingSynergyLevel* ActiveLevel =
			SynergyDefinition->FindLevel(Zone.SourceBuildingCount);
		if (!ActiveLevel)
		{
			continue;
		}

		const float Multiplier =
			1.f + FMath::Max(0.f, ActiveLevel->ProductionBonusPercent) / 100.f;

		for (ASFBaseBuilding* Target : ActiveBuildings)
		{
			if (Target->GetTeamIndex() != Zone.TeamIndex ||
				!Target->GetClass()->ImplementsInterface(
					USFProductionRateModifiable::StaticClass()))
			{
				continue;
			}

			const USFBuildingDefinition* TargetDefinition =
				Target->GetBuildingDefinition();

			if (!IsValid(TargetDefinition) ||
				!SynergyDefinition->CanAffectBuildingType(
					TargetDefinition->BuildingType))
			{
				continue;
			}

			FBuildingGridInfo TargetGridInfo;
			if (!Target->TryGetRegisteredGridInfo(TargetGridInfo) ||
				!Zone.EffectGridInfo.Overlaps(TargetGridInfo))
			{
				continue;
			}

			// 여러 원천의 효과 격자가 겹쳐도 대상별 최댓값 하나만 최종 배율로 등록합니다.
			float& DesiredMultiplier = DesiredMultipliers.FindOrAdd(
				Target, 1.f);
			DesiredMultiplier = FMath::Max(DesiredMultiplier, Multiplier);
		}
	}

	for (const TPair<TWeakObjectPtr<ASFBaseBuilding>, float>& Applied :
		AppliedMultipliers)
	{
		if (DesiredMultipliers.Contains(Applied.Key))
		{
			continue;
		}

		if (ASFBaseBuilding* Building = Applied.Key.Get())
		{
			RemoveProductionModifier(Building);
		}
	}

	for (const TPair<TWeakObjectPtr<ASFBaseBuilding>, float>& Desired :
		DesiredMultipliers)
	{
		ASFBaseBuilding* Building = Desired.Key.Get();
		const float* PreviousMultiplier = AppliedMultipliers.Find(Desired.Key);

		if (IsValid(Building) &&
			(!PreviousMultiplier ||
			 !FMath::IsNearlyEqual(*PreviousMultiplier, Desired.Value)))
		{
			ApplyProductionModifier(
				Building, Desired.Value);
		}
	}

	AppliedMultipliers = MoveTemp(DesiredMultipliers);
	OnSynergyStateChanged.Broadcast();
}

void USFBuildingSynergySubsystem::ApplyProductionModifier(
	ASFBaseBuilding* Building, float Multiplier)
{
	if (ISFProductionRateModifiable* Target =
		Cast<ISFProductionRateModifiable>(Building))
	{
		Target->SetProductionSpeedModifier(
			BuildingSynergyModifierId, Multiplier);
	}
}

void USFBuildingSynergySubsystem::RemoveProductionModifier(
	ASFBaseBuilding* Building)
{
	if (ISFProductionRateModifiable* Target =
		Cast<ISFProductionRateModifiable>(Building))
	{
		Target->RemoveProductionSpeedModifier(BuildingSynergyModifierId);
	}
}
