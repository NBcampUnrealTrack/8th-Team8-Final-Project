#include "SFBuildingGridSubsystem.h"

void USFBuildingGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp,	Log, TEXT("BuildingGridSubsystem Initialized. CellSize: %.1f"),	CellSize);
}

void USFBuildingGridSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("BuildingGridSubsystem Deinitialized."));

	Super::Deinitialize();
}

FIntPoint USFBuildingGridSubsystem::WorldCoordinateToGrid(const FVector& WorldLocation) const
{
	// startpoint maybe 0,0,0 or maybe 100, 50, 0
	// so -gridorigin == apply pivot point
	const FVector LocalLocation = WorldLocation - GridOrigin;

	const int32 GridX =
		FMath::FloorToInt(LocalLocation.X / CellSize);

	const int32 GridY =
		FMath::FloorToInt(LocalLocation.Y / CellSize);

	return FIntPoint(GridX, GridY);
}

FVector USFBuildingGridSubsystem::GridToWorldCoordinate(const FIntPoint& GridCoord) const
{
	return GridOrigin + FVector(
	GridCoord.X * CellSize + CellSize * 0.5f,
	GridCoord.Y * CellSize + CellSize * 0.5f,
	GridOrigin.Z);
}

bool USFBuildingGridSubsystem::CanPlaceBuildingGrid(const FIntPoint& OriginCoordinate, const FIntPoint& GridSize)
{
	
	for (int Y = 0; Y < GridSize.Y; Y++)
	{
		for (int X = 0; X < GridSize.X; X++)
		{
			const FIntPoint TempCoordinate = FIntPoint(OriginCoordinate.X + X, OriginCoordinate.Y + Y);
			if (PlacedCells.Contains(TempCoordinate))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool USFBuildingGridSubsystem::TryRegistBuildingGrid(const FIntPoint& OriginCoordinate, const FIntPoint& GridSize)
{
	if (!CanPlaceBuildingGrid(OriginCoordinate, GridSize))
	{
		return false;
	}

	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			const FIntPoint CellCoordinate =
				OriginCoordinate + FIntPoint(X, Y);

			PlacedCells.Add(CellCoordinate);
		}
	}

	return true;
}

void USFBuildingGridSubsystem::DebugDrawRadius(const FVector& WorldLocation, float Radius)
{
	const FIntPoint VecToGrid = WorldCoordinateToGrid(WorldLocation);
	FVector ApplyGridVector = GridToWorldCoordinate(VecToGrid);
	ApplyGridVector.Z += 5.f; // look well
	
	FMatrix Mat = FMatrix::Identity;
	Mat.SetOrigin(ApplyGridVector);
	
	DrawDebugCircle(
		GetWorld(), ApplyGridVector, Radius, 24, FColor::Red,
		false, 5.f, 0, 0.f,
		FVector(1, 0, 0),   // YAxis = 월드 X
		FVector(0, 1, 0),   // ZAxis = 월드 Y
		true
	);
	
	// 유효한 포인트만 표시해보기
	
	//float Normalize = Radius / CellSize;
	const float HalfCell = CellSize * 0.5f;
	const float RadiusSq = Radius * Radius;   // 반지름 미리 제곱

	for (float Y = -Radius; Y <= Radius; Y += CellSize)
	{
		for (float X = -Radius; X <= Radius; X += CellSize)
		{
			const FVector CellCenter(
				ApplyGridVector.X + X,
				ApplyGridVector.Y + Y,
				ApplyGridVector.Z);

			// point = dist currentpoint, pivotpoint
			const FVector2D CenterOffset(X, Y);
			if (CenterOffset.SizeSquared() <= RadiusSq)
			{
				DrawDebugPoint(GetWorld(), CellCenter, 10.f, FColor::Cyan, false, 5.f);
			}

			// square = max distance point
			const FVector2D FarCorner(
				FMath::Abs(X) + HalfCell,
				FMath::Abs(Y) + HalfCell);
			if (FarCorner.SizeSquared() <= RadiusSq)
			{
				DrawDebugBox(
					GetWorld(),
					CellCenter,
					FVector(HalfCell, HalfCell, 5.f),
					FColor::Green,
					false, 5.f, 0, 4.f);
			}
		}
	}
}

void USFBuildingGridSubsystem::DebugDrawGrid(const FVector& WorldLocation, const FIntPoint& InGridSize)
{
	const FIntPoint VecToGrid = WorldCoordinateToGrid(WorldLocation);
	FVector ApplyGridVector = GridToWorldCoordinate(VecToGrid);
	ApplyGridVector.Z += WorldLocation.Z + 5.f; // look well
	
	const float HalfCell = CellSize * 0.5f;

	for (int Y = 0; Y < InGridSize.Y; Y++)
	{
		for (int X = 0; X < InGridSize.X; X++)
		{
			FVector TempGridCenter(ApplyGridVector.X + X * CellSize, ApplyGridVector.Y + Y * CellSize, ApplyGridVector.Z);
			
			DrawDebugBox(
			GetWorld(),
			TempGridCenter,
			FVector(HalfCell, HalfCell, 5.f),
			FColor::Green,
			false, 5.f, 0, 4.f);
		}
	}
}
