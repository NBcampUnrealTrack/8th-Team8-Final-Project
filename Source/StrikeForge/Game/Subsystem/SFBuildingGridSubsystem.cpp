#include "SFBuildingGridSubsystem.h"
#include "Logging/SFLog.h"
#include "Buildings/Resource/ResourceBase.h"
#include "GameFramework/Actor.h"
#include "Buildings/Resource/Interface/SFMinable.h"
#if WITH_EDITOR
#include "EngineUtils.h"
#include "Game/Spawn/SFBuildingSpawnAnchor.h"
#endif

void USFBuildingGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	GridCells.Reset();

	SF_LOG(Log,
		"[BuildingGrid] 초기화 완료. CellSize=%.1f",
		CellSize);
}

void USFBuildingGridSubsystem::Deinitialize()
{
	SF_LOG(Log,
		"[BuildingGrid] 종료. 남은 셀=%d",
		GridCells.Num());

	GridCells.Reset();

	Super::Deinitialize();
}

#if WITH_EDITOR
void USFBuildingGridSubsystem::RefreshAllEditorGridPreviews()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld()) return;

	for (TActorIterator<ASFBuildingSpawnAnchor> It(World); It; ++It)
	{
		ASFBuildingSpawnAnchor* Anchor = *It;

		if (!IsValid(Anchor) || Anchor->IsActorBeingDestroyed())
		{
			continue;
		}

		Anchor->RefreshEditorPreview();
	}

	for (TActorIterator<AResourceBase> It(World); It; ++It)
	{
		AResourceBase* Resource = *It;

		if (!IsValid(Resource) || Resource->IsActorBeingDestroyed())
		{
			continue;
		}

		Resource->RefreshEditorGridPreview();
	}
}
#endif

FVector USFBuildingGridSubsystem::GridToWorldCoordinate(const FIntPoint& GridCoord) const
{
	return GridOrigin + FVector(
		GridCoord.X * CellSize + CellSize * 0.5f,
		GridCoord.Y * CellSize + CellSize * 0.5f,
		0.f);
}

bool USFBuildingGridSubsystem::CanOccupyBuildingCells(const AActor* Owner, const FBuildingGridInfo& GridInfo) const
{
	if (!IsValid(Owner) || !GridInfo.IsValid()) return false;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;

			const AActor* ExistingBuilding = Cell->BuildingActor.Get();
			if (IsValid(ExistingBuilding) && ExistingBuilding != Owner)
			{
				return false;
			}
		}
	}
	
	return true;
}

FSFBuildingGridEvaluationResult
USFBuildingGridSubsystem::EvaluatePlayerBuildingGrid(
	const AActor* Building,
	const FBuildingGridInfo& GridInfo) const
{
	FSFBuildingGridEvaluationResult Result;

	if (!CanRegisterBuildingGrid(Building, GridInfo))
	{
		return Result;
	}

	const IMinable* MiningBuilding = Cast<IMinable>(Building);
	// 일반 건물은 점유 격자만 통과하면 됩니다.
	if (MiningBuilding == nullptr)
	{
		Result.bCanPlaceOnGrid = true;
		return Result;
	}

	TArray<FSFMiningCellTarget> PreviewTargets;

	Result.MineableResourceAmount =
		CollectMiningCellTargets(
			Building, GridInfo, PreviewTargets);

	Result.bCanPlaceOnGrid = Result.MineableResourceAmount > 0;

	return Result;
}

int32 USFBuildingGridSubsystem::CollectMiningCellTargets(const AActor* MiningBuilding,
	const FBuildingGridInfo& BuildingGridInfo, TArray<FSFMiningCellTarget>& OutTargets) const
{
	OutTargets.Reset();

	if (!IsValid(MiningBuilding) ||	!BuildingGridInfo.IsValid())
	{
		return 0;
	}

	const IMinable* Consumer =	Cast<IMinable>(MiningBuilding);
	if (Consumer == nullptr) return 0;

	const FIntPoint EffectiveSearchGridSize =
		GetRotatedGridSize(
			Consumer->GetResourceSearchGridSize(),
			MiningBuilding->GetActorRotation());

	FBuildingGridInfo SearchGridInfo;

	if (!TryMakeCenteredSubGrid(
			BuildingGridInfo, EffectiveSearchGridSize,
			SearchGridInfo))
	{
		return 0;
	}

	OutTargets.Reserve(
		SearchGridInfo.GridSize.X *	SearchGridInfo.GridSize.Y);

	const EResourceType MineResourceType = Consumer->GetMineResourceType();
	int32 TotalResourceAmount = 0;

	for (int32 Y = 0; Y < SearchGridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < SearchGridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				SearchGridInfo.OriginCell +	FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (Cell == nullptr) continue;

			for (const FSFGridResourceEntry& Entry : Cell->Resources)
			{
				if (Entry.ResourceType != MineResourceType)	continue;

				AResourceBase* Resource = Entry.ResourceActor.Get();
				if (!IsValid(Resource))	continue;

				// 검사와 합산에서 같은 값을 재사용합니다.
				const int32 CellDeposit =
					Resource->GetCellDeposit(Entry.ResourceCellIndex);
				if (CellDeposit <= 0) continue;

				FSFMiningCellTarget Target;
				Target.ResourceActor	 =	Resource;
				Target.ResourceCellIndex =	Entry.ResourceCellIndex;

				OutTargets.Add(MoveTemp(Target));
				TotalResourceAmount += CellDeposit;
			}
		}
	}

	return TotalResourceAmount;
}

bool USFBuildingGridSubsystem::TryRegisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo)
{
	// ! > 누군가 점유중
	if (!CanRegisterBuildingGrid(Owner, GridInfo))
	{
		return false;
	}
	// 점유 등록
	IMinable* MiningBuilding = Cast<IMinable>(Owner);

	TArray<FSFMiningCellTarget> MiningTargets;

	/*
	 * 광산이라면 GridCells를 변경하기 전에
	 * 실제 채굴 가능한 셀을 다시 검사합니다.
	 *
	 * Preview 검사 이후 자원 상태가 바뀌었을 수도 있으므로
	 * 실제 등록 시점에도 재검사하는 것이 안전합니다.
	 */
	if (MiningBuilding)
	{
		const int32 TotalResourceAmount =
			CollectMiningCellTargets(Owner, GridInfo,MiningTargets);

		if (TotalResourceAmount <= 0)
		{
			return false;
		}
	}

	// 건물 점유 셀을 등록합니다.
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			FSFGridCellData& Cell =
				GridCells.FindOrAdd(CellCoordinate);

			Cell.BuildingActor = Owner;
		}
	}

	/*
	 * 일반 건물은 여기서 등록 완료입니다.
	 * 광산만 수집된 채굴 셀 목록을 전달합니다.
	 */
	if (!MiningBuilding)
	{
		return true;
	}

	if (!MiningBuilding->SetMiningTargets(MiningTargets))
	{
		/*
		 * 수집 이후 실제 전달 시점에 자원이 고갈되는 등의
		 * 이유로 등록에 실패했다면 점유 등록을 되돌립니다.
		 */
		UnregisterBuildingGrid(Owner, GridInfo);
		return false;
	}

	return true;
}

void USFBuildingGridSubsystem::UnregisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo)
{
	if (!IsValid(Owner) || !GridInfo.IsValid()) return;
	// 자원격자 해제
	if (IMinable* MiningBuilding = Cast<IMinable>(Owner))
	{
		MiningBuilding->ClearMiningTargets();
	}
	
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate = GridInfo.OriginCell + FIntPoint(X, Y);

			FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;

			if (!Cell->BuildingActor.IsValid() ||
				Cell->BuildingActor.Get() == Owner)
			{
				Cell->BuildingActor.Reset();
			}

			if (Cell->IsEmpty())
			{
				GridCells.Remove(
					CellCoordinate);
			}
		}
	}
}

bool USFBuildingGridSubsystem::TryRegisterResourceGrid(AResourceBase* Resource, const FBuildingGridInfo& GridInfo)
{
	// 첫 단계에서 모든 셀을 검사합니다.
	// 실패하면 GridCells는 전혀 변경되지 않습니다.
	if (!CanRegisterResourceGrid(Resource, GridInfo)) return false;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const int32 LinearIndex =
				Y * GridInfo.GridSize.X + X;

			// 분배 결과가 0인 셀은 자원 점유 셀로 등록하지 않습니다.
			if (Resource->GetCellDeposit(LinearIndex) <= 0)
			{
				continue;
			}

			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			FSFGridResourceEntry NewEntry;
			NewEntry.ResourceActor = Resource;
			NewEntry.ResourceType =	Resource->GetResourceType();
			NewEntry.ResourceCellIndex = LinearIndex;

			FSFGridCellData& Cell =	GridCells.FindOrAdd(CellCoordinate);
			Cell.Resources.Add(MoveTemp(NewEntry));
		}
	}

	return true;
}

void USFBuildingGridSubsystem::UnregisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo)
{
	if (!IsValid(Resource) || !GridInfo.IsValid()) return;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			FSFGridCellData* Cell = GridCells.Find(CellCoordinate);

			if (!Cell) continue;

			Cell->Resources.RemoveAllSwap(
				[Resource](const FSFGridResourceEntry& Entry)
				{
					return !Entry.ResourceActor.IsValid() ||
						    Entry.ResourceActor.Get() == Resource;
				});

			if (Cell->IsEmpty())
			{
				GridCells.Remove(CellCoordinate);
			}
		}
	}
}

void USFBuildingGridSubsystem::UnregisterResourceCell(
	const AResourceBase* Resource,
	const FBuildingGridInfo& GridInfo,
	int32 ResourceCellIndex)
{
	if (!IsValid(Resource) || !GridInfo.IsValid())
	{
		return;
	}

	const int32 CellCount =
		GridInfo.GridSize.X * GridInfo.GridSize.Y;

	if (ResourceCellIndex < 0 ||
		ResourceCellIndex >= CellCount)
	{
		return;
	}

	/*
	 * 등록할 때 사용했던 공식:
	 * LinearIndex = Y * GridSize.X + X
	 *
	 * 이것을 다시 X, Y로 복원합니다.
	 */
	const int32 LocalX =
		ResourceCellIndex % GridInfo.GridSize.X;

	const int32 LocalY =
		ResourceCellIndex / GridInfo.GridSize.X;

	const FIntPoint CellCoordinate =
		GridInfo.OriginCell + FIntPoint(LocalX, LocalY);

	FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
	if (!Cell) return;

	Cell->Resources.RemoveAllSwap(
		[Resource, ResourceCellIndex](
			const FSFGridResourceEntry& Entry)
		{
			return !Entry.ResourceActor.IsValid() ||
				(
					Entry.ResourceActor.Get() == Resource &&
					Entry.ResourceCellIndex == ResourceCellIndex
				);
		});

	if (Cell->IsEmpty())
	{
		GridCells.Remove(CellCoordinate);
	}
}

FIntPoint USFBuildingGridSubsystem::GetRotatedGridSize(const FIntPoint& BaseGridSize, const FRotator& WorldRotation)
{
	if (BaseGridSize.X <= 0 || BaseGridSize.Y <= 0)
	{
		return BaseGridSize;
	}

	const float NormalizedYaw =
		FRotator::NormalizeAxis(WorldRotation.Yaw);

	const int32 QuarterTurnCount =
		FMath::RoundToInt(NormalizedYaw / 90.0f);

	// 90도와 270도에서는 로컬 X축과 Y축이 월드에서 서로 교환됩니다.
	const bool bShouldSwapXY =
		FMath::Abs(QuarterTurnCount) % 2 == 1;

	return bShouldSwapXY
		? FIntPoint(BaseGridSize.Y,	BaseGridSize.X)
		: BaseGridSize;
}

FVector USFBuildingGridSubsystem::GridInfoToWorldCenter(const FBuildingGridInfo& GridInfo) const
{
	if (!GridInfo.IsValid())
	{
		return GridOrigin;
	}
	
	const FVector OriginCellCenter =
		GridToWorldCoordinate(GridInfo.OriginCell);

	return OriginCellCenter + FVector(
		(GridInfo.GridSize.X - 1) * CellSize * 0.5f,
		(GridInfo.GridSize.Y - 1) * CellSize * 0.5f,
		0.0f);
}

// 월드 중심과 점유 크기로 첫 번째 점유 셀을 계산합니다.
FBuildingGridInfo USFBuildingGridSubsystem::WorldCenterToGridInfo(const FVector& WorldCenter, const FIntPoint& InGridSize) const
{
	FBuildingGridInfo Result;
	Result.GridSize = InGridSize;

	if (!Result.IsValid() || CellSize <= 0.0f)
	{
		return Result;
	}
	
	const FVector LocalCenter =
		(WorldCenter - GridOrigin) / CellSize;
	
	// 점유 크기의 절반을 빼서 중심 위치를 첫 번째 점유 셀로 변환합니다.
	Result.OriginCell = FIntPoint(
		FMath::RoundToInt(
			LocalCenter.X - InGridSize.X * 0.5f),
		FMath::RoundToInt(
			LocalCenter.Y - InGridSize.Y * 0.5f));

	return Result;
}

bool USFBuildingGridSubsystem::TrySnapWorldCenterToGrid(
	const FVector& WorldCenter,
	FIntPoint GridSize,
	FBuildingGridInfo& OutGridInfo,
	FVector& OutSnappedWorldCenter) const
{
	OutGridInfo.OriginCell = FIntPoint::ZeroValue;
	OutGridInfo.GridSize = FIntPoint::ZeroValue;
	OutSnappedWorldCenter = WorldCenter;

	if (GridSize.X <= 0 || GridSize.Y <= 0 || CellSize <= 0.0f)
	{
		return false;
	}

	OutGridInfo = WorldCenterToGridInfo(WorldCenter, GridSize);

	if (!OutGridInfo.IsValid())
	{
		return false;
	}

	OutSnappedWorldCenter = GridInfoToWorldCenter(OutGridInfo);
	OutSnappedWorldCenter.Z = WorldCenter.Z;

	return true;
}

bool USFBuildingGridSubsystem::IsCenteredSubGridSizeValid(
	const FIntPoint& OuterGridSize,
	const FIntPoint& InnerGridSize)
{
	if (OuterGridSize.X <= 0 || OuterGridSize.Y <= 0 ||
		InnerGridSize.X <= 0 || InnerGridSize.Y <= 0)
	{
		return false;
	}

	const FIntPoint SizeDifference = OuterGridSize - InnerGridSize;

	return SizeDifference.X >= 0 &&
		SizeDifference.Y >= 0 &&
		SizeDifference.X % 2 == 0 &&
		SizeDifference.Y % 2 == 0;
}

bool USFBuildingGridSubsystem::TryMakeCenteredSubGrid(const FBuildingGridInfo& OuterGridInfo,
	const FIntPoint& InnerGridSize, FBuildingGridInfo& OutInnerGridInfo) const
{
	// 실패했을 때 Out 값이 유효한 격자로 오해되지 않게 초기화합니다.
	OutInnerGridInfo.OriginCell = FIntPoint::ZeroValue;

	OutInnerGridInfo.GridSize = FIntPoint::ZeroValue;

	if (!OuterGridInfo.IsValid() ||
		!IsCenteredSubGridSizeValid(
			OuterGridInfo.GridSize,
			InnerGridSize))
	{
		return false;
	}

	const FIntPoint SizeDifference =
		OuterGridInfo.GridSize - InnerGridSize;

	OutInnerGridInfo.GridSize = InnerGridSize;

	OutInnerGridInfo.OriginCell =
		OuterGridInfo.OriginCell +
		FIntPoint(SizeDifference.X / 2,	SizeDifference.Y / 2);

	return OutInnerGridInfo.IsValid();
}

bool USFBuildingGridSubsystem::CanRegisterBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const
{
	if (!CanOccupyBuildingCells(Owner, GridInfo))
	{
		return false;
	}

	/*
	 * 광산은 자원 셀과 격자를 공유할 수 있습니다.
	 * 사용할 수 있는 자원이 있는지는
	 * CollectMiningCellTargets()에서 별도로 검사합니다.
	 */
	if (Cast<IMinable>(Owner))
	{
		return true;
	}

	// 일반 건물은 유효한 자원이 있는 셀과 겹칠 수 없습니다.
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell =
				GridCells.Find(CellCoordinate);

			if (Cell && Cell->HasValidResource())
			{
				return false;
			}
		}
	}

	return true;
}

bool USFBuildingGridSubsystem::CanRegisterResourceGrid(const AResourceBase* Resource,
	const FBuildingGridInfo& GridInfo) const
{
	if (!IsValid(Resource) ||
		!GridInfo.IsValid() ||
		Resource->GetResourceDeposit() <= 0 ||
		Resource->GetResourceGridSize() != GridInfo.GridSize)
	{
		return false;
	}

	const EResourceType NewResourceType = Resource->GetResourceType();

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const int32 LinearIndex =
				Y * GridInfo.GridSize.X + X;

			// 매장량이 있는 셀에만 자원 중첩 규칙을 적용합니다.
			if (Resource->GetCellDeposit(LinearIndex) <= 0)
			{
				continue;
			}

			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);

			if (!Cell) continue;

			/*
			 * 같은 셀에는 같은 종류의 자원을
			 * 두 번 등록할 수 없습니다.
			 *
			 * 미네랄 + 에테르: 가능
			 * 미네랄 A + 미네랄 B: 불가능
			 */
			for (const FSFGridResourceEntry& Entry : Cell->Resources)
			{
				if (!Entry.ResourceActor.IsValid())	continue;
				if (Entry.ResourceType == NewResourceType) return false;
			}

			/*
			 * 일반 건물 위에는 자원을 등록하지 않습니다.
			 * 광산은 서로 다른 자원 종류를 무시할 수 있으므로
			 * 자원과 Grid를 공유할 수 있습니다.
			 */
			AActor* ExistingBuilding = Cell->BuildingActor.Get();

			if (IsValid(ExistingBuilding) &&
				Cast<IMinable>(ExistingBuilding) == nullptr)
			{
				return false;
			}
		}
	}

	return true;
}

void USFBuildingGridSubsystem::DebugDrawOccupiedCells() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	/*
	 * 이전 배치 세션에서 그린 점유 격자를 제거한 뒤,
	 * 현재 점유 상태를 다시 그립니다.
	 */
	FlushPersistentDebugLines(World);
	
	const FVector BoxExtent(CellSize * 0.5f, CellSize * 0.5f, 10.0f);

	for (const TPair<FIntPoint,	FSFGridCellData>& GridCell : GridCells)
	{
		AActor* OccupiedActor =	GridCell.Value.BuildingActor.Get();

		if (!IsValid(OccupiedActor))
		{
			for (const FSFGridResourceEntry& Entry : GridCell.Value.Resources)
			{
				AResourceBase* Resource = Entry.ResourceActor.Get();

				if (IsValid(Resource))
				{
					OccupiedActor = Resource;
					break;
				}
			}
		}
		if (!IsValid(OccupiedActor)) continue;

		FVector CellCenter =
			GridToWorldCoordinate(GridCell.Key);

		// 점유 중인 건물의 높이를 기준으로 표시합니다.
		CellCenter.Z =
			OccupiedActor->GetActorLocation().Z + 10.0f;

		DrawDebugBox(
			World,
			CellCenter,
			BoxExtent,
			FColor::Red,
			true,
			-1,
			0,
			3.0f);
	}
}

void USFBuildingGridSubsystem::DebugDrawMiningPlacementGrid(const FBuildingGridInfo& BuildingGridInfo,
	const FIntPoint& ResourceSearchGridSize, float WorldZ, float Duration) const
{
	if (!BuildingGridInfo.IsValid()) return;
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	const float HalfCell = CellSize * 0.5f;

	const FVector BoxExtent(
		HalfCell,
		HalfCell,
		5.0f);

	auto DrawGridCells =
		[this, World, WorldZ, Duration, &BoxExtent]
		(const FBuildingGridInfo& GridInfo,
			const FColor& Color,
			float ZOffset,
			float Thickness)
		{
			for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
			{
				for (int32 X = 0; X < GridInfo.GridSize.X;	++X)
				{
					const FIntPoint CellCoordinate =
						GridInfo.OriginCell + FIntPoint(X, Y);

					FVector CellCenter = GridToWorldCoordinate(CellCoordinate);

					CellCenter.Z =	WorldZ + ZOffset;

					DrawDebugBox(
						World,
						CellCenter,
						BoxExtent,
						Color,
						false,
						Duration,
						0,
						Thickness);
				}
			}
		};

	/*
	 * 먼저 전체 건물 점유 영역을 검정색으로 그립니다.
	 * 4×6 광산이면 24개 셀이 표시됩니다.
	 */
	DrawGridCells(
		BuildingGridInfo,
		FColor::Black,
		5.0f,
		4.0f);

	FBuildingGridInfo SearchGridInfo;

	if (!TryMakeCenteredSubGrid(
		BuildingGridInfo,
		ResourceSearchGridSize,
		SearchGridInfo))
	{
		return;
	}

	/*
	 * 검색 영역을 검정색 영역보다 조금 위에 그립니다.
	 * 두 DebugBox가 겹쳐 깜빡이는 현상을 줄일 수 있습니다.
	 */
	DrawGridCells(
		SearchGridInfo,
		FColor::Green,
		8.0f,
		5.0f);
}

// 빌딩의 gridsize debug
void USFBuildingGridSubsystem::DebugDrawGridInfo(const FBuildingGridInfo& GridInfo, float WorldZ, bool bCanPlace,
	float Duration) const
{
	if (!GridInfo.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float HalfCell = CellSize * 0.5f;
	const FVector BoxExtent(HalfCell,HalfCell,5.0f);

	const FColor GridColor = bCanPlace ?
		FColor::Green: FColor::Red;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			
			const FIntPoint CellCoordinate = GridInfo.OriginCell + FIntPoint(X, Y);
			FVector CellCenter = GridToWorldCoordinate(CellCoordinate);

			CellCenter.Z = WorldZ + 5.0f;
			DrawDebugBox(World,	CellCenter,	BoxExtent,
				GridColor,
				false,
				Duration,
				0,
				3.0f);
		}
	}
}

void USFBuildingGridSubsystem::DebugDrawResourceCells(
	EResourceType ResourceType,
	float Duration) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	const bool bPersistentLines = Duration < 0.0f;
	
	const FVector BoxExtent(
		CellSize * 0.5f,CellSize * 0.5f,10.0f);

	for (const TPair<FIntPoint, FSFGridCellData>& GridCell	: GridCells)
	{
		for (const FSFGridResourceEntry& Entry	: GridCell.Value.Resources)
		{
			AResourceBase* Resource = Entry.ResourceActor.Get();

			if (!IsValid(Resource) || Entry.ResourceType != ResourceType)
			{
				continue;
			}

			const int32 RemainingDeposit =
				Resource->GetCellDeposit(Entry.ResourceCellIndex);
			if (RemainingDeposit <= 0)	continue;

			FVector CellCenter = GridToWorldCoordinate(GridCell.Key);

			CellCenter.Z = Resource->GetActorLocation().Z + 10.0f;

			DrawDebugBox(
				World,
				CellCenter,
				BoxExtent,
				FColor::Green,
				bPersistentLines,
				Duration,
				0,
				3.0f);

			FVector TextLocation = CellCenter;
			TextLocation.Z += 30.0f;

			const FString DebugText =
				FString::Printf(
					TEXT("Index: %d\nAmount: %d"),
					Entry.ResourceCellIndex,
					RemainingDeposit);

			DrawDebugString(
				World,
				TextLocation,
				DebugText,
				nullptr,
				FColor::White,
				Duration,
				false);

			/*
			 * 동일한 종류의 자원은 한 셀에
			 * 하나만 등록될 수 있습니다.
			 */
			break;
		}
	}
}
