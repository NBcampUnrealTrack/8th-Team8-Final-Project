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
	// 실패처리한곳으로
	void SetCurrentPlacementInvalid();
	
	bool CanPlaceOnCurrentGrid() const;
	// 공통판정함수
	bool CanPlaceAtCurrentLocation() const;
	// nav
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
