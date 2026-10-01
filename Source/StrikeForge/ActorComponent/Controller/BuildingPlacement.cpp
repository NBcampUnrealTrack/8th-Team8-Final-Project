#include "BuildingPlacement.h"

#include "Logging/SFLog.h"
#include "DrawDebugHelpers.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "NavigationSystem.h"
#include "Buildings/Resource/Interface/SFMinable.h"

EResourceType UBuildingPlacement::GetPlacementResourceType() const
{
	const IMinable* MiningBuilding = Cast<IMinable>(TargetBuilding.Get());
	return MiningBuilding ? MiningBuilding->GetMineResourceType() : EResourceType::Mineral;
}

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

	PlacementGridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			InTargetBuilding->GetGridSize(),
			InTargetBuilding->GetActorRotation());

	TargetBuildingGridInfo = FBuildingGridInfo();
	TargetBuildingGridInfo.GridSize = PlacementGridSize;

	const IMinable* MiningBuilding = Cast<IMinable>(InTargetBuilding);
	bIsMiningPlacement = MiningBuilding != nullptr;

	CachedResourceSearchGridSize = FIntPoint::ZeroValue;

	if (MiningBuilding != nullptr)
	{
		CachedResourceSearchGridSize =
			USFBuildingGridSubsystem::GetRotatedGridSize(
				MiningBuilding->GetResourceSearchGridSize(),
				InTargetBuilding->GetActorRotation());
	}

	ResetPlacementEvaluation();
	bCanPlaceCurrentLocation = false;

	// 기존 영구선을 제거하고 전체 점유 격자를 표시합니다.
	GridSubsystem->DebugDrawOccupiedCells();

	// 자원 셀 인덱스와 매장량을 영구 표시합니다.
	if (MiningBuilding != nullptr)
	{
		GridSubsystem->DebugDrawResourceCells(
			MiningBuilding->GetMineResourceType(),
			-1.0f);
	}

	SetComponentTickEnabled(true);
	if (bIsMiningPlacement)
	{
		OnMineableResourceAmountChanged.Broadcast(CachedMineableResourceAmount);
	}
	UpdatePlacementMode();

	return	TargetBuilding.IsValid() &&	IsComponentTickEnabled();
}

void UBuildingPlacement::UpdatePlacementMode()
{
	ASFBaseBuilding* Building =	TargetBuilding.Get();
	if (!IsValid(Building))
	{
		SF_LOG(
			Warning,
			"[BuildingPlacement] TargetBuilding이 유효하지 않아 "
			"배치 모드를 종료합니다.");

		EndPlacementSession();
		return;
	}

	APlayerController* Controller =	OwnerController.Get();
	if (!IsValid(Controller))
	{
		SF_LOG(
			Warning,
			"[BuildingPlacement] OwnerController가 유효하지 않아 "
			"배치 모드를 종료합니다.");

		EndPlacementSession();
		return;
	}

	USFBuildingGridSubsystem* GridSubsystem = CachedGridSubsystem.Get();
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

	FBuildingGridInfo NewGridInfo;
	FVector NewBuildingLocation;

	if (!GridSubsystem->TrySnapWorldCenterToGrid(
			HitResult.Location,
			PlacementGridSize, NewGridInfo,
			NewBuildingLocation))
	{
		SetCurrentPlacementInvalid();
		return;
	}

	const bool bGridChanged =
		!bHasEvaluatedGrid ||
			LastEvaluatedGridInfo.OriginCell !=
			NewGridInfo.OriginCell ||
		LastEvaluatedGridInfo.GridSize !=
			NewGridInfo.GridSize;

	TargetBuildingGridInfo = NewGridInfo;

	if (bGridChanged)
	{
		Building->SetActorLocation(NewBuildingLocation);

		const FSFBuildingGridEvaluationResult Evaluation =
			GridSubsystem->EvaluatePlayerBuildingGrid(
				Building, NewGridInfo);

		bCanPlaceCurrentLocation =
			CanPlaceAtCurrentLocation(Evaluation.bCanPlaceOnGrid);

		if (bIsMiningPlacement)
		{
			CachedMineableResourceAmount = FMath::Max(0, Evaluation.MineableResourceAmount);
			OnMineableResourceAmountChanged.Broadcast(
				CachedMineableResourceAmount);
			SF_LOG(Log, "Total Resource Amount : %d", Evaluation.MineableResourceAmount);
		}

		Building->ApplyPreviewMaterial(
			bCanPlaceCurrentLocation);

		LastEvaluatedGridInfo = NewGridInfo;
		bHasEvaluatedGrid = true;
	}

	// 현재 건물 격자는 한 프레임만 유지되므로 매 Tick 그립니다.
	DrawCurrentPlacementGrid();
}

void UBuildingPlacement::SetCurrentPlacementInvalid()
{
	const bool bHadEvaluatedGrid = bHasEvaluatedGrid;

	bCanPlaceCurrentLocation = false;
	bHasEvaluatedGrid = false;

	// 지형 밖에 머무는 동안 매 Tick 0을 전송하지 않습니다.
	if (bIsMiningPlacement && bHadEvaluatedGrid)
	{
		CachedMineableResourceAmount = 0;
		OnMineableResourceAmountChanged.Broadcast(0);
	}

	ASFBaseBuilding* Building =	TargetBuilding.Get();
	if (IsValid(Building))
	{
		Building->ApplyPreviewMaterial(false);
	}
}

void UBuildingPlacement::DrawCurrentPlacementGrid() const
{
	const ASFBaseBuilding* Building =
		TargetBuilding.Get();

	const USFBuildingGridSubsystem* GridSubsystem =
		CachedGridSubsystem.Get();

	if (!IsValid(Building) ||
		!IsValid(GridSubsystem) ||
		!TargetBuildingGridInfo.IsValid())
	{
		return;
	}

	if (bIsMiningPlacement)
	{
		GridSubsystem->DebugDrawMiningPlacementGrid(
			TargetBuildingGridInfo,
			CachedResourceSearchGridSize,
			Building->GetActorLocation().Z,
			0.0f);

		return;
	}

	GridSubsystem->DebugDrawGridInfo(
		TargetBuildingGridInfo,
		Building->GetActorLocation().Z,
		bCanPlaceCurrentLocation,
		0.0f);
}

void UBuildingPlacement::ResetPlacementEvaluation()
{
	LastEvaluatedGridInfo =	FBuildingGridInfo();
	bHasEvaluatedGrid = false;
}

bool UBuildingPlacement::CanPlaceAtCurrentLocation(
	const bool bCanPlaceOnGrid) const
{
	const ASFBaseBuilding* Building =
		TargetBuilding.Get();

	if (!IsValid(Building) ||
		!IsValid(CachedGridSubsystem) ||
		!TargetBuildingGridInfo.IsValid())
	{
		return false;
	}

	const bool bUsesResourceGrid =
		Cast<IMinable>(Building) != nullptr;

	const bool bCanPlaceOnNavigation =
		bUsesResourceGrid ||
		CanPlaceOnNavMeshXY(
			TargetBuildingGridInfo);

	return
		bCanPlaceOnGrid &&
		bCanPlaceOnNavigation &&
		USFTutorialSubsystem::IsBuildLocationAllowed(
			this,
			Building->GetActorLocation());
}

void UBuildingPlacement::EndPlacementSession()
{
	SetComponentTickEnabled(false);

	const bool bWasMiningPlacement = bIsMiningPlacement;

	TargetBuilding.Reset();
	OwnerController.Reset();
	CachedGridSubsystem = nullptr;

	TargetBuildingGridInfo =
		FBuildingGridInfo();

	PlacementGridSize =
		FIntPoint(1, 1);

	CachedResourceSearchGridSize =
		FIntPoint::ZeroValue;

	ResetPlacementEvaluation();

	bCanPlaceCurrentLocation = false;
	bIsMiningPlacement = false;
	CachedMineableResourceAmount = 0;
	// Notify after ending the session: zero deposits remain visible during placement,
	// whereas cancellation/confirmation hides the placement-only HUD.
	if (bWasMiningPlacement) OnMineableResourceAmountChanged.Broadcast(0);

	if (UWorld* World = GetWorld())
	{
		// 배치 세션에서 생성한 영구 격자와 자원 문자열을 정리합니다.
		FlushPersistentDebugLines(World);
		FlushDebugStrings(World);
	}
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
		!bHasEvaluatedGrid)
	{
		return false;
	}

	const FSFBuildingGridEvaluationResult FinalEvaluation =
	CachedGridSubsystem->EvaluatePlayerBuildingGrid(
		Building,
		TargetBuildingGridInfo);

	if (!CanPlaceAtCurrentLocation(
			FinalEvaluation.bCanPlaceOnGrid))
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
