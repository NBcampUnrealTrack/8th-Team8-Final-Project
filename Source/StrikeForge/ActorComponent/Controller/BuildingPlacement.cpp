#include "BuildingPlacement.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Building/MiningBuilding/SFMiningBuilding.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "NavigationSystem.h"
#include "Buildings/Resource/Interface/SFMinable.h"

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
	
	
	if (InTargetBuilding->IsA<ASFMiningBuilding>())
	{
		EResourceType Type = Cast<ASFMiningBuilding>(InTargetBuilding)->GetMineResourceType();
		GridSubsystem->DebugDrawResourceCells(Type, 10.f);
	}
	else
	{
		GridSubsystem->DebugDrawOccupiedCells(5.f);
	}
	
	TargetBuilding = InTargetBuilding;
	OwnerController = Controller;
	CachedGridSubsystem = GridSubsystem;
	TargetBuildingGridInfo.GridSize = InTargetBuilding->GetGridSize();

	bCanPlaceCurrentLocation = false;

	SetComponentTickEnabled(true);
	UpdatePlacementMode();
	
	// BuildingRef이 isvalid 한가 확인, tick이 on인지 확인
	return TargetBuilding.IsValid() && IsComponentTickEnabled();
}

void UBuildingPlacement::UpdatePlacementMode()
{
	if (!TargetBuilding.IsValid())
	{
		EndPlacementSession();
		UE_LOG(LogTemp, Error, TEXT("UBuildingPlacement::UpdatePlacementMode:TargetActor is Invalid"));
		return;
	}

	if (!OwnerController.IsValid())
	{
		EndPlacementSession();
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

		const bool bCanPlaceOnGrid = CanPlaceOnCurrentGrid();
		
		const bool bUsesResourceGrid =
			Cast<IMinable>(
		TargetBuilding.Get()) != nullptr;

		const bool bCanPlaceOnNavigation =
			bUsesResourceGrid ||
			CanPlaceOnNavMeshXY(BuildingLocation);

		// 튜토리얼이 자리를 제한하는 중이면 그것도 조건에 넣는다.
		// 별도 문장으로 부르면 반환값이 버려져서, 경고만 뜨고 배치는 그대로
		// 허용되는 상태가 된다.
		bCanPlaceCurrentLocation =
			bCanPlaceOnGrid &&
			bCanPlaceOnNavigation &&
			USFTutorialSubsystem::IsBuildLocationAllowed(this, BuildingLocation);
		
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

bool UBuildingPlacement::CanPlaceOnCurrentGrid() const
{
	ASFBaseBuilding* Building = TargetBuilding.Get();

	if (!IsValid(Building) ||
		!IsValid(CachedGridSubsystem) ||
		!TargetBuildingGridInfo.IsValid())
	{
		return false;
	}

	return CachedGridSubsystem->CanPlacePlayerBuildingGrid(
		Building,
		TargetBuildingGridInfo);
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

	const bool bUsesResourceGrid =
		Cast<IMinable>(Building) != nullptr;

	const bool bCanPlaceOnNavigation =
		bUsesResourceGrid ||
		CanPlaceOnNavMeshXY(
			Building->GetActorLocation());

	if (!CanPlaceOnCurrentGrid() ||
		!bCanPlaceOnNavigation)
	{
		bCanPlaceCurrentLocation = false;
		Building->ApplyPreviewMaterial(false);
		return false;
	}

	// 튜토리얼 자리 제한은 여기서도 본다.
	//
	// 위쪽 틱 판정만 믿으면 판정과 클릭 사이 한 프레임에 자리가 바뀐 경우를
	// 놓친다. 실제로 짓는 문은 이 함수 하나뿐이므로 여기서 막는 것이 확실하다.
	if (!USFTutorialSubsystem::IsBuildLocationAllowed(
			this,
			Building->GetActorLocation()))
	{
		bCanPlaceCurrentLocation = false;
		Building->ApplyPreviewMaterial(false);
		return false;
	}

	// 등록과 자원 연결 모두 GridSubsystem이 처리한다.
	if (!Building->TryRegisterGrid(
		TargetBuildingGridInfo))
	{
		bCanPlaceCurrentLocation = false;
		Building->ApplyPreviewMaterial(false);
		return false;
	}

	// 고스트가 요구한 건물을 이 위치에 확정했다면, 완공될 때까지
	// 같은 건물을 포함한 새 건설을 막는다.
	USFTutorialSubsystem::NotifyTutorialBuildingPlacementConfirmed(this, Building);

	// 짓겠다고 확정한 순간 안내용 모형을 치운다.
	// 건설이 끝날 때까지 두면 실제 건물과 겹쳐 서 있어서 무엇이 진짜인지
	// 알 수 없다.
	USFTutorialSubsystem::HidePlacementGhost(this);

	EndPlacementSession();
	return true;
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
