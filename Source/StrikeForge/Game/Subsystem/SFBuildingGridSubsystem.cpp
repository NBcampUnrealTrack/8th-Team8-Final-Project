#include "SFBuildingGridSubsystem.h"
#include "Buildings/Resource/ResourceBase.h"
#include "GameFramework/Actor.h"
#include "Buildings/Resource/Interface/SFGridResourceConsumer.h"

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

// FIntPoint USFBuildingGridSubsystem::WorldCoordinateToGrid(const FVector& WorldLocation) const
// {
// 	// startpoint maybe 0,0,0 or maybe 100, 50, 0
// 	// so -gridorigin == apply pivot point
// 	const FVector LocalLocation = WorldLocation - GridOrigin;
//
// 	const int32 GridX =
// 		FMath::FloorToInt(LocalLocation.X / CellSize);
//
// 	const int32 GridY =
// 		FMath::FloorToInt(LocalLocation.Y / CellSize);
//
// 	return FIntPoint(GridX, GridY);
// }

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

	const ISFGridResourceConsumer* Consumer =
		Cast<ISFGridResourceConsumer>(Building);

	// 일반 건물은 CanRegisterBuildingGrid 결과만 따르면 된다.
	if (!Consumer)
	{
		return true;
	}

	// 플레이어가 배치하는 자원 소비 건물은
	// 반드시 현재 전체 격자에 자원이 존재해야 한다.
	return IsValid(FindResourceInGrid(Building, GridInfo));
}

bool USFBuildingGridSubsystem::TryRegisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo)
{
	// ! > 누군가 점유중
	if (!CanRegisterBuildingGrid(Owner, GridInfo))
	{
		return false;
	}
	// 점유 등록
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate = GridInfo.OriginCell + FIntPoint(X, Y);
			
			// 찾거나 추가
			FSFGridCellData& Cell = GridCells.FindOrAdd(CellCoordinate);
			Cell.BuildingActor = Owner;
		}
	}

	AResourceBase* Resource = FindResourceInGrid(Owner, GridInfo);

	if (IsValid(Resource) && !TryResolveResourceConnection(Owner, Resource))
	{
		// 점유는 등록됐지만 연결이 실패했으므로 원상 복구
		UnregisterBuildingGrid(Owner, GridInfo);
		return false;
	}
	
	return true;
}

void USFBuildingGridSubsystem::UnregisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo)
{
	if (!GridInfo.IsValid()) return;
	// 자원격자 해제
	AResourceBase* Resource = FindResourceInGrid(Owner, GridInfo);

	if (IsValid(Resource))
	{
		DisconnectResourceConnection(
			Owner,
			Resource);
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
	if (!CanRegisterResourceGrid(Resource, GridInfo))
	{
		return false;
	}
	
	// 실제 등록만 수행
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			FSFGridCellData& Cell =
				GridCells.FindOrAdd(CellCoordinate);

			Cell.ResourceActor = Resource;
		}
	}

	AActor* Building = FindBuildingInGrid(GridInfo);

	if (IsValid(Building) && !TryResolveResourceConnection(Building, Resource))
	{
		UnregisterResourceGrid(Resource, GridInfo);
		return false;
	}
	
	return true;
}

void USFBuildingGridSubsystem::UnregisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo)
{
	if (!GridInfo.IsValid()) return;
	
	// 건물 격자 해제
	AActor* Building = FindBuildingInGrid(GridInfo);

	if (IsValid(Building))
	{
		DisconnectResourceConnection(
			Building,
			Resource);
	}
	
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate = GridInfo.OriginCell + FIntPoint(X, Y);
			FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;
			// ResourceActor가 등록안돼있거나 등록된게 나자신이면
			if (!Cell->ResourceActor.IsValid() || Cell->ResourceActor.Get() == Resource)
			{	// delete value
				Cell->ResourceActor.Reset();
			}
			// 빌딩도 없고 자원도 없다면
			if (Cell->IsEmpty())
			{	// map요소 삭제
				GridCells.Remove(CellCoordinate);
			}
		}
	}
}

AResourceBase* USFBuildingGridSubsystem::FindResourceInGrid(const AActor* InBuilding,
	const FBuildingGridInfo& GridInfo) const
{
	AResourceBase* FoundResource = nullptr;
	
	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate = GridInfo.OriginCell + FIntPoint(X, Y);
			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) return nullptr; // 하나라도 없으면 끝
			
			AResourceBase* CellResource = Cell->ResourceActor.Get();
			if (!IsValid(CellResource)) return nullptr;
			
			if (!FoundResource) // nullptr일때만 값을 set
			{
				FoundResource = CellResource;
			}
			else if (FoundResource != CellResource) // 
			{
				return nullptr;
			}
		}
	}
		
	return FoundResource;
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

bool USFBuildingGridSubsystem::CanRegisterBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const
{
	// 건물 겹침검사
	if (!CanPlaceBuildingGrid(Owner, GridInfo))
	{
		return false;
	}
	
	// 겹치지않았으면 실행
	AResourceBase* FoundResource = nullptr;
	int32 ResourceCellCount = 0;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;
			
			AResourceBase* CellResource = Cell->ResourceActor.Get();
			if (!IsValid(CellResource))	continue;
			
			++ResourceCellCount;

			if (!FoundResource)
			{
				FoundResource = CellResource;
			}
			else if (FoundResource != CellResource)
			{
				return false;
			}
		}
	}

	// 자원이 없다면 일반 건물과 광산 모두 등록 가능하다.
	// 플레이어 광산 배치는 기존 BuildingPlacement에서 자원을 요구한다.
	if (ResourceCellCount == 0)
	{
		return true;
	}

	const int32 TotalCellCount = GridInfo.GridSize.X * GridInfo.GridSize.Y;

	// 일부 셀만 자원과 겹치는 배치는 허용하지 않는다.
	if (ResourceCellCount != TotalCellCount)
	{
		return false;
	}

	return CanShareResourceGrid(Owner, FoundResource);
}

bool USFBuildingGridSubsystem::CanRegisterResourceGrid(const AResourceBase* Resource,
	const FBuildingGridInfo& GridInfo) const
{
	if (!IsValid(Resource) || !GridInfo.IsValid())
	{
		return false;
	}

	AActor* FoundBuilding = nullptr;
	int32 BuildingCellCount = 0;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell = GridCells.Find(CellCoordinate);
			if (!Cell) continue;

			AResourceBase* ExistingResource = Cell->ResourceActor.Get();

			if (IsValid(ExistingResource) && ExistingResource != Resource)
			{
				return false;
			}

			AActor* CellBuilding = Cell->BuildingActor.Get();
			if (!IsValid(CellBuilding))	continue;

			++BuildingCellCount;

			if (!FoundBuilding)
			{
				FoundBuilding = CellBuilding;
			}
			else if (FoundBuilding != CellBuilding)
			{
				return false;
			}
		}
	}

	if (BuildingCellCount == 0)
	{
		return true;
	}

	const int32 TotalCellCount = GridInfo.GridSize.X * GridInfo.GridSize.Y;

	if (BuildingCellCount != TotalCellCount)
	{
		return false;
	}

	return CanShareResourceGrid(FoundBuilding, Resource);
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
	if (!IsValid(World))
	{
		return;
	}

	const FVector BoxExtent(
		CellSize * 0.5f,
		CellSize * 0.5f,
		10.0f);

	for (const TPair<
			FIntPoint,
			FSFGridCellData>& GridCell
		: GridCells)
	{
		AActor* OccupiedActor = (GridCell.Value.BuildingActor.IsValid()) ?
					GridCell.Value.BuildingActor.Get() : GridCell.Value.ResourceActor.Get();

		// 이미 Destroy된 액터의 오래된 정보는 표시하지 않습니다.
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

void USFBuildingGridSubsystem::DebugDrawResourceCells(EResourceType ResourceType, float Duration) const
{
	const FVector BoxExtent(CellSize * 0.5f, CellSize * 0.5f,	10.0f);
	
	for (const TPair<FIntPoint,	FSFGridCellData>& GridCell: GridCells)
	{
		AResourceBase* OccupiedResource = GridCell.Value.ResourceActor.Get();
		if (!IsValid(OccupiedResource)) continue;
		
		if (OccupiedResource->GetResourceType() != ResourceType) continue;
		// 자원발견 + 같은 타입
		FVector CellCenter = GridToWorldCoordinate(GridCell.Key);
		// 점유 중인 건물의 높이를 기준으로 표시
		CellCenter.Z = OccupiedResource->GetActorLocation().Z + 10.0f;
		
		DrawDebugBox(
		GetWorld(),
		CellCenter,
		BoxExtent,
		FColor::Green,
		false,
		Duration,
		0,
		3.0f);
	}
}

bool USFBuildingGridSubsystem::CanShareResourceGrid(const AActor* Building, const AResourceBase* Resource) const
{
	if (!IsValid(Building) || !IsValid(Resource))
	{
		return false;
	}

	const ISFGridResourceConsumer* Consumer =
		Cast<ISFGridResourceConsumer>(Building);

	return Consumer &&
		Consumer->CanAcceptResource(Resource);
}

AActor* USFBuildingGridSubsystem::FindBuildingInGrid(const FBuildingGridInfo& GridInfo) const
{
	if (!GridInfo.IsValid())
	{
		return nullptr;
	}

	AActor* FoundBuilding = nullptr;

	for (int32 Y = 0; Y < GridInfo.GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridInfo.GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				GridInfo.OriginCell + FIntPoint(X, Y);

			const FSFGridCellData* Cell =
				GridCells.Find(CellCoordinate);

			if (!Cell)
			{
				return nullptr;
			}

			AActor* CellBuilding =
				Cell->BuildingActor.Get();

			if (!IsValid(CellBuilding))
			{
				return nullptr;
			}

			if (!FoundBuilding)
			{
				FoundBuilding = CellBuilding;
			}
			else if (FoundBuilding != CellBuilding)
			{
				return nullptr;
			}
		}
	}

	return FoundBuilding;
}

bool USFBuildingGridSubsystem::TryResolveResourceConnection(AActor* Building, AResourceBase* Resource)
{
	if (!IsValid(Building) || !IsValid(Resource))
	{
		return false;
	}

	ISFGridResourceConsumer* Consumer =
		Cast<ISFGridResourceConsumer>(Building);

	if (!Consumer ||
		!Consumer->CanAcceptResource(Resource))
	{
		return false;
	}

	return Consumer->TryLinkResource(Resource);
}

void USFBuildingGridSubsystem::DisconnectResourceConnection(AActor* Building, const AResourceBase* Resource)
{
	if (!IsValid(Building) || !IsValid(Resource))
	{
		return;
	}

	ISFGridResourceConsumer* Consumer =
		Cast<ISFGridResourceConsumer>(Building);

	if (Consumer)
	{
		Consumer->UnlinkResource(Resource);
	}
}
