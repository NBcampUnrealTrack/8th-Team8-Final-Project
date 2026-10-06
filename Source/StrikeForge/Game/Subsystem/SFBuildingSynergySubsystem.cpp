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
			Grid->GridInfoToWorldCenter(CenterGridInfo), SafeGridSize);
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

	// 배지 기준 건물의 격자 중심 X·Y와 클러스터에서 가장 높은 건물 상단 + 높이 오프셋으로 UI 배지 기준점을 계산합니다.
	// RecalculateSynergies가 클러스터 진행 정보(BadgeWorldLocation)를 만들 때 기준 건물을 정한 뒤 호출합니다.
	FVector CalculateClusterBadgeWorldLocation(
		const USFBuildingGridSubsystem* Grid,
		const TArray<FSFSynergySourceEntry>& SourceEntries,
		const TArray<int32>& ClusterIndices,
		int32 AnchorSourceIndex,
		float HeightOffset)
	{
		if (!IsValid(Grid) ||
			!SourceEntries.IsValidIndex(AnchorSourceIndex) ||
			!SourceEntries[AnchorSourceIndex].GridInfo.IsValid())
		{
			return FVector::ZeroVector;
		}

		bool bHasBuildingTop = false;
		float HighestBuildingTop = 0.f;

		for (const int32 SourceIndex : ClusterIndices)
		{
			if (!SourceEntries.IsValidIndex(SourceIndex))
			{
				continue;
			}

			const ASFBaseBuilding* SourceBuilding = SourceEntries[SourceIndex].Building.Get();
			if (!IsValid(SourceBuilding))
			{
				continue;
			}

			const FBoxSphereBounds BuildBounds = SourceBuilding->GetBuildBounds();
			const FVector LocalBuildingTop(
				BuildBounds.Origin.X,
				BuildBounds.Origin.Y,
				BuildBounds.Origin.Z + BuildBounds.BoxExtent.Z);
			const float BuildingTop = SourceBuilding->GetActorTransform()
				.TransformPosition(LocalBuildingTop).Z;

			HighestBuildingTop = bHasBuildingTop
				? FMath::Max(HighestBuildingTop, BuildingTop)
				: BuildingTop;
			bHasBuildingTop = true;
		}

		if (!bHasBuildingTop)
		{
			return FVector::ZeroVector;
		}

		FVector BadgeWorldLocation =
			Grid->GridInfoToWorldCenter(SourceEntries[AnchorSourceIndex].GridInfo);
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

// 건물 beginplay시 호출
void USFBuildingSynergySubsystem::RegisterBuilding(ASFBaseBuilding* Building)
{
	if (!IsValid(Building))
	{
		return;
	}

	bool bAlreadyRegistered = false;
	RegisteredBuildings.Add(
		TWeakObjectPtr<ASFBaseBuilding>(Building),
		&bAlreadyRegistered);

	// 미완성 건물(배치 고스트·건설 중)은 계산에 들어가지 않으므로 결과가 바뀌지 않습니다.
	// 완성되면 FinalizeTierActivation → NotifyBuildingChanged에서 다시 계산됩니다.
	if (!bAlreadyRegistered && Building->IsTierActivated())
	{
		MarkSynergiesDirty();
	}
}
// 건물이 Endplay할때(파괴되었을때) 호출
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

	// 배지가 이 건물 위에 있었다면 재계산에서 클러스터의 남은 건물로 옮겨지고, 남은 건물이 없으면 배지가 사라집니다.
	BadgeAnchorStamps.Remove(WeakBuilding);

	// 이미 비활성화된 건물(고스트 취소, 파괴 시 DeactivateActiveTier에서 계산 완료)은 다시 계산할 필요가 없습니다.
	if (Building->IsTierActivated())
	{
		MarkSynergiesDirty();
	}
}

// 선택한 시너지 원천 건물을 배지 기준으로 기록하고, 전체 재계산 없이 소속 클러스터 배지의 X·Y만 그 건물 위로 옮깁니다.
// ASFInGamePlayerController::SetSelectedActor가 내 팀 건물을 선택했을 때 호출합니다.
void USFBuildingSynergySubsystem::SelectBadgeAnchorBuilding(ASFBaseBuilding* Building)
{
	const TWeakObjectPtr<ASFBaseBuilding> WeakBuilding(Building);

	// 마지막 재계산에서 클러스터에 들어간 원천 건물만 배지 기준이 될 수 있습니다(미완성·일반 건물 제외).
	const TArray<int32>* ClusterIndices = BuildingClusterIndices.Find(WeakBuilding);
	const USFBuildingGridSubsystem* Grid =
		GetWorld() ? GetWorld()->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;

	FBuildingGridInfo RegisteredGridInfo;
	if (!IsValid(Building) ||
		!ClusterIndices ||
		!IsValid(Grid) ||
		!Building->TryGetRegisteredGridInfo(RegisteredGridInfo))
	{
		return;
	}

	// 이후 건설·파괴로 재계산돼도 이 건물이 배지 기준으로 유지되도록 순번을 기록합니다.
	BadgeAnchorStamps.Add(WeakBuilding, NextBadgeAnchorStamp++);

	// Z(클러스터에서 가장 높은 건물 상단)는 기준 건물과 무관하므로 X·Y만 바꿉니다.
	const FVector AnchorCenter = Grid->GridInfoToWorldCenter(RegisteredGridInfo);
	bool bBadgeMoved = false;

	for (const int32 ProgressInfoIndex : *ClusterIndices)
	{
		if (!ClusterProgressInfos.IsValidIndex(ProgressInfoIndex))
		{
			continue;
		}

		FVector& BadgeWorldLocation =
			ClusterProgressInfos[ProgressInfoIndex].BadgeWorldLocation;

		if (!FMath::IsNearlyEqual(BadgeWorldLocation.X, AnchorCenter.X) ||
			!FMath::IsNearlyEqual(BadgeWorldLocation.Y, AnchorCenter.Y))
		{
			BadgeWorldLocation.X = AnchorCenter.X;
			BadgeWorldLocation.Y = AnchorCenter.Y;
			bBadgeMoved = true;
		}
	}

	// 이미 배지가 위에 있는 건물을 다시 클릭한 경우에는 알리지 않습니다.
	if (bBadgeMoved)
	{
		OnSynergyStateChanged.Broadcast();
	}
}
// 건물이 완공/업그레이드시 호출
void USFBuildingSynergySubsystem::NotifyBuildingChanged(ASFBaseBuilding* Building)
{
	if (!IsValid(Building) ||
		!RegisteredBuildings.Contains(TWeakObjectPtr<ASFBaseBuilding>(Building)))
	{
		return;
	}

	MarkSynergiesDirty();
}

// 여러 건물을 한꺼번에 등록·변경하는 구간을 시작합니다. 구간 안의 변경은 재계산 필요 표시만 남깁니다.
// ASFGameMode::SpawnStageBuildingsFromAnchors 시작 시 호출하며, 같은 함수의 ON_SCOPE_EXIT가 EndUpdateBatch와 짝을 맞춥니다.
void USFBuildingSynergySubsystem::BeginUpdateBatch()
{
	++UpdateBatchDepth;
}

// 가장 바깥 구간이 끝나면 구간 동안 쌓인 변경을 FlushDirtySynergies로 한 번에 계산합니다.
// ASFGameMode::SpawnStageBuildingsFromAnchors가 끝날 때 ON_SCOPE_EXIT로 호출됩니다(중간 return 포함).
void USFBuildingSynergySubsystem::EndUpdateBatch()
{
	if (!ensureMsgf(UpdateBatchDepth > 0, TEXT("EndUpdateBatch가 BeginUpdateBatch보다 많이 호출됐습니다.")))
	{
		return;
	}

	--UpdateBatchDepth;

	if (UpdateBatchDepth == 0)
	{
		FlushDirtySynergies();
	}
}

// 시너지 결과가 바뀔 수 있음을 기록하고, Batch 구간이 아니면 즉시 계산합니다.
// RegisterBuilding·UnregisterBuilding·NotifyBuildingChanged가 호출합니다.
void USFBuildingSynergySubsystem::MarkSynergiesDirty()
{
	bSynergyDirty = true;

	if (UpdateBatchDepth == 0)
	{
		FlushDirtySynergies();
	}
}

// 기록된 변경이 있을 때만 RecalculateSynergies를 한 번 실행합니다.
// 계산 도중 들어온 변경은 표시를 남겨 두어 다음 Flush에서 반영합니다.
void USFBuildingSynergySubsystem::FlushDirtySynergies()
{
	if (!bSynergyDirty || bIsRecalculating)
	{
		return;
	}

	bSynergyDirty = false;
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

// 지정한 격자에 그 종류의 건물이 있다고 할 때 받을 생산 증가율(%)을 현재 구역으로 계산합니다.
// 같은 시너지는 최고값, 다른 시너지끼리는 합산하며 RecalculateSynergies와 UBuildingPlacement(배치 미리보기)가 함께 씁니다.
float USFBuildingSynergySubsystem::CalculateProductionBonusPercentAt(
	int32 TeamIndex,
	ESFBuildingType BuildingType,
	const FBuildingGridInfo& GridInfo) const
{
	if (!GridInfo.IsValid())
	{
		return 0.f;
	}

	TMap<const USFBuildingSynergyDefinition*, float> HighestBonusBySynergy;

	for (const FSFBuildingSynergyZoneInfo& Zone : SynergyZones)
	{
		const USFBuildingSynergyDefinition* SynergyDefinition = Zone.Definition;

		if (Zone.TeamIndex != TeamIndex ||
			!IsValid(SynergyDefinition) ||
			!SynergyDefinition->CanAffectBuildingType(BuildingType) ||
			!Zone.EffectGridInfo.Overlaps(GridInfo))
		{
			continue;
		}

		const FSFBuildingSynergyLevel* ActiveLevel =
			SynergyDefinition->FindLevel(Zone.SourceBuildingCount);
		if (!ActiveLevel)
		{
			continue;
		}

		// 같은 시너지의 구역끼리는 가장 높은 증가율 하나만 남깁니다.
		float& HighestBonus = HighestBonusBySynergy.FindOrAdd(SynergyDefinition, 0.f);
		HighestBonus = FMath::Max(
			HighestBonus,
			FMath::Max(0.f, ActiveLevel->ProductionBonusPercent));
	}

	// 시너지 종류별 최고값을 더합니다(예: 군사 +20% + 사격 +20% = +40%).
	float TotalBonusPercent = 0.f;

	for (const TPair<const USFBuildingSynergyDefinition*, float>& SynergyBonus :
		HighestBonusBySynergy)
	{
		TotalBonusPercent += SynergyBonus.Value;
	}

	return TotalBonusPercent;
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
	BuildingClusterIndices.Reset();
	BadgeAnchorStamps.Reset();
	NextBadgeAnchorStamp = 0;
	UpdateBatchDepth = 0;
	bSynergyDirty = false;
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
	// 현재 Beginplay에서 건물을 전부등록
	// 다시 함수가 호출될때 살아있는지 계속 검증 = 죽은건물참조정리
	for (auto It = RegisteredBuildings.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent(); // 현재 이터레이터 정리 API
		}
	}

	// 사라진 건물의 배지 기준 기록도 정리합니다.
	for (auto It = BadgeAnchorStamps.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// 살아있는건물 수집(타겟)
	TArray<ASFBaseBuilding*> ActiveBuildings;
	// 현재클래스의 사용자정의구조체 ( 건물Ref + Definition + Grid정보 + ClusterGrid정보 )
	TArray<FSFSynergySourceEntry> SourceEntries;

	// 죽은건물제외 건물순회
	for (const TWeakObjectPtr<ASFBaseBuilding>& WeakBuilding : RegisteredBuildings)
	{
		ASFBaseBuilding* Building = WeakBuilding.Get();
		if (!IsValid(Building) ||
			!Building->IsTierActivated() ||
			Building->GetTeamIndex() == Targetable::NeutralTeam)
		{
			continue;
		}

		// 건물 정의안에 시너지정의있음
		const USFBuildingDefinition* BuildingDefinition =
			Building->GetBuildingDefinition();

		if (!IsValid(BuildingDefinition))
		{
			continue;
		}

		// 격자 등록된게 없으면 return
		// 건물 Beginplay에서 격자등록을 마친후 SynergySubsystem이 호출되야함
		FBuildingGridInfo RegisteredGridInfo;
		if (!Building->TryGetRegisteredGridInfo(RegisteredGridInfo))
		{
			continue;
		}

		// 중립건물 아니고 + 건물정의존재하고 + 격자등록 완료했으면 추가 
		ActiveBuildings.Add(Building);
		// 중복된 시너지 거름
		TSet<const USFBuildingSynergyDefinition*> SeenSynergyDefinitions;

		// 건물의 Definition을 가져와서 시너지DA정보 추출
		for (const FSFGrantedBuildingSynergy& GrantedSynergy :
			BuildingDefinition->GrantedSynergies) // 시너지DA를 담은 배열
		{
			if (!IsValid(GrantedSynergy.Definition) || // 시너지 DA
				SeenSynergyDefinitions.Contains(GrantedSynergy.Definition))
			{
				continue;
			}
			// 새롭게 추가된 시너지만 추가
			SeenSynergyDefinitions.Add(GrantedSynergy.Definition);
			
			// Source시너지에 하나씩 추가
			FSFSynergySourceEntry& Source = SourceEntries.AddDefaulted_GetRef();
			Source.Building = Building;
			Source.Definition = GrantedSynergy.Definition;
			Source.GridInfo = RegisteredGridInfo;
			Source.ClusterLinkGridSize = GetRotatedGridSizeForBuilding(
				GrantedSynergy.Definition->ClusterLinkSizeCells, Building);
		}
	}

	// 이전 재계산에서 만든 효과 격자를 지웁니다.
	// 이번 월드 상태를 기준으로 다시 생성합니다.
	SynergyZones.Reset();
	// 아직 활성화되지 않은 1개짜리 구역까지 포함하는 클러스터 [*UI 진행 정보*]
	// 이전 HUD 배지 정보를 지웁니다.
	// 이번 클러스터 상태를 기준으로 다시 생성합니다.
	ClusterProgressInfos.Reset();
	BuildingClusterIndices.Reset();

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
				// Current의 링크 영역 안에 Candidate의 중심이 있음
				// 또는 Candidate의 링크 영역 안에 Current의 중심이 있음 > true
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

		// 건물 클릭 시 전체 재계산 없이 이 클러스터 배지만 옮길 수 있도록 구성 건물마다 진행 정보 번호를 기록합니다.
		const int32 ProgressInfoIndex = ClusterProgressInfos.Num() - 1;
		for (const int32 SourceIndex : ClusterIndices)
		{
			BuildingClusterIndices.FindOrAdd(SourceEntries[SourceIndex].Building)
				.Add(ProgressInfoIndex);
		}

		// 배지 기준 건물은 기록 순번이 가장 큰 건물입니다(처음 생긴 클러스터의 첫 건물, 이후 클릭한 건물).
		// 기록된 건물이 없으면(새 클러스터, 기준 건물 파괴) 격자 순서 첫 건물을 새로 기록합니다.
		int32 AnchorSourceIndex = INDEX_NONE;
		int32 LatestAnchorStamp = INDEX_NONE;

		for (const int32 SourceIndex : ClusterIndices)
		{
			const int32* AnchorStamp =
				BadgeAnchorStamps.Find(SourceEntries[SourceIndex].Building);

			if (AnchorStamp && *AnchorStamp > LatestAnchorStamp)
			{
				LatestAnchorStamp = *AnchorStamp;
				AnchorSourceIndex = SourceIndex;
			}
		}

		if (AnchorSourceIndex == INDEX_NONE && !ClusterIndices.IsEmpty())
		{
			AnchorSourceIndex = ClusterIndices[0];
			BadgeAnchorStamps.Add(
				SourceEntries[AnchorSourceIndex].Building,
				NextBadgeAnchorStamp++);
		}

		ProgressInfo.BadgeWorldLocation = CalculateClusterBadgeWorldLocation(
			Grid, SourceEntries, ClusterIndices, AnchorSourceIndex,
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
			Zone.ClusterIndex = ProgressInfoIndex;
			Zone.SourceBuilding = Source.Building;
			Zone.EffectGridInfo = MakeCenteredGridInfo(
				Grid, Source.GridInfo, EffectSize);
			Zone.WorldCenter = Grid->GridInfoToWorldCenter(Zone.EffectGridInfo);
			Zone.WorldCenter.Z = SourceBuilding->GetActorLocation().Z;
		}
	}

	// 시너지 부여: 대상 건물마다 방금 만든 구역으로 최종 증가율을 구합니다.
	// 배치 미리보기도 같은 CalculateProductionBonusPercentAt을 쓰므로 미리보기 값과 실제 버프가 항상 같습니다.
	TMap<TWeakObjectPtr<ASFBaseBuilding>, float> DesiredMultipliers;

	for (ASFBaseBuilding* Target : ActiveBuildings)
	{
		if (!Target->GetClass()->ImplementsInterface(
				USFProductionRateModifiable::StaticClass()))
		{
			continue;
		}

		const USFBuildingDefinition* TargetDefinition =
			Target->GetBuildingDefinition();

		FBuildingGridInfo TargetGridInfo;
		if (!IsValid(TargetDefinition) ||
			!Target->TryGetRegisteredGridInfo(TargetGridInfo))
		{
			continue;
		}

		const float BonusPercent = CalculateProductionBonusPercentAt(
			Target->GetTeamIndex(),
			TargetDefinition->BuildingType,
			TargetGridInfo);

		if (BonusPercent > 0.f)
		{
			DesiredMultipliers.Add(Target, 1.f + BonusPercent / 100.f);
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
			ApplyProductionModifier(Building, Desired.Value);
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
