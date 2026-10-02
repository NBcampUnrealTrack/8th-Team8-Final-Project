#include "BuildingPlacement.h"

#include "Logging/SFLog.h"
#include "Game/Grid/SFGridDecalVisualizer.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "NavigationSystem.h"
#include "Buildings/Resource/Interface/SFMinable.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/Synergy/SFBuildingSynergyDefinition.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"

namespace
{
	// 스테이지 로딩 화면 단계인지 확인합니다. InitializationFailed가 앞 순서라 값 비교 대신 단계를 직접 확인합니다.
	// BeginPlay와 HandleStageStateChanged가 격자 데칼 예열의 시작·종료를 판단할 때 사용합니다.
	bool IsStageLoadingState(ESFStageState StageState)
	{
		return StageState == ESFStageState::Initializing ||
			StageState == ESFStageState::LoadingRaceData ||
			StageState == ESFStageState::LoadingInitialBuildingAssets;
	}
}

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

// 격자 표시 액터를 미리 만들고, 스테이지 로딩 중이면 ASFGridDecalVisualizer::BeginWarmUp으로 데칼을 예열합니다.
// 예열 종료는 ASFGameState::OnStageStateChanged → HandleStageStateChanged로 이어집니다.
void UBuildingPlacement::BeginPlay()
{
	Super::BeginPlay();

	// 패키지 빌드는 표시 액터를 만드는 순간 엔진이 데칼 PSO를 비동기로 미리 컴파일합니다.
	ASFGridDecalVisualizer* Visualizer = GetOrCreateGridVisualizer();

	// 에디터 빌드는 그 기능이 꺼져 있으므로 스테이지 로딩 화면 동안 투명 데칼을 그려 PSO를 만듭니다.
	UWorld* World = GetWorld();
	ASFGameState* SFGameState =
		IsValid(World) ? World->GetGameState<ASFGameState>() : nullptr;

	if (!IsValid(Visualizer) ||
		!IsValid(SFGameState) ||
		!IsStageLoadingState(SFGameState->GetStageState()))
	{
		return;
	}

	Visualizer->BeginWarmUp(Cast<APlayerController>(GetOwner()));

	ObservedGameState = SFGameState;
	SFGameState->OnStageStateChanged.AddUniqueDynamic(
		this, &ThisClass::HandleStageStateChanged);
}

// 스테이지 단계가 로딩을 벗어나면 StopGridDecalWarmUp으로 격자 데칼 예열을 멈춥니다.
// BeginPlay에서 ASFGameState::OnStageStateChanged에 연결됩니다.
void UBuildingPlacement::HandleStageStateChanged(ESFStageState NewStageState)
{
	// ReadyToStart나 초기화 실패처럼 로딩이 끝나면 예열을 멈춥니다.
	if (!IsStageLoadingState(NewStageState))
	{
		StopGridDecalWarmUp();
	}
}

// GameState 구독을 해제하고 ASFGridDecalVisualizer::EndWarmUp을 호출합니다.
// HandleStageStateChanged(로딩 종료)와 EndPlay가 호출합니다.
void UBuildingPlacement::StopGridDecalWarmUp()
{
	if (ASFGameState* SFGameState = ObservedGameState.Get())
	{
		SFGameState->OnStageStateChanged.RemoveDynamic(
			this, &ThisClass::HandleStageStateChanged);
	}
	ObservedGameState.Reset();

	if (ASFGridDecalVisualizer* Visualizer = GridVisualizer.Get())
	{
		Visualizer->EndWarmUp();
	}
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

	ResetPlacementEvaluation();
	bCanPlaceCurrentLocation = false;

	// 배치 세션 동안 점유 셀(빨강)과 채굴 가능한 자원 셀(초록)을 표시합니다.
	if (ASFGridDecalVisualizer* Visualizer = GetOrCreateGridVisualizer())
	{
		TArray<FIntPoint> BlockedCells;
		TArray<FIntPoint> ResourceCells;
		FFloatInterval HeightRange;

		GridSubsystem->CollectPlacementDisplayCells(
			MiningBuilding != nullptr,
			MiningBuilding != nullptr
				? MiningBuilding->GetMineResourceType()
				: EResourceType::Mineral,
			BlockedCells,
			ResourceCells,
			HeightRange);

		Visualizer->ShowOccupiedCells(
			BlockedCells, ResourceCells, HeightRange);

		// 시너지를 주는 건물이면 합류 범위를, 버프를 받는 건물이면 받을 수 있는 효과 구역을 함께 표시합니다.
		ShowPlacementSynergyHints(
			Visualizer, InTargetBuilding, BlockedCells, HeightRange);
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

		// 데칼은 유지되므로 격자가 바뀐 프레임에만 위치와 색을 갱신합니다.
		if (ASFGridDecalVisualizer* Visualizer = GridVisualizer.Get())
		{
			// TrySnapWorldCenterToGrid가 NewGridInfo로 스냅한 중심을 그대로 사용합니다.
			Visualizer->ShowPlacementPreview(
				NewGridInfo,
				NewBuildingLocation,
				bCanPlaceCurrentLocation);
		}
	}
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

	// 지형 밖이거나 확정에 실패하면 다음 유효 격자에서 다시 표시합니다.
	if (ASFGridDecalVisualizer* Visualizer = GridVisualizer.Get())
	{
		Visualizer->HidePlacementPreview();
	}
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

	ResetPlacementEvaluation();

	bCanPlaceCurrentLocation = false;
	bIsMiningPlacement = false;
	CachedMineableResourceAmount = 0;
	// Notify after ending the session: zero deposits remain visible during placement,
	// whereas cancellation/confirmation hides the placement-only HUD.
	if (bWasMiningPlacement) OnMineableResourceAmountChanged.Broadcast(0);

	// 표시 액터는 파괴하지 않고 다음 배치 세션에서 재사용합니다.
	if (ASFGridDecalVisualizer* Visualizer = GridVisualizer.Get())
	{
		Visualizer->HidePlacementDisplays();
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

// 선택한 건물의 시너지 클러스터 표시 요청을 ASFGridDecalVisualizer::FlashSynergyClustersOfBuilding에 전달합니다.
// ASFInGamePlayerController::SetSelectedActor가 호출합니다.
void UBuildingPlacement::FlashSynergyClustersOfBuilding(
	const ASFBaseBuilding* Building) const
{
	if (ASFGridDecalVisualizer* Visualizer = GridVisualizer.Get())
	{
		Visualizer->FlashSynergyClustersOfBuilding(Building);
	}
}

// 합류 범위(하늘색)와 받을 수 있는 효과 구역(시너지 색)을 모아 격자 데칼에 함께 표시합니다.
// StartPlacementMode가 점유 칸을 표시한 직후 호출하며, 점유 칸 집합은 두 수집 함수가 함께 씁니다.
void UBuildingPlacement::ShowPlacementSynergyHints(
	ASFGridDecalVisualizer* Visualizer,
	const ASFBaseBuilding* Building,
	const TArray<FIntPoint>& BlockedCells,
	const FFloatInterval& HeightRange) const
{
	if (!IsValid(Visualizer))
	{
		return;
	}

	// 점유 칸은 이미 빨간색으로 표시되므로 두 안내 표시에서 모두 제외합니다.
	TSet<FIntPoint> BlockedCellSet;
	BlockedCellSet.Append(BlockedCells);

	TArray<FIntPoint> LinkCells;
	CollectExistingLinkRangeCells(Building, BlockedCellSet, LinkCells);
	Visualizer->ShowLinkRangeCells(LinkCells, HeightRange);

	TMap<const USFBuildingSynergyDefinition*, TArray<FIntPoint>> EffectCellsBySynergy;
	FFloatInterval EffectHeightRange = HeightRange;
	CollectReceivableEffectCells(
		Building, BlockedCellSet, EffectCellsBySynergy, EffectHeightRange);
	Visualizer->ShowReceivableEffectCells(EffectCellsBySynergy, EffectHeightRange);
}

// 배치 중인 건물과 같은 시너지를 주는 완성 건물들의 링크 범위 칸을 모으고 점유 칸은 뺍니다.
// USFBuildingSynergySubsystem::CollectClusterLinkGridInfos를 쓰며 ShowPlacementSynergyHints가 호출합니다.
void UBuildingPlacement::CollectExistingLinkRangeCells(
	const ASFBaseBuilding* Building,
	const TSet<FIntPoint>& BlockedCellSet,
	TArray<FIntPoint>& OutLinkCells) const
{
	OutLinkCells.Reset();

	const USFBuildingDefinition* Definition =
		IsValid(Building) ? Building->GetBuildingDefinition() : nullptr;

	const USFBuildingSynergySubsystem* Synergy =
		USFBuildingSynergySubsystem::Get(this);

	if (!IsValid(Definition) ||
		Definition->GrantedSynergies.IsEmpty() ||
		!IsValid(Synergy))
	{
		return;
	}

	TSet<FIntPoint> LinkCellSet;
	TArray<FBuildingGridInfo> LinkGridInfos;

	for (const FSFGrantedBuildingSynergy& GrantedSynergy :
		Definition->GrantedSynergies)
	{
		// 배치 중인 건물과 같은 팀·같은 시너지를 주는 완성 건물들의 링크 범위입니다.
		Synergy->CollectClusterLinkGridInfos(
			Building->GetTeamIndex(),
			GrantedSynergy.Definition,
			LinkGridInfos);

		for (const FBuildingGridInfo& LinkGridInfo : LinkGridInfos)
		{
			for (int32 Y = 0; Y < LinkGridInfo.GridSize.Y; ++Y)
			{
				for (int32 X = 0; X < LinkGridInfo.GridSize.X; ++X)
				{
					const FIntPoint Cell =
						LinkGridInfo.OriginCell + FIntPoint(X, Y);

					if (!BlockedCellSet.Contains(Cell))
					{
						LinkCellSet.Add(Cell);
					}
				}
			}
		}
	}

	OutLinkCells = LinkCellSet.Array();
}

// 배치 중인 건물 종류를 버프 대상으로 삼는 내 팀 시너지 효과 구역 칸을 시너지별로 모으고 점유 칸은 뺍니다.
// USFBuildingSynergySubsystem::GetSynergyZonesForTeam(활성 구역)을 쓰며 ShowPlacementSynergyHints가 호출합니다.
void UBuildingPlacement::CollectReceivableEffectCells(
	const ASFBaseBuilding* Building,
	const TSet<FIntPoint>& BlockedCellSet,
	TMap<const USFBuildingSynergyDefinition*, TArray<FIntPoint>>& OutCellsBySynergy,
	FFloatInterval& InOutHeightRange) const
{
	OutCellsBySynergy.Reset();

	const USFBuildingDefinition* Definition =
		IsValid(Building) ? Building->GetBuildingDefinition() : nullptr;

	const USFBuildingSynergySubsystem* Synergy =
		USFBuildingSynergySubsystem::Get(this);

	if (!IsValid(Definition) || !IsValid(Synergy))
	{
		return;
	}

	// 같은 시너지 구역끼리 겹친 칸은 한 번만 칠하도록 시너지별 집합으로 모읍니다.
	TMap<const USFBuildingSynergyDefinition*, TSet<FIntPoint>> CellSetsBySynergy;

	for (const FSFBuildingSynergyZoneInfo& Zone :
		Synergy->GetSynergyZonesForTeam(Building->GetTeamIndex()))
	{
		const USFBuildingSynergyDefinition* SynergyDefinition = Zone.Definition;

		if (!IsValid(SynergyDefinition) ||
			!SynergyDefinition->CanAffectBuildingType(Definition->BuildingType))
		{
			continue;
		}

		TSet<FIntPoint>& Cells = CellSetsBySynergy.FindOrAdd(SynergyDefinition);
		const FBuildingGridInfo& EffectGridInfo = Zone.EffectGridInfo;

		for (int32 Y = 0; Y < EffectGridInfo.GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < EffectGridInfo.GridSize.X; ++X)
			{
				const FIntPoint Cell = EffectGridInfo.OriginCell + FIntPoint(X, Y);

				if (!BlockedCellSet.Contains(Cell))
				{
					Cells.Add(Cell);
				}
			}
		}

		InOutHeightRange.Include(Zone.WorldCenter.Z);
	}

	for (const TPair<const USFBuildingSynergyDefinition*, TSet<FIntPoint>>& Pair :
		CellSetsBySynergy)
	{
		if (!Pair.Value.IsEmpty())
		{
			OutCellsBySynergy.Add(Pair.Key, Pair.Value.Array());
		}
	}
}

// 격자 표시 액터를 한 번만 스폰해 InitializeVisualizer로 초기화하고 이후에는 재사용합니다.
// BeginPlay와 StartPlacementMode가 호출하며 머티리얼은 BP_SFInGamePlayerController의 GridCellDecalMaterial입니다.
ASFGridDecalVisualizer* UBuildingPlacement::GetOrCreateGridVisualizer()
{
	if (IsValid(GridVisualizer))
	{
		return GridVisualizer;
	}

	UWorld* World = GetWorld();
	const USFBuildingGridSubsystem* GridSubsystem =
		IsValid(World) ? World->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;

	if (!IsValid(World) ||
		!IsValid(GridSubsystem) ||
		!IsValid(GridCellDecalMaterial))
	{
		SF_LOG(Warning,
			"[BuildingPlacement] 격자 데칼 머티리얼이 지정되지 않아 "
			"격자를 표시할 수 없습니다.");
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	GridVisualizer = World->SpawnActor<ASFGridDecalVisualizer>(
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (IsValid(GridVisualizer))
	{
		GridVisualizer->InitializeVisualizer(
			GridCellDecalMaterial,
			GridSubsystem->GetCellSize(),
			GridSubsystem->GetGridOrigin());
	}

	return GridVisualizer;
}

// 예열용 GameState 구독을 정리하고 격자 표시 액터를 파괴합니다.
void UBuildingPlacement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopGridDecalWarmUp();

	if (IsValid(GridVisualizer))
	{
		GridVisualizer->Destroy();
	}
	GridVisualizer = nullptr;

	Super::EndPlay(EndPlayReason);
}
