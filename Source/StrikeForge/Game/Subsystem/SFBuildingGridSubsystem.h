#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Building/MiningData/SFMiningCellTarget.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "SFBuildingGridSubsystem.generated.h"

class AResourceBase;

/**
 * 건물 격자 검사 결과입니다.
 * 함수 반환용이며 Subsystem이나 BuildingPlacement에 캐싱하지 않습니다.
 */
struct FSFBuildingGridEvaluationResult
{
	bool bCanPlaceOnGrid = false;
	int32 MineableResourceAmount = 0;
};

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

	// 배열 순회하면서 자원이 유효한지 검사
	bool HasValidResource() const
	{
		return Resources.ContainsByPredicate(
			[](const FSFGridResourceEntry& Entry)
			{
				return Entry.ResourceActor.IsValid();
			});
	}

	// 격자점유가 비었는지 확인 (자원과 건물체크)
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
	
	UFUNCTION(BlueprintCallable)
	FVector GridToWorldCoordinate(const FIntPoint& GridCoord) const;
	
	// 현재 건물 격자와 광산 자원 조건을 함께 검사합니다.
	// 초기에 검사용도
	FSFBuildingGridEvaluationResult EvaluatePlayerBuildingGrid(
		const AActor* Building,	const FBuildingGridInfo& GridInfo) const;
	
	// 건물 점유를 검사한 뒤 성공한 경우에만 등록합니다.
	// 등록직전에 재검사용도
	bool TryRegisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);
	void UnregisterBuildingGrid(AActor* Owner, const FBuildingGridInfo& GridInfo);

	// 자원 점유를 검사한 뒤 매장량이 있는 셀만 등록합니다.
	bool TryRegisterResourceGrid(AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	void UnregisterResourceGrid(const AResourceBase* Resource, const FBuildingGridInfo& GridInfo);
	void UnregisterResourceCell(
		const AResourceBase* Resource,
		const FBuildingGridInfo& GridInfo,
		int32 ResourceCellIndex);
	
	/**
	 * 로컬 GridSize를 건물의 월드 회전에 맞는 점유 크기로 변환합니다.
	 */
	static FIntPoint GetRotatedGridSize(
		const FIntPoint& BaseGridSize, const FRotator& WorldRotation);
	/** 바깥 격자 안에 작은 격자를 셀 단위로 중앙 정렬할 수 있는지 검사합니다. */
	static bool IsCenteredSubGridSizeValid(
		const FIntPoint& OuterGridSize,
		const FIntPoint& InnerGridSize);
	FVector GridInfoToWorldCenter(const FBuildingGridInfo& GridInfo) const;
	// world location -> grid
	FBuildingGridInfo WorldCenterToGridInfo(const FVector& WorldCenter, const FIntPoint& GridSize) const;
	/**
	 * 월드 중심을 격자에 맞추고, 같은 계산에서 나온 격자 정보와 XY 스냅 위치를 반환합니다.
	 * Z는 입력 위치를 그대로 유지하며 Actor를 직접 이동시키지 않습니다.
	 */
	bool TrySnapWorldCenterToGrid(
		const FVector& WorldCenter,
		FIntPoint GridSize,
		FBuildingGridInfo& OutGridInfo,
		FVector& OutSnappedWorldCenter) const;
#if WITH_EDITOR
	/** 에디터 월드의 모든 건물·자원 격자 프리뷰를 갱신합니다. */
	void RefreshAllEditorGridPreviews();
#endif
	/**
	 * 바깥 격자 안에 작은 격자를 중앙 정렬합니다.
	 * 두 격자의 크기 차이가 홀수이면 중앙 정렬할 수 없습니다.
	 */
	bool TryMakeCenteredSubGrid(
		const FBuildingGridInfo& OuterGridInfo,
		const FIntPoint& InnerGridSize,
		FBuildingGridInfo& OutInnerGridInfo) const;

	/**
	 * 배치 모드 격자 표시에 사용할 셀을 수집합니다.
	 * 건물 셀과 강조 대상이 아닌 자원 셀은 막힌 셀, 강조 자원 종류의 매장량이 남은 셀은 자원 셀입니다.
	 */
	void CollectPlacementDisplayCells(
		bool bHighlightResource,
		EResourceType HighlightResourceType,
		TArray<FIntPoint>& OutBlockedCells,
		TArray<FIntPoint>& OutResourceCells,
		FFloatInterval& OutHeightRange) const;

	FORCEINLINE float GetCellSize() const { return CellSize; }
	FORCEINLINE const FVector& GetGridOrigin() const { return GridOrigin; }

private: // functions

	// 광산건물시도시 자원 수집
	int32 CollectMiningCellTargets(
		const AActor* MiningBuilding,
		const FBuildingGridInfo& BuildingGridInfo,
		TArray<FSFMiningCellTarget>& OutTargets) const;

	// 다른 건물과 겹치는지 검사합니다.
	bool CanOccupyBuildingCells(
		const AActor* Owner,
		const FBuildingGridInfo& GridInfo) const;

	// 실제 건물 등록에 사용하는 최종 점유 규칙입니다.
	bool CanRegisterBuildingGrid(
		const AActor* Owner,
		const FBuildingGridInfo& GridInfo) const;

	// 실제 자원 등록에 사용하는 최종 점유 규칙입니다.
	bool CanRegisterResourceGrid(
		const AResourceBase* Resource,
		const FBuildingGridInfo& GridInfo) const;

private: // properties
	
	float CellSize = 100.0f;
	FVector GridOrigin = FVector::ZeroVector;

	TMap<FIntPoint, FSFGridCellData> GridCells;
};
