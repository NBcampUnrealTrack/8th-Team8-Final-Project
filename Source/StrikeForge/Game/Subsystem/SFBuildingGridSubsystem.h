#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFBuildingGridSubsystem.generated.h"

UCLASS()
class STRIKEFORGE_API USFBuildingGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable)
	FIntPoint WorldCoordinateToGrid(const FVector& WorldLocation) const;
	UFUNCTION(BlueprintCallable)
	FVector GridToWorldCoordinate(const FIntPoint& GridCoord) const;
	
	//check can build
	bool CanPlaceBuildingGrid(const FIntPoint& OriginCoordinate, const FIntPoint& GridSize);
	bool TryRegistBuildingGrid(const FIntPoint& OriginCoordinate, const FIntPoint& GridSize);
	
	float GetCellSize() const { return CellSize; }
	const FVector& GetGridOrigin() const { return GridOrigin; }
#pragma region Debug
	UFUNCTION(BlueprintCallable)
	void DebugDrawRadius(const FVector& WorldLocation, float Radius);
	UFUNCTION(BlueprintCallable)
	void DebugDrawGrid(const FVector& WorldLocation, const FIntPoint& InGridSize);
#pragma endregion 
private:
	// size of grid
	float CellSize = 100.f;
	// Grid pivot
	FVector GridOrigin = FVector::ZeroVector;
	
	// array of placed grid // (1, 1)
	// (1, 1) = 1(X) * cellsize + cellsize / 2
	TSet<FIntPoint> PlacedCells; 
};
