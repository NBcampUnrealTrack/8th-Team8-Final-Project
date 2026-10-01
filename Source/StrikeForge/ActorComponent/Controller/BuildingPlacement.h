#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "BuildingPlacement.generated.h"

class ASFBaseBuilding;
class USFBuildingGridSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnMineableResourceAmountChanged,
	int32, MineableResourceAmount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UBuildingPlacement : public UActorComponent
{
	GENERATED_BODY()

public: // functions
	UBuildingPlacement();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
						   FActorComponentTickFunction* ThisTickFunction) override;
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
public: // properties
	UPROPERTY(BlueprintAssignable, Category = "Building|Placement")
	FSFOnMineableResourceAmountChanged OnMineableResourceAmountChanged;
private:
	void UpdatePlacementMode();
	void SetCurrentPlacementInvalid();
	void DrawCurrentPlacementGrid() const;
	void ResetPlacementEvaluation();

	bool CanPlaceAtCurrentLocation(bool bCanPlaceOnGrid) const;

	bool CanPlaceOnNavMeshXY(const FBuildingGridInfo& GridInfo) const;
	// 현재 배치 중인 건물입니다.
	UPROPERTY(Transient)
	TWeakObjectPtr<ASFBaseBuilding> TargetBuilding;
	// 현재 미리보기 건물이 차지하는 격자 정보입니다.
	UPROPERTY(Transient)
	FBuildingGridInfo TargetBuildingGridInfo;
	
	// 건설 세션 동안 고정된 회전 적용 GridSize입니다.
	FIntPoint PlacementGridSize =
		FIntPoint(1, 1);

	// 광산의 회전 적용 자원 검색 크기입니다.
	FIntPoint CachedResourceSearchGridSize =
		FIntPoint::ZeroValue;

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
};
