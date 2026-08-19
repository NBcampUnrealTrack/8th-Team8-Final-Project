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
	// = endplacementmode == tick off cachedproperty nullptr
	
	void EndPlacementSession();
private:
	void UpdatePlacementMode();
	
	bool CanPlaceOnCurrentGrid() const;
	// nav
	bool CanPlaceOnNavMeshXY(const FVector& WorldLocation) const;

	UPROPERTY()
	TWeakObjectPtr<ASFBaseBuilding> TargetBuilding;
	UPROPERTY()
	FBuildingGridInfo TargetBuildingGridInfo;
	UPROPERTY()
	bool bCanPlaceCurrentLocation = false;
	
	UPROPERTY()
	TWeakObjectPtr<APlayerController> OwnerController;
	UPROPERTY() // should not be deleted by GC
	TObjectPtr<USFBuildingGridSubsystem> CachedGridSubsystem;
};
