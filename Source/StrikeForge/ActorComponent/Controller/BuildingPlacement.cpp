#include "BuildingPlacement.h"

UBuildingPlacement::UBuildingPlacement()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
}

void UBuildingPlacement::StartPlacementMode(AActor* InTargetActor)
{
	TargetActor = InTargetActor;
	OwnerController = Cast<APlayerController>(GetOwner());
	UpdatePlacementMode();
	SetComponentTickEnabled(true);
}

void UBuildingPlacement::UpdatePlacementMode()
{
	if (!TargetActor.IsValid())
	{
		ConfrimPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:TargetActor is Invalid"));
		return;
	}

	if (!OwnerController.IsValid())
	{
		ConfrimPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:Controller is Invalid"));
		return;
	}

	FHitResult HitResult;

	if (OwnerController.Get()->GetHitResultUnderCursor(
			ECC_LANDSCAPE,
			false,
			HitResult))
	{
		TargetActor.Get()->SetActorLocation(HitResult.Location);
	}
}

void UBuildingPlacement::ConfrimPlacementMode()
{
	SetComponentTickEnabled(false);
	TargetActor = nullptr;
}

void UBuildingPlacement::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	UpdatePlacementMode();
}

