#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "SFBuildingGridSubsystem.generated.h"

class AResourceBase;
// 블루프린트 노출 x
struct FSFGridCellData
{
	TWeakObjectPtr<AResourceBase> ResourceActor;
	TWeakObjectPtr<AActor> BuildingActor;
	
	bool IsEmpty() const
	{
		return !ResourceActor.IsValid() && !BuildingActor.IsValid();
	}
};

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
	
	// check
	bool CanPlacePlayerBuildingGrid(const AActor* Building, const FBuildingGridInfo& GridInfo) const;
	// building
	// 점유o이면 x, 점유x면 regist까지 진행
	bool TryRegisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	void UnregisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	// resource
	bool TryRegisterResourceGrid(AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	void UnregisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	
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
	void DebugDrawResourceCells(EResourceType ResourceType, float Duration = 5.0f) const;
#pragma endregion 
private:
	// 건물끼리 겹치는지 검사
	bool CanPlaceBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const;
	// 실제 건물 등록에 적용할 최종 규칙
	bool CanRegisterBuildingGrid(const AActor* Owner, const FBuildingGridInfo& GridInfo) const;
	// 실제 자원 등록에 적용할 최종 규칙
	bool CanRegisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo) const;
	// filter resource
	AResourceBase* FindResourceInGrid(const AActor* InBuilding, const FBuildingGridInfo& GridInfo) const;

	
	bool CanShareResourceGrid(
	const AActor* Building,
	const AResourceBase* Resource) const;
	
	AActor* FindBuildingInGrid(
	const FBuildingGridInfo& GridInfo) const;

	bool TryResolveResourceConnection(
		AActor* Building,
		AResourceBase* Resource);
	
	void DisconnectResourceConnection(
	AActor* Building,
	const AResourceBase* Resource);
private:
	// size of grid
	float CellSize = 100.f;
	// Grid pivot
	FVector GridOrigin = FVector::ZeroVector;
	
	// array of placed grid // (1, 1)
	// (1, 1) = 1(X) * cellsize + cellsize / 2
	TMap<FIntPoint, FSFGridCellData> GridCells; 
};
