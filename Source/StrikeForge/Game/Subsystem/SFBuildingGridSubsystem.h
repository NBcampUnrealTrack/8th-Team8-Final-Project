#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "SFBuildingGridSubsystem.generated.h"



UCLASS()
class STRIKEFORGE_API USFBuildingGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// // WorldCenterToGridInfo로 전환됨. 현재는 사용x
	// FIntPoint WorldCoordinateToGrid(const FVector& WorldLocation) const;
	UFUNCTION(BlueprintCallable)
	FVector GridToWorldCoordinate(const FIntPoint& GridCoord) const;
	
	//check TMap하고 등록되있는지 판단만
	bool CanPlaceBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const;
	// 점유o이면 x, 점유x면 regist까지 진행
	bool TryRegistBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	void UnregisterBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo);
	
	FVector GridInfoToWorldCenter(const FBuildingGridInfo& GridInfo) const;
	// world location -> grid
	FBuildingGridInfo WorldCenterToGridInfo(const FVector& WorldCenter, const FIntPoint& GridSize) const;
	// upgrade?
	//bool TryResizeBuildingGrid(AActor* Owner, const FBuildingGridInfo& OldFootprint, const FBuildingGridInfo& NewFootprint);
	FORCEINLINE float GetCellSize() const { return CellSize; }
	FORCEINLINE const FVector& GetGridOrigin() const { return GridOrigin; }
#pragma region Debug
	// UFUNCTION(BlueprintCallable)
	// void DebugDrawRadius(const FVector& WorldLocation, float Radius);
	// UFUNCTION(BlueprintCallable) 
	// void DebugDrawGrid(const FVector& WorldLocation, const FIntPoint& InGridSize);
	UFUNCTION(BlueprintCallable, Category = "SF|Grid|Debug")
	void DebugDrawOccupiedCells(float Duration = 5.0f) const;
	UFUNCTION(BlueprintCallable, Category = "SF|Grid|Debug")
	void DebugDrawGridInfo(const FBuildingGridInfo& GridInfo, float WorldZ,	bool bCanPlace,	float Duration = 0.0f) const;
#pragma endregion 
private:
	// size of grid
	float CellSize = 100.f;
	// Grid pivot
	FVector GridOrigin = FVector::ZeroVector;
	
	// array of placed grid // (1, 1)
	// (1, 1) = 1(X) * cellsize + cellsize / 2
	TMap<FIntPoint, TWeakObjectPtr<AActor>> OccupiedCells; 
};
