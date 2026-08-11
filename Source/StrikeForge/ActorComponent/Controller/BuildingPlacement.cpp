#include "BuildingPlacement.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Kismet/GameplayStatics.h"

UBuildingPlacement::UBuildingPlacement()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
}

void UBuildingPlacement::StartPlacementMode(AActor* InTargetActor)
{
	TargetActor = InTargetActor;
	OwnerController = Cast<APlayerController>(GetOwner());

	const AActor* OwnerAct = GetOwner();
	if (!IsValid(OwnerAct)) return;
	const UWorld* World = OwnerAct->GetWorld();
	if (!IsValid(World)) return;
	USFBuildingGridSubsystem* GridSubsystem = World->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem)) return;
	CachedGridSubsystem = GridSubsystem;
	
	UpdatePlacementMode();
	SetComponentTickEnabled(true);
}

void UBuildingPlacement::UpdatePlacementMode()
{
	if (!TargetActor.IsValid())
	{
		ConfirmPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:TargetActor is Invalid"));
		return;
	}

	if (!OwnerController.IsValid())
	{
		ConfirmPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:Controller is Invalid"));
		return;
	}

	FHitResult HitResult;

	if (OwnerController.Get()->GetHitResultUnderCursor(
			ECC_LANDSCAPE,
			false,
			HitResult))
	{
		FVector HitLocation = HitResult.Location;
		if (ToggleGrid)
		{
			const FIntPoint GridPoint = CachedGridSubsystem->WorldCoordinateToGrid(HitLocation);
			// targetactor's gridsize?
			// CachedGridSubsystem->DebugDrawGrid(HitLocation, gridsize);
			HitLocation = CachedGridSubsystem->GridToWorldCoordinate(GridPoint);
		}
		TargetActor.Get()->SetActorLocation(HitLocation);
	}
}

void UBuildingPlacement::ConfirmPlacementMode()
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

void UBuildingPlacement::ToggleGridFunction(bool toggle)
{
	ToggleGrid = toggle;
}