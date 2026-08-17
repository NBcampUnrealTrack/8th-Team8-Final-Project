#include "BuildingPlacement.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "NavigationSystem.h"

UBuildingPlacement::UBuildingPlacement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UBuildingPlacement::StartPlacementMode(ASFBaseBuilding* InTargetBuilding)
{
	EndPlacementSession();

	if (!IsValid(InTargetBuilding)) return false;
	APlayerController* Controller =	Cast<APlayerController>(GetOwner());
	if (!IsValid(Controller)) return false;

	USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem)) return false;
	
	GridSubsystem->DebugDrawOccupiedCells(5.f);
	
	TargetBuilding = InTargetBuilding;
	OwnerController = Controller;
	CachedGridSubsystem = GridSubsystem;
	TargetBuildingGridInfo.GridSize = InTargetBuilding->GetGridSize();

	bCanPlaceCurrentLocation = false;

	SetComponentTickEnabled(true);
	UpdatePlacementMode();
	
	// BuildingRef이 isvalid 한가 확인, tick이 on인지 확인
	return IsPlacementActive();
}

void UBuildingPlacement::UpdatePlacementMode()
{
	if (!TargetBuilding.IsValid())
	{
		CancelPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:TargetActor is Invalid"));
		return;
	}

	if (!OwnerController.IsValid())
	{
		CancelPlacementMode();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:Controller is Invalid"));
		return;
	}
	
	FHitResult HitResult;

	if (OwnerController.Get()->GetHitResultUnderCursor(
			ECC_LANDSCAPE,
			false,
			HitResult))
	{
		
		TargetBuildingGridInfo =
			CachedGridSubsystem->WorldCenterToGridInfo(
				HitResult.Location,
				TargetBuildingGridInfo.GridSize);

		FVector BuildingLocation =
			CachedGridSubsystem->GridInfoToWorldCenter(
				TargetBuildingGridInfo);

		BuildingLocation.Z = HitResult.Location.Z;

		TargetBuilding->SetActorLocation(
			BuildingLocation);

		const bool bCanPlaceOnGrid =
			CachedGridSubsystem->CanPlaceBuildingGrid(
				TargetBuilding.Get(),
				TargetBuildingGridInfo);

		const bool bCanPlaceOnNavigation =
			CanPlaceOnNavMeshXY(BuildingLocation);

		bCanPlaceCurrentLocation =
			bCanPlaceOnGrid &&
			bCanPlaceOnNavigation;
		
		CachedGridSubsystem->DebugDrawGridInfo(
			TargetBuildingGridInfo,
			BuildingLocation.Z,
			bCanPlaceCurrentLocation,
			0.0f);
		
		TargetBuilding->ApplyPreviewMaterial(
			bCanPlaceCurrentLocation);
	}
	else
	{	// LandScape Trace Failed.
		bCanPlaceCurrentLocation = false;
		TargetBuilding->ApplyPreviewMaterial(false);
		return;
	}
}

void UBuildingPlacement::EndPlacementSession()
{
	// tick off
	SetComponentTickEnabled(false);
	// cached 정보 초기화
	TargetBuilding.Reset();
	OwnerController.Reset();
	CachedGridSubsystem = nullptr;
	TargetBuildingGridInfo = FBuildingGridInfo();
	bCanPlaceCurrentLocation = false;
}

void UBuildingPlacement::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	UpdatePlacementMode();
}

bool UBuildingPlacement::TryConfirmPlacementMode()
{
	ASFBaseBuilding* Building =
	TargetBuilding.Get();

	if (!IsValid(Building) ||
		!IsValid(CachedGridSubsystem) ||
		!bCanPlaceCurrentLocation)
	{
		return false;
	}
	
	// NavMesh 범위 판단
	if (!CanPlaceOnNavMeshXY(Building->GetActorLocation()))
	{
		bCanPlaceCurrentLocation = false;
		Building->ApplyPreviewMaterial(false);
		return false;
	}
	
	// building안에서 grid subsystem을 부르고 등록가능한지 판단
	// 가능하면 set TargetBuildingGridInfo
	if (!Building->TryRegisterGrid(TargetBuildingGridInfo))
	{
		bCanPlaceCurrentLocation = false;
		Building->ApplyPreviewMaterial(false);
		return false;
	}
	
	// tick off
	EndPlacementSession();
	return true;
}

void UBuildingPlacement::CancelPlacementMode()
{
	EndPlacementSession();
}

bool UBuildingPlacement::IsPlacementActive() const
{
	return TargetBuilding.IsValid() && IsComponentTickEnabled();
}

bool UBuildingPlacement::CanPlaceOnNavMeshXY(const FVector& WorldLocation) const
{
	UNavigationSystemV1* NavSys =
	   UNavigationSystemV1::GetCurrent(GetWorld());

	if (!IsValid(NavSys) ||
		!NavSys->GetDefaultNavDataInstance())
	{
		return false;
	}

	FNavLocation ProjectedLocation;

	// 근처 NavMesh를 검색하는 범위입니다.
	const FVector QueryExtent(
		100.0f,
		100.0f,
		1000.0f);

	if (!NavSys->ProjectPointToNavigation(
			WorldLocation,
			ProjectedLocation,
			QueryExtent))
	{
		return false;
	}

	// Z는 무시하고 XY 차이만 검사합니다.
	constexpr float MaxXYError = 5.0f;

	return FVector::DistSquared2D(
			   WorldLocation,
			   ProjectedLocation.Location)
		<= FMath::Square(MaxXYError);
}
