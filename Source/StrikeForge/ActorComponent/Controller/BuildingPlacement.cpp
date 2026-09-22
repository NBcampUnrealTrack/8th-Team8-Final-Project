#include "BuildingPlacement.h"
#include "Logging/SFLog.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "NavigationSystem.h"
#include "Buildings/Resource/Interface/SFMinable.h"

UBuildingPlacement::UBuildingPlacement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UBuildingPlacement::StartPlacementMode(
	ASFBaseBuilding* InTargetBuilding)
{
	EndPlacementSession();

	if (!IsValid(InTargetBuilding))
	{
		SF_LOG(Warning,
			"[BuildingPlacement] 유효하지 않은 건물로 "
			"배치 모드를 시작하려 했습니다.");
		return false;
	}
	
	APlayerController* Controller =	Cast<APlayerController>(GetOwner());
	if (!IsValid(Controller))
	{
		SF_LOG(Warning,
			"[BuildingPlacement] Owner가 "
			"PlayerController가 아닙니다.");
		return false;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		SF_LOG(Warning,
			"[BuildingPlacement] 유효한 World가 없습니다.");
		return false;
	}
	
	USFBuildingGridSubsystem* GridSubsystem = World->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem))
	{
		SF_LOG(Warning,
			"[BuildingPlacement] BuildingGridSubsystem을 "
			"찾을 수 없습니다.");

		return false;
	}
	
	TargetBuilding = InTargetBuilding;
	OwnerController = Controller;
	CachedGridSubsystem = GridSubsystem;

	const FIntPoint BaseGridSize =	InTargetBuilding->GetGridSize();
	// DA의 로컬 크기에 Actor 회전을 적용하여 월드 격자 점유 크기로 변환합니다.
	TargetBuildingGridInfo.GridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			BaseGridSize, InTargetBuilding->GetActorRotation());

	bCanPlaceCurrentLocation = false;

	// 배치 시작 시점의 현재 점유 격자를 한 번 표시합니다.
	GridSubsystem->DebugDrawOccupiedCells();
	
	SetComponentTickEnabled(true);
	UpdatePlacementMode();
	
	// BuildingRef이 isvalid 한가 확인, tick이 on인지 확인
	return TargetBuilding.IsValid() && IsComponentTickEnabled();
}

void UBuildingPlacement::UpdatePlacementMode()
{
	ASFBaseBuilding* Building =
		TargetBuilding.Get();
	if (!IsValid(Building))
	{
		SF_LOG(
			Warning,
			"[BuildingPlacement] TargetBuilding이 유효하지 않아 "
			"배치 모드를 종료합니다.");

		EndPlacementSession();
		return;
	}

	APlayerController* Controller =
		OwnerController.Get();
	if (!IsValid(Controller))
	{
		SF_LOG(
			Warning,
			"[BuildingPlacement] OwnerController가 유효하지 않아 "
			"배치 모드를 종료합니다.");

		EndPlacementSession();
		return;
	}

	USFBuildingGridSubsystem* GridSubsystem =
		CachedGridSubsystem.Get();
	if (!IsValid(GridSubsystem))
	{
		SF_LOG(
			Warning,
			"[BuildingPlacement] BuildingGridSubsystem이 유효하지 않아 "
			"배치 모드를 종료합니다.");

		EndPlacementSession();
		return;
	}

	FHitResult HitResult;

	if (!Controller->GetHitResultUnderCursor(
			ECC_LANDSCAPE,
			false,
			HitResult))
	{
		// 지형을 찾지 못한 프레임은 배치 불가로 처리합니다.
		SetCurrentPlacementInvalid();
		return;
	}

	FVector BuildingLocation;

	if (!GridSubsystem->TrySnapWorldCenterToGrid(
			HitResult.Location,
			TargetBuildingGridInfo.GridSize,
			TargetBuildingGridInfo,
			BuildingLocation))
	{
		SetCurrentPlacementInvalid();
		return;
	}

	Building->SetActorLocation(BuildingLocation);

	bCanPlaceCurrentLocation = CanPlaceAtCurrentLocation();
	const IMinable* MinableBuilding = Cast<IMinable>(Building);

	if (MinableBuilding)
	{
		GridSubsystem->DebugDrawResourceCells(
			MinableBuilding->GetMineResourceType(),
			0.0f);

		// 실제 자원 검색 판정과 같은 회전된 검색 크기를 디버그 표시에 사용합니다.
		const FIntPoint EffectiveSearchGridSize =
			USFBuildingGridSubsystem::GetRotatedGridSize(
				MinableBuilding->GetResourceSearchGridSize(),
				Building->GetActorRotation());

		GridSubsystem->DebugDrawMiningPlacementGrid(
			TargetBuildingGridInfo,
			EffectiveSearchGridSize,
			BuildingLocation.Z,
			0.0f);
	}
	else
	{
		GridSubsystem->DebugDrawGridInfo(
			TargetBuildingGridInfo,
			BuildingLocation.Z,
			bCanPlaceCurrentLocation,
			0.0f);
	}

	Building->ApplyPreviewMaterial(bCanPlaceCurrentLocation);
}

void UBuildingPlacement::SetCurrentPlacementInvalid()
{
	bCanPlaceCurrentLocation = false;

	ASFBaseBuilding* Building =
		TargetBuilding.Get();

	if (IsValid(Building))
	{
		Building->ApplyPreviewMaterial(false);
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

bool UBuildingPlacement::CanPlaceAtCurrentLocation() const
{
	const ASFBaseBuilding* Building = TargetBuilding.Get();

	if (!IsValid(Building) || !IsValid(CachedGridSubsystem) ||
		!TargetBuildingGridInfo.IsValid())
	{
		return false;
	}

	const FVector BuildingLocation = Building->GetActorLocation();

	const bool bUsesResourceGrid = Cast<IMinable>(Building) != nullptr;

	// 광산은 자원 셀 위에 배치되므로 현재 NavMesh 경계 검사에서 제외합니다.
	const bool bCanPlaceOnNavigation =
		bUsesResourceGrid || CanPlaceOnNavMeshXY(TargetBuildingGridInfo);
	
	return CanPlaceOnCurrentGrid() &&
		bCanPlaceOnNavigation &&
		USFTutorialSubsystem::IsBuildLocationAllowed(
			this, BuildingLocation);
}

void UBuildingPlacement::EndPlacementSession()
{
	SetComponentTickEnabled(false);

	TargetBuilding.Reset();
	OwnerController.Reset();
	CachedGridSubsystem = nullptr;
	TargetBuildingGridInfo = FBuildingGridInfo();
	bCanPlaceCurrentLocation = false;
	
	FlushPersistentDebugLines(GetWorld());
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

	// 확정 직전에도 같은 최종 규칙으로 다시 검사합니다.
	if (!CanPlaceAtCurrentLocation())
	{
		SetCurrentPlacementInvalid();
		return false;
	}

	// 등록과 자원 연결 모두 GridSubsystem이 처리한다.
	if (!Building->TryRegisterGrid(TargetBuildingGridInfo))
	{
		SetCurrentPlacementInvalid();
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

bool UBuildingPlacement::CanPlaceOnNavMeshXY(const FBuildingGridInfo& GridInfo) const
{
	const USFBuildingGridSubsystem* GridSubsystem =
		CachedGridSubsystem.Get();

	if (!IsValid(GridSubsystem) || !GridInfo.IsValid())
	{
		return false;
	}

	UNavigationSystemV1* NavSystem =
		UNavigationSystemV1::GetCurrent(GetWorld());
	if (!IsValid(NavSystem)) return false;

	ANavigationData* NavigationData =
		NavSystem->GetDefaultNavDataInstance();
	if (!IsValid(NavigationData)) return false;

	const FBox NavigationBounds = NavigationData->GetBounds();
	if (!NavigationBounds.IsValid) return false;

	const float CellSize = GridSubsystem->GetCellSize();
	const FVector& GridOrigin = GridSubsystem->GetGridOrigin();

	// OriginCell이 가리키는 첫 셀의 왼쪽 아래 모서리입니다.
	const float GridMinX =
		GridOrigin.X + GridInfo.OriginCell.X * CellSize;

	const float GridMinY =
		GridOrigin.Y + GridInfo.OriginCell.Y * CellSize;

	// 건물이 차지하는 마지막 셀의 오른쪽 위 모서리입니다.
	const float GridMaxX =
		GridMinX + GridInfo.GridSize.X * CellSize;

	const float GridMaxY =
		GridMinY + GridInfo.GridSize.Y * CellSize;

	constexpr float BoundaryTolerance = 1.0f;

	return
		GridMinX >= NavigationBounds.Min.X -
			BoundaryTolerance &&
		GridMinY >= NavigationBounds.Min.Y -
			BoundaryTolerance &&
		GridMaxX <= NavigationBounds.Max.X +
			BoundaryTolerance &&
		GridMaxY <= NavigationBounds.Max.Y +
			BoundaryTolerance;
}
