#include "SFBuildingGridSubsystem.h"
#include "Buildings/Resource/ResourceBase.h"
#include "GameFramework/Actor.h"
#include "Buildings/Resource/Interface/SFMinable.h"

void USFBuildingGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 맵마다 기준점이 0,0,0으로 시작하지않고 각자 다른 중점을 가진다면
	// 맵마다 중점을 origin에 입력을 해야하는 로직이 필요합니다.
	UE_LOG(LogTemp,	Log, TEXT("BuildingGridSubsystem Initialized. CellSize: %.1f"),	CellSize);
}

void USFBuildingGridSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("BuildingGridSubsystem Deinitialized."));

	Super::Deinitialize();
}

FVector USFBuildingGridSubsystem::GridToWorldCoordinate(const FIntPoint& GridCoord) const
{
	return GridOrigin + FVector(
	GridCoord.X * CellSize + CellSize * 0.5f,
	GridCoord.Y * CellSize + CellSize * 0.5f,
	0.f);
}

bool USFBuildingGridSubsystem::CanPlaceBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const
{
	// gridsize = 차지할 공간길이(length) 절대 음수 불가
	if (!IsValid(Owner) || !GridInfo.IsValid()) return false;

	for (int Y = 0; Y < GridInfo.GridSize.Y; Y++)
	{
		for (int X = 0; X < GridInfo.GridSize.X; X++)
		{	// X는 X값에 Y는 Y값에 더해주는 +연산자 오버로딩 있음
			const FIntPoint TempCoordinate = GridInfo.OriginCell + FIntPoint(X, Y); 
			// TMap에서 Key값으로 값이 존재하는지 검사
			const FSFGridCellData* Cell = GridCells.Find(TempCoordinate);
			if (!Cell) continue;
			// TMap에서 존재하면 점유중, 본인이 본인자리에 배치는 가능(예외), 갑자기 nullptr 방지(destroy)
			// 가장 첫번째 조건을 검사함 > nullptr이면 isvalid 오류남 nullptr검사부터
			AActor* ExistingBuilding = Cell->BuildingActor.Get();
			if (IsValid(ExistingBuilding) && ExistingBuilding != Owner)
			{
				return false;
			}
		}
	}
	
	return true;
}

bool USFBuildingGridSubsystem::CanPlacePlayerBuildingGrid(const AActor* Building,
	const FBuildingGridInfo& GridInfo) const
{
	if (!CanRegisterBuildingGrid(Building, GridInfo))
	{
		return false;
	}

	const IMinable* Consumer =
		Cast<IMinable>(Building);

	// 일반 건물은 건물 격자 검사 결과만 사용합니다.
	if (!Consumer)
	{
		return true;
	}

	// 광산은 중앙 검색 영역에 채굴 가능한 셀이
	// 하나 이상 있어야 배치할 수 있습니다.
	TArray<FSFMiningCellTarget> MiningTargets;

	return CollectMiningCellTargets(
		Building,
		GridInfo,
		MiningTargets);
}

bool USFBuildingGridSubsystem::CollectMiningCellTargets(const AActor* MiningBuilding,
	const FBuildingGridInfo& BuildingGridInfo, TArray<FSFMiningCellTarget>& OutTargets) const
{
	OutTargets.Reset();

	if (!IsValid(MiningBuilding) ||	!BuildingGridInfo.IsValid())
	{
		return false;
	}

	const IMinable* Consumer = Cast<IMinable>(MiningBuilding);
	if (!Consumer) return false;

	FBuildingGridInfo SearchGridInfo;

	if (!TryMakeCenteredSubGrid(
			BuildingGridInfo,
			Consumer->GetResourceSearchGridSize(),
			SearchGridInfo))
	{
		return false;
	}

	/*
	 * 같은 종류의 자원은 한 셀에 하나만 존재하므로
	 * 광산이 취급하는 자원 대상 수도 최대 검색 셀 수입니다.
	 */
	OutTargets.Reserve(
		SearchGridInfo.GridSize.X *
		SearchGridInfo.GridSize.Y);

	for (int32 Y = 0; Y < SearchGridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < SearchGridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				SearchGridInfo.OriginCell +	FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);

			if (!Cell) continue;

			for (const FSFGridResourceEntry& Entry : Cell->Resources)
			{
				AResourceBase* Resource = Entry.ResourceActor.Get();
				if (!IsValid(Resource))	continue;

				const IBeMinedable* MineableResource = Cast<IBeMinedable>(Resource);
				if (!MineableResource) continue;

				// 광산이 취급하는 종류와 같은 자원만 수집합니다.
				if (MineableResource->GetResourceType() !=
					Consumer->GetMineResourceType())
				{
					continue;
				}

				// 고갈됐거나 잘못된 인덱스는 0을 반환합니다.
				if (Resource->GetCellDeposit(Entry.ResourceCellIndex) <= 0)
				{
					continue;
				}

				FSFMiningCellTarget Target;
				Target.ResourceActor = Resource;
				Target.ResourceCellIndex =	Entry.ResourceCellIndex;

				/*
				 * AddUnique를 사용하지 않습니다.
				 * 같은 ResourceActor라도 서로 다른 셀은
				 * 별개의 채굴 대상입니다.
				 */
				OutTargets.Add(MoveTemp(Target));
			}
		}
	}

	/*
	 * 셀별 남은 매장량이 적은 대상부터 채굴하도록 정렬합니다.
	 * 매장량은 복사본이 아니라 ResourceBase에서 직접 읽습니다.
	 */
	OutTargets.Sort(
		[](const FSFMiningCellTarget& Left,
		   const FSFMiningCellTarget& Right)
		{
			AResourceBase* LeftResource =
				Left.ResourceActor.Get();

			AResourceBase* RightResource =
				Right.ResourceActor.Get();

			if (!IsValid(LeftResource))
			{
				return false;
			}

			if (!IsValid(RightResource))
			{
				return true;
			}

			return LeftResource->GetCellDeposit(
					   Left.ResourceCellIndex)
				< RightResource->GetCellDeposit(
					   Right.ResourceCellIndex);
		});

	return OutTargets.Num() > 0;
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
	if (MiningBuilding &&
		!CollectMiningCellTargets(
			Owner, GridInfo,MiningTargets))
	{
		return false;
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
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const int32 LinearIndex =
				Y * GridInfo.GridSize.X + X;

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

FVector USFBuildingGridSubsystem::GridInfoToWorldCenter(const FBuildingGridInfo& GridInfo) const
{	// GridInfo.OriginCell = WorldCenterToGridInfo에서 계산한값
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

// 건물의 월드 중심과 점유 크기를 바탕으로, 해당 건물이 점유해야 할 첫 번째 Grid 셀 좌표를 계산한다.
FBuildingGridInfo USFBuildingGridSubsystem::WorldCenterToGridInfo(const FVector& WorldCenter, const FIntPoint& InGridSize) const
{	// vector = mousecursor, size = buildingGridsize
	FBuildingGridInfo Result;
	Result.GridSize = InGridSize;

	if (!Result.IsValid() || CellSize <= 0.0f)
	{
		return Result;
	}
	
	// Fintpoint
	const FVector LocalCenter =
		(WorldCenter - GridOrigin) / CellSize;
	
	// Size까지 고려해 중점 맞추고 Fintpoint 반환
	Result.OriginCell = FIntPoint(
		FMath::RoundToInt(
			LocalCenter.X - InGridSize.X * 0.5f),
		FMath::RoundToInt(
			LocalCenter.Y - InGridSize.Y * 0.5f));

	return Result;
}

bool USFBuildingGridSubsystem::TryMakeCenteredSubGrid(const FBuildingGridInfo& OuterGridInfo,
	const FIntPoint& InnerGridSize, FBuildingGridInfo& OutInnerGridInfo) const
{
	// 실패했을 때 Out 값이 유효한 격자로 오해되지 않게 초기화합니다.
	OutInnerGridInfo.OriginCell = FIntPoint::ZeroValue;

	OutInnerGridInfo.GridSize = FIntPoint::ZeroValue;

	if (!OuterGridInfo.IsValid() || InnerGridSize.X <= 0 ||
		InnerGridSize.Y <= 0)
	{
		return false;
	}

	const FIntPoint SizeDifference =
		OuterGridInfo.GridSize - InnerGridSize;

	// 검색 격자가 건물 격자보다 클 수 없습니다.
	if (SizeDifference.X < 0 ||	SizeDifference.Y < 0)
	{
		return false;
	}

	/*
	 * 차이가 홀수이면 양쪽에 같은 수의 셀을 둘 수 없습니다.
	 *
	 * 4 - 3 = 1
	 * 왼쪽 0.5칸, 오른쪽 0.5칸 배치는 불가능합니다.
	 */
	if (SizeDifference.X % 2 != 0 || SizeDifference.Y % 2 != 0)
	{
		return false;
	}

	OutInnerGridInfo.GridSize = InnerGridSize;

	OutInnerGridInfo.OriginCell =
		OuterGridInfo.OriginCell +
		FIntPoint(SizeDifference.X / 2,	SizeDifference.Y / 2);

	return OutInnerGridInfo.IsValid();
}

bool USFBuildingGridSubsystem::CanRegisterBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const
{
	if (!CanPlaceBuildingGrid(Owner, GridInfo))	return false;

	const bool bIsMiningBuilding = Cast<IMinable>(Owner) != nullptr;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;

			for (const FSFGridResourceEntry& Entry : Cell->Resources)
			{
				if (!Entry.ResourceActor.IsValid())	continue;
				// 일반 건물은 자원과 겹칠 수 없습니다.
				if (!bIsMiningBuilding)	return false;
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

// void USFBuildingGridSubsystem::DebugDrawRadius(const FVector& WorldLocation, float Radius)
// {
// 	const FIntPoint VecToGrid = WorldCoordinateToGrid(WorldLocation);
// 	FVector ApplyGridVector = GridToWorldCoordinate(VecToGrid);
// 	ApplyGridVector.Z += 5.f; // look well
// 	
// 	FMatrix Mat = FMatrix::Identity;
// 	Mat.SetOrigin(ApplyGridVector);
// 	
// 	DrawDebugCircle(
// 		GetWorld(), ApplyGridVector, Radius, 24, FColor::Red,
// 		false, 5.f, 0, 0.f,
// 		FVector(1, 0, 0),   // YAxis = 월드 X
// 		FVector(0, 1, 0),   // ZAxis = 월드 Y
// 		true
// 	);
// 	
// 	// 유효한 포인트만 표시해보기
// 	
// 	//float Normalize = Radius / CellSize;
// 	const float HalfCell = CellSize * 0.5f;
// 	const float RadiusSq = Radius * Radius;   // 반지름 미리 제곱
//
// 	for (float Y = -Radius; Y <= Radius; Y += CellSize)
// 	{
// 		for (float X = -Radius; X <= Radius; X += CellSize)
// 		{
// 			const FVector CellCenter(
// 				ApplyGridVector.X + X,
// 				ApplyGridVector.Y + Y,
// 				ApplyGridVector.Z);
//
// 			// point = dist currentpoint, pivotpoint
// 			const FVector2D CenterOffset(X, Y);
// 			if (CenterOffset.SizeSquared() <= RadiusSq)
// 			{
// 				DrawDebugPoint(GetWorld(), CellCenter, 10.f, FColor::Cyan, false, 5.f);
// 			}
//
// 			// square = max distance point
// 			const FVector2D FarCorner(
// 				FMath::Abs(X) + HalfCell,
// 				FMath::Abs(Y) + HalfCell);
// 			if (FarCorner.SizeSquared() <= RadiusSq)
// 			{
// 				DrawDebugBox(
// 					GetWorld(),
// 					CellCenter,
// 					FVector(HalfCell, HalfCell, 5.f),
// 					FColor::Green,
// 					false, 5.f, 0, 4.f);
// 			}
// 		}
// 	}
// }

// void USFBuildingGridSubsystem::DebugDrawGrid(const FVector& WorldLocation, const FIntPoint& InGridSize)
// {
// 	const FIntPoint VecToGrid = WorldCoordinateToGrid(WorldLocation);
// 	FVector ApplyGridVector = GridToWorldCoordinate(VecToGrid);
// 	ApplyGridVector.Z = WorldLocation.Z + 5.f; // look well
// 	
// 	const float HalfCell = CellSize * 0.5f;
//
// 	for (int Y = 0; Y < InGridSize.Y; Y++)
// 	{
// 		for (int X = 0; X < InGridSize.X; X++)
// 		{
// 			FVector TempGridCenter(ApplyGridVector.X + X * CellSize, ApplyGridVector.Y + Y * CellSize, ApplyGridVector.Z);
// 			
// 			DrawDebugBox(
// 			GetWorld(),
// 			TempGridCenter,
// 			FVector(HalfCell, HalfCell, 5.f),
// 			FColor::Green,
// 			false, 5.f, 0, 4.f);
// 		}
// 	}
// }

void USFBuildingGridSubsystem::DebugDrawOccupiedCells(float Duration) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

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
			false,
			Duration,
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
				false,
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