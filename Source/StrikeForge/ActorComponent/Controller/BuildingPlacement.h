#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Game/GameState/SFGameState.h"
#include "BuildingPlacement.generated.h"

class ASFBaseBuilding;
class ASFGridDecalVisualizer;
class UMaterialInterface;
class USFBuildingGridSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnMineableResourceAmountChanged,
	int32, MineableResourceAmount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UBuildingPlacement : public UActorComponent
{
	GENERATED_BODY()

public: // functions
	UBuildingPlacement();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
						   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION(BlueprintCallable)
	bool StartPlacementMode(ASFBaseBuilding* InTargetBuilding);
	
	bool TryConfirmPlacementMode();
	void EndPlacementSession();

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	bool IsMiningPlacementActive() const { return bIsMiningPlacement && TargetBuilding.IsValid() && IsComponentTickEnabled(); }

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	int32 GetMineableResourceAmount() const { return CachedMineableResourceAmount; }

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	EResourceType GetPlacementResourceType() const;

	/** 선택한 건물이 속한 활성 시너지 클러스터를 시너지 완성 표시(노란색)로 한 번 보여 줍니다. */
	void FlashSynergyClustersOfBuilding(const ASFBaseBuilding* Building) const;
public: // properties
	UPROPERTY(BlueprintAssignable, Category = "Building|Placement")
	FSFOnMineableResourceAmountChanged OnMineableResourceAmountChanged;
private:
	void UpdatePlacementMode();
	void SetCurrentPlacementInvalid();
	ASFGridDecalVisualizer* GetOrCreateGridVisualizer();
	void ResetPlacementEvaluation();

	// 스테이지 로딩이 끝나면 격자 데칼 예열을 멈춥니다.
	UFUNCTION()
	void HandleStageStateChanged(ESFStageState NewStageState);
	void StopGridDecalWarmUp();

	bool CanPlaceAtCurrentLocation(bool bCanPlaceOnGrid) const;

	bool CanPlaceOnNavMeshXY(const FBuildingGridInfo& GridInfo) const;

	// 배치 중인 건물이 주는 시너지와 같은 시너지를 주는 완성 건물들의 링크 범위 칸을 모읍니다.
	// 점유 칸은 제외합니다.
	void CollectExistingLinkRangeCells(
		const ASFBaseBuilding* Building,
		const TArray<FIntPoint>& BlockedCells,
		TArray<FIntPoint>& OutLinkCells) const;
	// 현재 배치 중인 건물입니다.
	UPROPERTY(Transient)
	TWeakObjectPtr<ASFBaseBuilding> TargetBuilding;
	// 현재 미리보기 건물이 차지하는 격자 정보입니다.
	UPROPERTY(Transient)
	FBuildingGridInfo TargetBuildingGridInfo;
	
	// 건설 세션 동안 고정된 회전 적용 GridSize입니다.
	FIntPoint PlacementGridSize =
		FIntPoint(1, 1);

	// 마지막으로 배치 판정을 수행한 격자입니다.
	FBuildingGridInfo LastEvaluatedGridInfo;

	bool bHasEvaluatedGrid = false;
	bool bIsMiningPlacement = false;
	int32 CachedMineableResourceAmount = 0;
	
	// 가장 최근 미리보기 위치의 배치 가능 여부입니다.
	UPROPERTY(Transient)
	bool bCanPlaceCurrentLocation = false;
	
	// 현재 배치 입력을 처리하는 컨트롤러입니다.
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> OwnerController;
	// 현재 World에서 사용하는 격자 Subsystem 캐시입니다.
	UPROPERTY(Transient)
	TObjectPtr<USFBuildingGridSubsystem> CachedGridSubsystem;

	// 격자 칸 데칼 머티리얼입니다. BP_SFInGamePlayerController의 PlacementComponent에서 지정합니다.
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement|Grid")
	TObjectPtr<UMaterialInterface> GridCellDecalMaterial;

	// 배치 세션마다 재사용하는 격자 표시 액터입니다.
	UPROPERTY(Transient)
	TObjectPtr<ASFGridDecalVisualizer> GridVisualizer;

	// 격자 데칼 예열 종료 시점을 받기 위해 구독 중인 GameState입니다.
	UPROPERTY(Transient)
	TWeakObjectPtr<ASFGameState> ObservedGameState;
};
