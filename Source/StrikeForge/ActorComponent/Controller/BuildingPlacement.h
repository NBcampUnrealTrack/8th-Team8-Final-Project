#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "BuildingPlacement.generated.h"

class ASFBaseBuilding;
class USFBuildingGridSubsystem;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UBuildingPlacement : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingPlacement();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
						   FActorComponentTickFunction* ThisTickFunction) override;
	UFUNCTION(BlueprintCallable)
	bool StartPlacementMode(ASFBaseBuilding* InTargetBuilding);
	
	bool TryConfirmPlacementMode();
	void EndPlacementSession();
private:
	void UpdatePlacementMode();
	void SetCurrentPlacementInvalid();
	
	bool CanPlaceOnCurrentGrid() const;
	// 격자, NavMesh, 튜토리얼 제한을 모두 포함한 최종 배치 판정입니다.
	bool CanPlaceAtCurrentLocation() const;
	// 건물 격자의 XY 범위가 NavMesh 경계 안에 있는지 검사합니다.
	bool CanPlaceOnNavMeshXY(const FBuildingGridInfo& GridInfo) const;

	// 현재 배치 중인 건물입니다.
	UPROPERTY(Transient)
	TWeakObjectPtr<ASFBaseBuilding> TargetBuilding;
	// 현재 미리보기 건물이 차지하는 격자 정보입니다.
	UPROPERTY(Transient)
	FBuildingGridInfo TargetBuildingGridInfo;
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
