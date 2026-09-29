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
		FSFGrantedBuildingSynergy GrantedSynergy;
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
	TArray<FSFBuildingSynergyZoneInfo> Result;

	for (const FSFBuildingSynergyZoneInfo& Zone : SynergyZones)
	{
		if (Zone.TeamIndex == TeamIndex)
		{
			Result.Add(Zone);
		}
	}

	return Result;
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
			Source.GrantedSynergy = GrantedSynergy;
			Source.GridInfo = RegisteredGridInfo;
			Source.ClusterLinkGridSize = GetRotatedGridSizeForBuilding(
				GrantedSynergy.Definition->ClusterLinkSizeCells,
				Building);
		}
	}

	SynergyZones.Reset();
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
			Seed.GrantedSynergy.Definition.Get();

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
					Candidate.GrantedSynergy.Definition != SynergyDefinition ||
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

		const FSFBuildingSynergyLevel* ActiveLevel =
			SynergyDefinition->FindLevel(ClusterIndices.Num());

		if (!ActiveLevel)
		{
			continue;
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
			Zone.SynergyId = SynergyDefinition->GetEffectiveSynergyId();
			Zone.TeamIndex = TeamIndex;
			Zone.SourceBuildingCount = SourceCount;
			Zone.ProductionBonusPercent = ActiveLevel->ProductionBonusPercent;
			Zone.BoundaryEffect = Source.GrantedSynergy.BoundaryEffect;
			Zone.BoundaryColor = Source.GrantedSynergy.BoundaryColor;
			Zone.BadgeIcon = Source.GrantedSynergy.BadgeIcon;
			Zone.EffectGridInfo = MakeCenteredGridInfo(
				Grid, Source.GridInfo, EffectSize);
			Zone.WorldCenter = Grid->GridInfoToWorldCenter(Zone.EffectGridInfo);
			Zone.WorldCenter.Z = SourceBuilding->GetActorLocation().Z;
			Zone.WorldSize = FVector2D(
				Zone.EffectGridInfo.GridSize.X * Grid->GetCellSize(),
				Zone.EffectGridInfo.GridSize.Y * Grid->GetCellSize());
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

			const float Multiplier =
				1.f + FMath::Max(0.f, Zone.ProductionBonusPercent) / 100.f;

			// 여러 원천의 효과 격자가 겹쳐도 대상별 최댓값 하나만 최종 배율로 등록합니다.
			float& DesiredMultiplier = DesiredMultipliers.FindOrAdd(
				Target,
				1.f);
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
				Building,
				Desired.Value);
		}
	}

	AppliedMultipliers = MoveTemp(DesiredMultipliers);
	OnSynergyZonesChanged.Broadcast();
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
