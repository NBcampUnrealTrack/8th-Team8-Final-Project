#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Building/MiningData/SFMiningCellTarget.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "SFBuildingGridSubsystem.generated.h"

class AResourceBase;

// Grid 한 칸에 등록되는 자원 하나의 정보입니다.
// Blueprint에 노출하지 않는 런타임 전용 구조체입니다.
struct FSFGridResourceEntry
{
	TWeakObjectPtr<AResourceBase> ResourceActor;

	EResourceType ResourceType = EResourceType::Mineral;

	int32 ResourceCellIndex = INDEX_NONE;
};

struct FSFGridCellData
{
	// 같은 셀에 서로 다른 종류의 자원은 존재할 수 있습니다.
	// 같은 ResourceType은 하나만 등록할 수 있습니다.
	TArray<FSFGridResourceEntry> Resources;

	// 건물은 같은 셀에 하나만 존재합니다.
	TWeakObjectPtr<AActor> BuildingActor;

	// 배열 순회하면서 value가 isvalid한지 검사
	bool HasValidResource() const
	{
		return Resources.ContainsByPredicate(
			[](const FSFGridResourceEntry& Entry)
			{
				return Entry.ResourceActor.IsValid();
			});
	}

	// Celldata가 비었는지 확인
	bool IsEmpty() const
	{
		return !HasValidResource() &&
			!BuildingActor.IsValid();
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
	
	bool CollectMiningCellTargets(
	const AActor* MiningBuilding,
	const FBuildingGridInfo& BuildingGridInfo,
	TArray<FSFMiningCellTarget>& OutTargets) const;
	
	// building
	// 점유o이면 x, 점유x면 regist까지 진행
	bool TryRegisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	void UnregisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	// resource
	bool TryRegisterResourceGrid(AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	void UnregisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	void UnregisterResourceCell(const AResourceBase* Resource,
	const FBuildingGridInfo& GridInfo, int32 ResourceCellIndex);
	
	FVector GridInfoToWorldCenter(const FBuildingGridInfo& GridInfo) const;
	// world location -> grid
	FBuildingGridInfo WorldCenterToGridInfo(const FVector& WorldCenter, const FIntPoint& GridSize) const;
	/**
	 * OuterGridInfo 안에 InnerGridSize 크기의 격자를
	 * 정확히 중앙 정렬하여 반환합니다.
	 *
	 * 두 격자 크기의 차이가 홀수라면 정확한 중앙 정렬이
	 * 불가능하므로 false를 반환합니다.
	 */
	bool TryMakeCenteredSubGrid(
		const FBuildingGridInfo& OuterGridInfo,
		const FIntPoint& InnerGridSize,
		FBuildingGridInfo& OutInnerGridInfo) const;
	
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
	void DebugDrawMiningPlacementGrid(const FBuildingGridInfo& BuildingGridInfo,const FIntPoint& ResourceSearchGridSize,float WorldZ,float Duration = 0.0f) const;
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
	
private:
	// size of grid
	float CellSize = 100.f;
	// Grid pivot
	FVector GridOrigin = FVector::ZeroVector;
	
	// array of placed grid // (1, 1)
	// (1, 1) = 1(X) * cellsize + cellsize / 2
	TMap<FIntPoint, FSFGridCellData> GridCells; 
};
