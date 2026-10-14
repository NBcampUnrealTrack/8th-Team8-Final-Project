#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Game/GameState/SFGameState.h"
#include "Math/Interval.h"
#include "BuildingPlacement.generated.h"

class ASFBaseBuilding;
class ASFGridDecalVisualizer;
class UMaterialInterface;
class USFBuildingGridSubsystem;
class USFBuildingSynergyDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnMineableResourceAmountChanged,
	int32, MineableResourceAmount);

/** 배치 모드 여부와, 배치 중인 건물이 지금 위치에서 받을 시너지 생산 증가율(%)을 알립니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnPlacementSynergyBonusChanged,
	bool, bPlacementActive, float, BonusPercent);

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

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	bool IsPlacementModeActive() const { return TargetBuilding.IsValid() && IsComponentTickEnabled(); }

	/** 배치 중인 건물이 마지막으로 평가한 격자에서 받을 시너지 생산 증가율(%)입니다. */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	float GetPlacementSynergyBonusPercent() const { return CachedPlacementSynergyBonusPercent; }

	/** 선택한 건물이 속한 활성 시너지 클러스터를 완성시너지색(노랑)으로 범위를 한 번 보여 줍니다. */
	void FlashSynergyClustersOfBuilding(const ASFBaseBuilding* Building) const;
public: // properties
	UPROPERTY(BlueprintAssignable, Category = "Building|Placement")
	FSFOnMineableResourceAmountChanged OnMineableResourceAmountChanged;

	/** 배치 모드가 시작·종료되거나 지금 위치의 시너지 보너스가 바뀌면 알립니다. 커서 옆 보너스 위젯이 구독합니다. */
	UPROPERTY(BlueprintAssignable, Category = "Building|Placement")
	FSFOnPlacementSynergyBonusChanged OnPlacementSynergyBonusChanged;
private:
	void UpdatePlacementMode();
	void SetCurrentPlacementInvalid();
	ASFGridDecalVisualizer* GetOrCreateGridVisualizer();
	void ResetPlacementEvaluation();

	// 점유·자원 칸과 시너지 안내(합류 범위·받을 수 있는 효과 구역)를 현재 월드 상태로 다시 그립니다.
	void RefreshPlacementDisplays();

	// 마지막 평가 격자에서 받을 시너지 보너스를 다시 계산하고, 바뀌었거나 아직 알리지 않았으면 알립니다.
	void UpdatePlacementSynergyBonus();

	// 생산 속도 인터페이스를 구현하고, 팀이 지을 수 있는 건물의 시너지 중 이 건물 종류를 버프 대상으로 삼는 것이 있는지 확인합니다.
	bool CanReceiveSynergyBonus(const ASFBaseBuilding* Building) const;

	// 배치 중에 다른 건물이 완성·파괴되어 시너지가 바뀌면 격자 표시와 보너스를 다시 계산합니다.
	UFUNCTION()
	void HandleSynergyStateChanged();

	// 스테이지 로딩이 끝나면 격자 데칼 예열을 멈춥니다.
	UFUNCTION()
	void HandleStageStateChanged(ESFStageState NewStageState);
	void StopGridDecalWarmUp();

	bool CanPlaceAtCurrentLocation(bool bCanPlaceOnGrid) const;

	bool CanPlaceOnNavMeshXY(const FBuildingGridInfo& GridInfo) const;

	// 배치 중인 건물 기준 시너지 안내를 표시합니다.
	// 합류 범위(하늘색)와 받을 수 있는 효과 구역(시너지 색)을 함께 그리며, 두 표시 모두 점유 칸은 제외합니다.
	void ShowPlacementSynergyHints(
		ASFGridDecalVisualizer* Visualizer,
		const ASFBaseBuilding* Building,
		const TArray<FIntPoint>& BlockedCells,
		const FFloatInterval& HeightRange) const;

	// 배치 중인 건물이 주는 시너지와 같은 시너지를 주는 완성 건물들의 링크 범위 칸을 모읍니다.
	void CollectExistingLinkRangeCells(
		const ASFBaseBuilding* Building,
		const TSet<FIntPoint>& BlockedCellSet,
		TArray<FIntPoint>& OutLinkCells) const;

	// 배치 중인 건물 종류가 버프 대상인 내 팀 시너지 효과 구역 칸을 시너지별로 모읍니다.
	// 구역 원천 건물의 높이를 InOutHeightRange에 더합니다.
	void CollectReceivableEffectCells(
		const ASFBaseBuilding* Building,
		const TSet<FIntPoint>& BlockedCellSet,
		TMap<const USFBuildingSynergyDefinition*, TArray<FIntPoint>>& OutCellsBySynergy,
		FFloatInterval& InOutHeightRange) const;
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

	// 마지막으로 알린 시너지 보너스와, 이번 배치 세션에서 보너스 표시를 알렸는지 여부입니다.
	float CachedPlacementSynergyBonusPercent = 0.f;
	bool bPlacementSynergyBonusShown = false;

	// 배치 중인 건물이 시너지 생산 버프를 받을 수 있는 건물인지 여부입니다. false면 보너스 위젯을 띄우지 않습니다.
	bool bCanReceivePlacementSynergyBonus = false;
	
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
