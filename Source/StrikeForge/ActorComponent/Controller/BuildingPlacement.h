#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildingPlacement.generated.h"

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
	void StartPlacementMode(AActor* InTargetActor);
	UFUNCTION(BlueprintCallable)
	void UpdatePlacementMode();
	UFUNCTION(BlueprintCallable)
	void ConfirmPlacementMode();
	
	UFUNCTION(BlueprintCallable)
	void ToggleGridFunction(bool toggle);
private:
	
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;
	UPROPERTY()
	TWeakObjectPtr<APlayerController> OwnerController;
	UPROPERTY() // should not be deleted by GC
	TObjectPtr<USFBuildingGridSubsystem> CachedGridSubsystem;
	
	bool ToggleGrid = true;
};
