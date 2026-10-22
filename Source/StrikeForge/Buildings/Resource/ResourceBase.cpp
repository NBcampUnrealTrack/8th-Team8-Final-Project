#include "ResourceBase.h"
#include "Components/BoxComponent.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Logging/SFLog.h"
#include "Net/UnrealNetwork.h"
#if WITH_EDITOR
#include "EngineUtils.h"
#include "Game/Spawn/SFBuildingSpawnAnchor.h"
#endif
AResourceBase::AResourceBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 배치 격자 데칼이 자원 표면에 묻지 않도록 합니다.
	StaticMeshComponent->bReceivesDecals = false;
	
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	BoxComponent->SetupAttachment(GetRootComponent());

	BoxComponent->SetVisibility(false);
	// BoxComponent->SetLineThickness(2.f);
	BoxComponent->SetHiddenInGame(true);
	
	BoxComponent->SetCollisionProfileName("SpawnedBuilding");
#if WITH_EDITORONLY_DATA

	GridPreviewComponent =
		CreateDefaultSubobject<UBoxComponent>(TEXT("GridPreviewComponent"));

	GridPreviewComponent->SetupAttachment(GetRootComponent());

	GridPreviewComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	GridPreviewComponent->SetGenerateOverlapEvents(false);
	GridPreviewComponent->SetCanEverAffectNavigation(false);

	// 에디터에서는 보이지만 PIE와 게임에서는 보이지 않습니다.
	GridPreviewComponent->SetHiddenInGame(true);
	GridPreviewComponent->SetVisibility(true);

	GridPreviewComponent->ShapeColor = FColor::Green;
	GridPreviewComponent->bDrawOnlyIfSelected = false;
	GridPreviewComponent->SetLineThickness(5.0f);

#endif
}

void AResourceBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AResourceBase, ResourceDeposit);
	DOREPLIFETIME(AResourceBase, CellDeposits);
}

void AResourceBase::OnRep_CellDeposits()
{
	if (!bResourceGridRegistered) return;
	if (auto* Grid = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>())
		for (int32 I = 0; I < CellDeposits.Num(); ++I)
			if (CellDeposits[I] <= 0) Grid->UnregisterResourceCell(this, RegisteredResourceGridInfo, I);
}

void AResourceBase::InitializeCellDeposits()
{
	CellDeposits.Reset();

	if (ResourceGridSize.X <= 0 || ResourceGridSize.Y <= 0)
	{
		SF_LOG(Error,
			"[%s] 잘못된 ResourceGridSize입니다. GridSize=%s",
			*GetName(),	*ResourceGridSize.ToString());

		return;
	}

	const int32 CellCount =
		ResourceGridSize.X * ResourceGridSize.Y;

	ResourceDeposit =
		FMath::Max(0, ResourceDeposit);

	CellDeposits.SetNumUninitialized(CellCount);

	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		CellDeposits[Index] =
			CalculateDistributedCellDeposit(
				ResourceDeposit,
				ResourceGridSize,
				Index);
	}
}

int32 AResourceBase::CalculateDistributedCellDeposit(
	int32 TotalDeposit,
	const FIntPoint& GridSize,
	int32 ResourceCellIndex)
{
	if (TotalDeposit <= 0 || GridSize.X <= 0 || GridSize.Y <= 0)
	{
		return 0;
	}

	const int32 CellCount = GridSize.X * GridSize.Y;

	if (ResourceCellIndex < 0 || ResourceCellIndex >= CellCount)
	{
		return 0;
	}

	const int32 BaseDeposit = TotalDeposit / CellCount;
	const int32 Remainder = TotalDeposit % CellCount;

	return BaseDeposit + (ResourceCellIndex < Remainder ? 1 : 0);
}

int32 AResourceBase::GetCellDeposit(
	int32 ResourceCellIndex) const
{
	if (!CellDeposits.IsValidIndex(ResourceCellIndex))
	{
		return 0;
	}

	return CellDeposits[ResourceCellIndex];
}

int32 AResourceBase::ExtractFromCell(
	int32 ResourceCellIndex,
	int32 ExtractAmount)
{
	if (!HasAuthority()) return 0;
	if (ExtractAmount <= 0 || ResourceDeposit <= 0 ||
		!CellDeposits.IsValidIndex(ResourceCellIndex))
	{
		return 0;
	}

	int32& CellDeposit = CellDeposits[ResourceCellIndex];

	if (CellDeposit <= 0)
	{
		return 0;
	}

	const int32 ExtractedAmount =
		FMath::Min(ExtractAmount,CellDeposit);

	CellDeposit -= ExtractedAmount;

	ResourceDeposit =
		FMath::Max(0,ResourceDeposit - ExtractedAmount);

	/*
 * 셀 매장량이 방금 0이 되었다면
 * 해당 셀의 자원 점유 정보만 해제합니다.
 *
 * CellDeposits 원소는 제거하지 않습니다.
 * ResourceCellIndex는 계속 고정되어야 합니다.
 */
	if (CellDeposit == 0 &&	bResourceGridRegistered)
	{
		UWorld* World = GetWorld();

		USFBuildingGridSubsystem* GridSubsystem = IsValid(World)
				? World->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;

		if (IsValid(GridSubsystem))
		{
			GridSubsystem->UnregisterResourceCell(
				this,
				RegisteredResourceGridInfo,
				ResourceCellIndex);
		}
	}
	
	if (ResourceDeposit == 0)
	{
		SF_LOG(Log,
			"%s :: 모든 자원 셀 고갈",
			*UEnum::GetValueAsString(ResourceType));

		/*
		 * 외부에서 고갈 연출이나 저장 처리를 할 수 있도록
		 * 액터를 제거하기 전에 알립니다.
		 */
		OnDepleted.Broadcast(this);
		
		// destroy effect
		Destroy();
	}

	return ExtractedAmount;
}

int32 AResourceBase::Extract(int32 InDigAmount)
{
	if (!HasAuthority()) return 0;
	if (InDigAmount <= 0 || ResourceDeposit <= 0)
	{
		return 0;
	}

	int32 RemainingRequest = InDigAmount;
	int32 TotalExtracted = 0;

	/*
	 * Extract()는 셀 인덱스를 모르는 기존 호출자를 위한
	 * 호환 함수입니다. 앞쪽의 남은 셀부터 순서대로 채굴합니다.
	 */
	for (int32 Index = 0; Index < CellDeposits.Num() &&	RemainingRequest > 0; ++Index)
	{
		const int32 ExtractedFromCell =
			ExtractFromCell(Index,RemainingRequest);

		TotalExtracted += ExtractedFromCell;
		RemainingRequest -=	ExtractedFromCell;
	}

	return TotalExtracted;
}

FSFSelectableInfo AResourceBase::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info;
	SF_LOG(Warning, "%s Selected.", *UEnum::GetValueAsString(ResourceType));
	
	Info.SelectableType =
		ESFSelectableType::ResourceBuilding;

	Info.bShowHealth = false;
	Info.bShowAttackPower = false;
	Info.bShowMoveSpeed = false;
	Info.bShowSkillIcons = false;

	Info.DataRowName = SelectionUIDataRowName;

	return Info;
}

FBeMinedableData AResourceBase::GetBeMinedableData() const
{
	FBeMinedableData Data;
	
	Data.ResourceType = ResourceType;
	Data.ResourceLeftAmount = ResourceDeposit;
	Data.ResourceTier = ResourceTier;
	Data.ResourceRequiredWork = ResourceRequiredWork;
	
	return Data;
}

void AResourceBase::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeCellDeposits();
	
	USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem)) return;
	
	FVector WorldCenterVector;

	if (!GridSubsystem->TrySnapWorldCenterToGrid(
			GetActorLocation(),
			ResourceGridSize,
			RegisteredResourceGridInfo,
			WorldCenterVector))
	{
		SF_LOG(Warning,
			"Resource: %s Failed to Calculate GridSystem",
			*GetName());

		Destroy();
		return;
	}

	SetActorLocation(WorldCenterVector);
	
	bResourceGridRegistered  = 
	GridSubsystem->TryRegisterResourceGrid(this, RegisteredResourceGridInfo);
	
	if (!bResourceGridRegistered)
	{
		SF_LOG(Warning,
			"Resource: %s Failed to Register GridSystem",
			*GetName());

		Destroy();
		return;
	}

	SetBoxExtent();

}

void AResourceBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bResourceGridRegistered )
	{
		if (USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>())
		{
			GridSubsystem->UnregisterResourceGrid(this, RegisteredResourceGridInfo);
		}
		
		bResourceGridRegistered  = false;
	}
	
	Super::EndPlay(EndPlayReason);
}

void AResourceBase::SetBoxExtent()
{
	const FBoxSphereBounds BoundsInParent = StaticMeshComponent->CalcBounds(
		StaticMeshComponent->GetRelativeTransform());
	const FVector DownSizeBoxExtent = BoundsInParent.BoxExtent * 0.85f;
	const float SubBoxExtentZ = BoundsInParent.BoxExtent.Z - DownSizeBoxExtent.Z;
	FVector CustomOrigin = BoundsInParent.Origin;
	CustomOrigin.Z -= SubBoxExtentZ;
	
	BoxComponent->SetBoxExtent(DownSizeBoxExtent);
	BoxComponent->SetRelativeLocation(CustomOrigin);
}

#if WITH_EDITOR
void AResourceBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	SetBoxExtent();

	if (USFBuildingGridSubsystem* GridSubsystem =
			World->GetSubsystem<USFBuildingGridSubsystem>())
	{
		GridSubsystem->RefreshAllEditorGridPreviews();
	}
}

void AResourceBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	// 드래그 중에는 자유 이동을 유지하고 마우스를 놓았을 때만 스냅합니다.
	if (!bFinished)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World) || World->IsGameWorld())
	{
		return;
	}

	USFBuildingGridSubsystem* GridSubsystem =
		World->GetSubsystem<USFBuildingGridSubsystem>();

	if (!IsValid(GridSubsystem))
	{
		return;
	}

	FBuildingGridInfo EditorGridInfo;
	FVector SnappedLocation;

	if (!GridSubsystem->TrySnapWorldCenterToGrid(
			GetActorLocation(),
			ResourceGridSize,
			EditorGridInfo,
			SnappedLocation))
	{
		return;
	}

	if (!GetActorLocation().Equals(
			SnappedLocation,
			KINDA_SMALL_NUMBER))
	{
		SetActorLocation(
			SnappedLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	
	GridSubsystem->RefreshAllEditorGridPreviews();
}

void AResourceBase::PostEditUndo()
{
	Super::PostEditUndo();
	if (UWorld* World = GetWorld())
	{
		if (USFBuildingGridSubsystem* GridSubsystem =
				World->GetSubsystem<USFBuildingGridSubsystem>())
		{
			GridSubsystem->RefreshAllEditorGridPreviews();
		}
	}
}

void AResourceBase::Destroyed()
{
	if (UWorld* World = GetWorld())
	{
		if (USFBuildingGridSubsystem* GridSubsystem =
				World->GetSubsystem<USFBuildingGridSubsystem>())
		{
			GridSubsystem->RefreshAllEditorGridPreviews();
		}
	}

	Super::Destroyed();
}

void AResourceBase::RefreshEditorGridPreview()
{
#if WITH_EDITORONLY_DATA

	if (!IsValid(GridPreviewComponent))
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World) || World->IsGameWorld())
	{
		GridPreviewComponent->SetVisibility(false);
		return;
	}

	USFBuildingGridSubsystem* GridSubsystem =
		World->GetSubsystem<USFBuildingGridSubsystem>();

	if (!IsValid(GridSubsystem) ||
		ResourceGridSize.X <= 0 ||
		ResourceGridSize.Y <= 0)
	{
		GridPreviewComponent->SetVisibility(false);
		return;
	}

	const float CellSize =
		GridSubsystem->GetCellSize();

	const FVector PreviewExtent(
		ResourceGridSize.X * CellSize * 0.5f,
		ResourceGridSize.Y * CellSize * 0.5f,
		5.0f);

	GridPreviewComponent->SetBoxExtent(
		PreviewExtent,
		false);

	GridPreviewComponent->SetRelativeLocation(
		FVector(0.0f, 0.0f, 5.0f));

	const FBuildingGridInfo EditorGridInfo =
	GridSubsystem->WorldCenterToGridInfo(
		GetActorLocation(), ResourceGridSize);

	const bool bHasResourceConflict =
	HasEditorResourceGridConflict(
		EditorGridInfo,	GridSubsystem);

	const bool bHasSpawnAnchorConflict =
		HasEditorSpawnAnchorGridConflict(
			EditorGridInfo,	GridSubsystem);

	const bool bHasInvalidDeposit = ResourceDeposit <= 0;

	const bool bHasConflict =
		bHasInvalidDeposit ||
		bHasResourceConflict ||
		bHasSpawnAnchorConflict;

	GridPreviewComponent->ShapeColor =
		bHasConflict
			? FColor::Red
			: FColor::Green;

	/*
	 * ShapeColor만 변경하면 기존 SceneProxy에
	 * 색상 변경이 즉시 반영되지 않을 수 있습니다.
	 */
	GridPreviewComponent->MarkRenderStateDirty();
	
	GridPreviewComponent->SetVisibility(true);

#endif
}

bool AResourceBase::HasEditorResourceGridConflict(const FBuildingGridInfo& ThisGridInfo,
	const USFBuildingGridSubsystem* GridSubsystem) const
{
	if (!ThisGridInfo.IsValid() || !IsValid(GridSubsystem))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld()) return false;

	for (TActorIterator<AResourceBase> It(World); It; ++It)
	{
		const AResourceBase* OtherResource = *It;

		if (!IsValid(OtherResource) ||	OtherResource == this ||
			OtherResource->IsActorBeingDestroyed())
		{
			continue;
		}

		/*
		 * 다른 종류의 자원은 같은 셀에
		 * 함께 존재할 수 있습니다.
		 */
		if (OtherResource->ResourceType != ResourceType)
		{
			continue;
		}

		if (OtherResource->ResourceDeposit <= 0)
		{
			continue;
		}

		const FBuildingGridInfo OtherGridInfo =
			GridSubsystem->WorldCenterToGridInfo(
				OtherResource->GetActorLocation(),
				OtherResource->ResourceGridSize);

		if (!ThisGridInfo.Overlaps(OtherGridInfo))
		{
			continue;
		}

		for (int32 Y = 0; Y < ThisGridInfo.GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < ThisGridInfo.GridSize.X; ++X)
			{
				const int32 ThisCellIndex =
					Y * ThisGridInfo.GridSize.X + X;

				if (CalculateDistributedCellDeposit(
						ResourceDeposit,
						ThisGridInfo.GridSize,
						ThisCellIndex) <= 0)
				{
					continue;
				}

				const FIntPoint CellCoordinate =
					ThisGridInfo.OriginCell + FIntPoint(X, Y);

				const FIntPoint OtherLocalCell =
					CellCoordinate - OtherGridInfo.OriginCell;

				if (OtherLocalCell.X < 0 ||
					OtherLocalCell.X >= OtherGridInfo.GridSize.X ||
					OtherLocalCell.Y < 0 ||
					OtherLocalCell.Y >= OtherGridInfo.GridSize.Y)
				{
					continue;
				}

				const int32 OtherCellIndex =
					OtherLocalCell.Y * OtherGridInfo.GridSize.X +
					OtherLocalCell.X;

				if (CalculateDistributedCellDeposit(
						OtherResource->ResourceDeposit,
						OtherGridInfo.GridSize,
						OtherCellIndex) > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AResourceBase::HasEditorSpawnAnchorGridConflict(const FBuildingGridInfo& ThisGridInfo,
	const USFBuildingGridSubsystem* GridSubsystem) const
{
	if (!ThisGridInfo.IsValid() || !IsValid(GridSubsystem))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return false;
	}

	for (TActorIterator<ASFBuildingSpawnAnchor> It(World); It; ++It)
	{
		const ASFBuildingSpawnAnchor* Anchor = *It;

		if (!IsValid(Anchor) ||	Anchor->IsActorBeingDestroyed())
		{
			continue;
		}

		for (int32 Y = 0; Y < ThisGridInfo.GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < ThisGridInfo.GridSize.X; ++X)
			{
				const int32 ResourceCellIndex =
					Y * ThisGridInfo.GridSize.X + X;

				if (CalculateDistributedCellDeposit(
						ResourceDeposit,
						ThisGridInfo.GridSize,
						ResourceCellIndex) <= 0)
				{
					continue;
				}

				FBuildingGridInfo ResourceCellGridInfo;
				ResourceCellGridInfo.OriginCell =
					ThisGridInfo.OriginCell + FIntPoint(X, Y);
				ResourceCellGridInfo.GridSize = FIntPoint(1, 1);

				if (Anchor->BlocksEditorResourceGrid(
						ResourceCellGridInfo,
						GridSubsystem))
				{
					return true;
				}
			}
		}
	}

	return false;
}
#endif
