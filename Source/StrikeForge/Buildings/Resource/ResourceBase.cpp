#include "ResourceBase.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Logging/SFLog.h"
#if WITH_EDITOR
#include "EngineUtils.h"
#endif
AResourceBase::AResourceBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
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
	GridPreviewComponent->SetLineThickness(3.0f);

#endif
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

	const int32 BaseDeposit =
		ResourceDeposit / CellCount;

	const int32 Remainder =
		ResourceDeposit % CellCount;

	CellDeposits.Init(BaseDeposit, CellCount);

	/*
	 * 나누어떨어지지 않은 나머지는
	 * 앞쪽 셀부터 1씩 배분합니다.
	 */
	for (int32 Index = 0; Index < Remainder; ++Index)
	{
		CellDeposits[Index] += 1;
	}
	
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
	
	RegisteredResourceGridInfo = GridSubsystem->WorldCenterToGridInfo(
		GetActorLocation(),
		ResourceGridSize);
	
	FVector WorldCenterVector = GridSubsystem->GridInfoToWorldCenter(RegisteredResourceGridInfo);
	/*
	 * 격자는 XY만 정렬합니다.
	 * 레벨에서 지정한 자원의 높이는 유지합니다.
	 */
	WorldCenterVector.Z = GetActorLocation().Z;
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
	RefreshAllEditorResourceGridPreviews();
	
}

void AResourceBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	/*
	드래그하는 동안 매번 격자 중심으로 이동시키면
	액터가 마우스를 따라가지 않고 잘 배치가 안됀다.
	엑터의 Pivot은 격자이동하지않고 움직이는데
	정작 엑터는 격자이동을하고 Pivot을 움직여도 엑터는 가만히있는현상이 있다
	
	bFinished 로 조건을 추가한
	현재 방식은 자유롭게 이동한 뒤 마우스를 놓을 때 한 번만 정렬합니다.
	 */

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

	if (ResourceGridSize.X <= 0 ||
		ResourceGridSize.Y <= 0)
	{
		return;
	}

	const FBuildingGridInfo EditorGridInfo =
		GridSubsystem->WorldCenterToGridInfo(
			GetActorLocation(),
			ResourceGridSize);

	if (!EditorGridInfo.IsValid())
	{
		return;
	}

	FVector SnappedLocation =
		GridSubsystem->GridInfoToWorldCenter(
			EditorGridInfo);

	/*
	 * 격자는 XY만 담당합니다.
	 * 지형 높이나 디자이너가 지정한 Z값은 유지합니다.
	 */
	SnappedLocation.Z = GetActorLocation().Z;

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
	
	// color
	/*
	 * 하나의 자원이 이동하면 주변 자원의
	 * 충돌 상태도 함께 달라질 수 있습니다.
	 */
	RefreshAllEditorResourceGridPreviews();
}

void AResourceBase::PostEditUndo()
{
	Super::PostEditUndo();
	/*
	 * 위치 이동, 삭제 복구 등의 Undo/Redo가 발생하면
	 * 다른 자원들의 충돌 상태도 함께 달라질 수 있습니다.
	 */
	RefreshAllEditorResourceGridPreviews();
}

void AResourceBase::Destroyed()
{
	/*
	 * 이 액터가 제거되기 전에 남아 있는 자원들의
	 * 프리뷰 색상을 다시 계산합니다.
	 *
	 * 이 시점에는 IsActorBeingDestroyed()가 true이므로
	 * 충돌 검사에서 이 자원을 제외해야 합니다.
	 */
	RefreshAllEditorResourceGridPreviews();
	
	Super::Destroyed();
}

void AResourceBase::RefreshAllEditorResourceGridPreviews()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld()) return;

	for (TActorIterator<AResourceBase> It(World); It; ++It)
	{
		AResourceBase* Resource = *It;

		if (!IsValid(Resource) || Resource->IsActorBeingDestroyed())
		{
			continue;
		}

		Resource->RefreshEditorGridPreview();
	}
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

	const bool bHasConflict =
		HasEditorResourceGridConflict(
			EditorGridInfo,	GridSubsystem);

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

		const FBuildingGridInfo OtherGridInfo =
			GridSubsystem->WorldCenterToGridInfo(
				OtherResource->GetActorLocation(),
				OtherResource->ResourceGridSize);

		if (ThisGridInfo.Overlaps(OtherGridInfo))
		{
			return true;
		}
	}

	return false;
}
#endif
