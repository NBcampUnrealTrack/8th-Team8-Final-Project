#include "SFBuildingSpawnAnchor.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Datas/Building/BuildingDefinition/SFMiningBuildingDefinition.h"
#if WITH_EDITOR
#include "EngineUtils.h"
#endif
ASFBuildingSpawnAnchor::ASFBuildingSpawnAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	
#if WITH_EDITORONLY_DATA
	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PreviewMeshComponent");
	PreviewMeshComponent->SetupAttachment(GetRootComponent());
	
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	PreviewMeshComponent->SetCanEverAffectNavigation(false);
	// 에디터 Viewport에서는 보이고 PIE에서는 보이지 않는다.
	PreviewMeshComponent->SetHiddenInGame(true);
	
	GridPreviewComponent =
		CreateDefaultSubobject<UBoxComponent>(
			TEXT("GridPreviewComponent"));

	GridPreviewComponent->SetupAttachment(GetRootComponent());
	GridPreviewComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	GridPreviewComponent->SetGenerateOverlapEvents(false);
	GridPreviewComponent->SetCanEverAffectNavigation(false);
	GridPreviewComponent->SetHiddenInGame(true);
	GridPreviewComponent->SetVisibility(false);

	GridPreviewComponent->ShapeColor = FColor::Green;
	GridPreviewComponent->bDrawOnlyIfSelected = false;
	GridPreviewComponent->SetLineThickness(5.0f);
#endif
}

#if WITH_EDITOR
void ASFBuildingSpawnAnchor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (UWorld* World = GetWorld())
	{
		if (USFBuildingGridSubsystem* GridSubsystem =
				World->GetSubsystem<USFBuildingGridSubsystem>())
		{
			GridSubsystem->RefreshAllEditorGridPreviews();
		}
	}
}

void ASFBuildingSpawnAnchor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	/*
	 * 드래그 도중에는 자유롭게 이동시킨다.
	 * 마우스를 놓은 순간에만 격자 중심으로 정렬한다.
	 */
	if (!bFinished)	return;

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

	if (PreviewDefinition.IsNull())
	{
		GridSubsystem->RefreshAllEditorGridPreviews();
		return;
	}

	USFBuildingDefinition* Definition =
		PreviewDefinition.LoadSynchronous();
	if (!IsValid(Definition))
	{
		GridSubsystem->RefreshAllEditorGridPreviews();
		return;
	}

	// Definition에는 로컬 크기를 유지하고 실제 스냅에는 Anchor 회전을 반영합니다.
	const FIntPoint EffectiveGridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			Definition->GridSize, GetActorRotation());

	const FVector CurrentLocation =	GetActorLocation();
	FBuildingGridInfo EditorGridInfo;
	FVector SnappedLocation;

	if (!GridSubsystem->TrySnapWorldCenterToGrid(
			CurrentLocation,
			EffectiveGridSize,
			EditorGridInfo,
			SnappedLocation))
	{
		GridSubsystem->RefreshAllEditorGridPreviews();
		return;
	}

	if (!CurrentLocation.Equals(
			SnappedLocation, KINDA_SMALL_NUMBER))
	{
		SetActorLocation(
			SnappedLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	GridSubsystem->RefreshAllEditorGridPreviews();
}

void ASFBuildingSpawnAnchor::PostEditUndo()
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

void ASFBuildingSpawnAnchor::Destroyed()
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

bool ASFBuildingSpawnAnchor::TryGetBuildingMeshRelativeTransform(
	const USFBuildingDefinition* Definition,
	FTransform& OutRelativeTransform) const
{
	OutRelativeTransform = FTransform::Identity;

	if (!IsValid(Definition) ||	Definition->BuildingClass.IsNull())
	{
		return false;
	}

	/*
	 * ASFBaseBuilding::StaticClass()를 사용하면
	 * C++ 부모 클래스의 기본값만 가져오게 된다.
	 *
	 * Definition에 지정된 실제 BP 클래스를 불러와야
	 * 해당 BP에서 변경한 위치·회전·Scale을 가져올 수 있다.
	 */
	UClass* BuildingClass =
		Definition->BuildingClass.LoadSynchronous();
	if (!IsValid(BuildingClass)) return false;

	const ASFBaseBuilding* BuildingCDO =
		BuildingClass->GetDefaultObject<ASFBaseBuilding>();
	if (!IsValid(BuildingCDO)) return false;

	const UStaticMeshComponent* BuildingMeshComponent =
		BuildingCDO->FindComponentByClass<UStaticMeshComponent>();
	if (!IsValid(BuildingMeshComponent))return false;
	
	OutRelativeTransform =
		BuildingMeshComponent->GetRelativeTransform();

	return true;
}

bool ASFBuildingSpawnAnchor::HasEditorSpawnAnchorGridConflict(const FBuildingGridInfo& ThisGridInfo,
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
		const ASFBuildingSpawnAnchor* OtherAnchor = *It;

		if (!IsValid(OtherAnchor) ||
			OtherAnchor == this ||
			OtherAnchor->IsActorBeingDestroyed() ||
			OtherAnchor->PreviewDefinition.IsNull())
		{
			continue;
		}

		const USFBuildingDefinition* OtherDefinition =
			OtherAnchor->PreviewDefinition.LoadSynchronous();
		if (!IsValid(OtherDefinition))	continue;

		// 다른 Anchor도 자신의 회전을 반영한 월드 점유 크기로 비교합니다.
		const FIntPoint OtherEffectiveGridSize =
			USFBuildingGridSubsystem::GetRotatedGridSize(
				OtherDefinition->GridSize,
				OtherAnchor->GetActorRotation());

		const FBuildingGridInfo OtherGridInfo =
			GridSubsystem->WorldCenterToGridInfo(
				OtherAnchor->GetActorLocation(),
				OtherEffectiveGridSize);

		if (ThisGridInfo.Overlaps(OtherGridInfo))
		{
			return true;
		}
	}

	return false;
}

bool ASFBuildingSpawnAnchor::HasEditorResourceGridConflict(const USFBuildingDefinition* Definition,
	const FBuildingGridInfo& ThisGridInfo, const USFBuildingGridSubsystem* GridSubsystem) const
{
	if (!IsValid(Definition) || !ThisGridInfo.IsValid() ||
	   !IsValid(GridSubsystem))
	{
		return false;
	}

	/*
	 * 광산은 자원 셀과 격자를 공유할 수 있습니다.
	 * 자원 종류와 검색 영역은 HasUsableEditorMiningResource()에서 검사합니다.
	 */
	if (Cast<USFMiningBuildingDefinition>(Definition))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return false;
	}

	for (TActorIterator<AResourceBase> It(World); It; ++It)
	{
		const AResourceBase* Resource = *It;

		if (!IsValid(Resource) ||
			Resource->IsActorBeingDestroyed() ||
			Resource->IsDepleted())
		{
			continue;
		}

		const FIntPoint& ResourceGridSize =
			Resource->GetResourceGridSize();

		if (ResourceGridSize.X <= 0 ||
			ResourceGridSize.Y <= 0)
		{
			continue;
		}

		const FBuildingGridInfo ResourceGridInfo =
			GridSubsystem->WorldCenterToGridInfo(
				Resource->GetActorLocation(),
				ResourceGridSize);

		if (!ThisGridInfo.Overlaps(ResourceGridInfo))
		{
			continue;
		}

		for (int32 Y = 0; Y < ResourceGridInfo.GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < ResourceGridInfo.GridSize.X; ++X)
			{
				const int32 ResourceCellIndex =
					Y * ResourceGridInfo.GridSize.X + X;

				if (AResourceBase::CalculateDistributedCellDeposit(
						Resource->GetResourceDeposit(),
						ResourceGridInfo.GridSize,
						ResourceCellIndex) <= 0)
				{
					continue;
				}

				FBuildingGridInfo ResourceCellGridInfo;
				ResourceCellGridInfo.OriginCell =
					ResourceGridInfo.OriginCell + FIntPoint(X, Y);
				ResourceCellGridInfo.GridSize = FIntPoint(1, 1);

				if (ThisGridInfo.Overlaps(ResourceCellGridInfo))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool ASFBuildingSpawnAnchor::HasUsableEditorMiningResource(const USFMiningBuildingDefinition* MiningDefinition,
	const FBuildingGridInfo& BuildingGridInfo, const USFBuildingGridSubsystem* GridSubsystem) const
{
	if (!IsValid(MiningDefinition) || !BuildingGridInfo.IsValid() ||
		!IsValid(GridSubsystem))
	{
		return false;
	}

	// 검색 격자도 Definition의 로컬 크기이므로
	// SpawnAnchor의 회전을 반영한 월드 크기로 변환합니다.
	const FIntPoint EffectiveSearchGridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			MiningDefinition->ResourceSearchGridSize,
			GetActorRotation());

	FBuildingGridInfo SearchGridInfo;

	if (!GridSubsystem->TryMakeCenteredSubGrid(
			BuildingGridInfo,
			EffectiveSearchGridSize,SearchGridInfo))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->IsGameWorld())
	{
		return false;
	}

	const FIntPoint SearchMaxExclusive =
		SearchGridInfo.OriginCell + SearchGridInfo.GridSize;

	for (TActorIterator<AResourceBase> It(World); It; ++It)
	{
		const AResourceBase* Resource = *It;

		if (!IsValid(Resource) ||
			Resource->IsActorBeingDestroyed() ||
			Resource->GetResourceType() !=
				MiningDefinition->CanDigResourceType)
		{
			continue;
		}

		const FIntPoint& ResourceGridSize =	Resource->GetResourceGridSize();

		if (ResourceGridSize.X <= 0 ||	ResourceGridSize.Y <= 0)
		{
			continue;
		}

		const int32 TotalDeposit =
			FMath::Max(
				0,Resource->GetResourceDeposit());

		if (TotalDeposit <= 0)
		{
			continue;
		}

		const FBuildingGridInfo ResourceGridInfo =
			GridSubsystem->WorldCenterToGridInfo(
				Resource->GetActorLocation(),
				ResourceGridSize);

		// 검색 영역과 전혀 겹치지 않으면
		// 개별 자원 셀을 검사할 필요가 없습니다.
		if (!SearchGridInfo.Overlaps(ResourceGridInfo))
		{
			continue;
		}

		for (int32 Y = 0; Y < ResourceGridSize.Y; ++Y)
		{
			for (int32 X = 0; X < ResourceGridSize.X; ++X)
			{
				const FIntPoint CellCoordinate =
					ResourceGridInfo.OriginCell +
					FIntPoint(X, Y);

				const bool bInsideSearchGrid =
					CellCoordinate.X >=	SearchGridInfo.OriginCell.X &&
					CellCoordinate.X <	SearchMaxExclusive.X &&
					CellCoordinate.Y >=	SearchGridInfo.OriginCell.Y &&
					CellCoordinate.Y <	SearchMaxExclusive.Y;

				if (!bInsideSearchGrid)
				{
					continue;
				}

				const int32 ResourceCellIndex =
					Y * ResourceGridSize.X + X;

				const int32 CellDeposit =
					AResourceBase::CalculateDistributedCellDeposit(
						TotalDeposit,
						ResourceGridSize,
						ResourceCellIndex);

				if (CellDeposit > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void ASFBuildingSpawnAnchor::ClearEditorPreview()
{
#if WITH_EDITORONLY_DATA
	if (IsValid(PreviewMeshComponent))
	{
		PreviewMeshComponent->SetStaticMesh(nullptr);
		PreviewMeshComponent->SetRelativeTransform(
			FTransform::Identity);
		PreviewMeshComponent->SetVisibility(false);
	}

	if (IsValid(GridPreviewComponent))
	{
		GridPreviewComponent->SetRelativeTransform(
			FTransform::Identity);
		GridPreviewComponent->SetVisibility(false);
	}
#endif
}

bool ASFBuildingSpawnAnchor::BlocksEditorResourceGrid(const FBuildingGridInfo& ResourceGridInfo,
	const USFBuildingGridSubsystem* GridSubsystem) const
{
	if (!ResourceGridInfo.IsValid() || !IsValid(GridSubsystem) ||
		IsActorBeingDestroyed() || PreviewDefinition.IsNull())
	{
		return false;
	}

	const USFBuildingDefinition* Definition =
		PreviewDefinition.LoadSynchronous();

	if (!IsValid(Definition))
	{
		return false;
	}

	/*
	 * 광산은 자원 격자와 겹칠 수 있습니다.
	 * 광산의 자원 종류와 검색 범위는 광산 프리뷰에서 검사합니다.
	 */
	if (Cast<USFMiningBuildingDefinition>(Definition))
	{
		return false;
	}

	const FIntPoint EffectiveGridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			Definition->GridSize, GetActorRotation());

	if (EffectiveGridSize.X <= 0 ||	EffectiveGridSize.Y <= 0)
	{
		return false;
	}

	const FBuildingGridInfo AnchorGridInfo =
		GridSubsystem->WorldCenterToGridInfo(
			GetActorLocation(),	EffectiveGridSize);

	return AnchorGridInfo.IsValid() &&
		AnchorGridInfo.Overlaps(ResourceGridInfo);
}

void ASFBuildingSpawnAnchor::RefreshEditorGridPreview(const USFBuildingDefinition* Definition)
{
#if WITH_EDITORONLY_DATA
	if (!IsValid(GridPreviewComponent))
	{
		return;
	}

	/*
	 * 이 함수가 중간에 실패하더라도
	 * 이전 크기의 박스가 남지 않게 먼저 숨긴다.
	 */
	GridPreviewComponent->SetVisibility(false);

	if (!IsValid(Definition))
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

	// 프리뷰 박스는 로컬 GridSize를 사용하고 Anchor의 회전을 그대로 따라갑니다.
	// 실제 격자 스냅과 등록에서만 회전된 EffectiveGridSize를 사용합니다.
	const FIntPoint GridSize = Definition->GridSize;

	if (!IsValid(GridSubsystem) ||
		GridSize.X <= 0 ||
		GridSize.Y <= 0)
	{
		return;
	}
	
	// 박스 표시는 로컬 크기를 사용하지만 충돌 판정은 월드 점유 크기를 사용합니다.
	const FIntPoint EffectiveGridSize =
		USFBuildingGridSubsystem::GetRotatedGridSize(
			GridSize,
			GetActorRotation());

	const FBuildingGridInfo EditorGridInfo =
		GridSubsystem->WorldCenterToGridInfo(
			GetActorLocation(),
			EffectiveGridSize);

	if (!EditorGridInfo.IsValid())
	{
		return;
	}
	
	const float CellSize = GridSubsystem->GetCellSize();
	if (CellSize <= 0.0f) return;

	/*
	 * SetBoxExtent는 박스 전체 크기가 아니라
	 * 중심부터 끝까지의 반지름, 즉 절반 크기를 받는다.
	 */
	const FVector PreviewExtent(
		GridSize.X * CellSize * 0.5f,
		GridSize.Y * CellSize * 0.5f,
		5.0f);

	GridPreviewComponent->SetBoxExtent(
		PreviewExtent,false);

	/*
	 * 박스 높이는 총 10이고 중심을 Z=5에 둔다.
	 * 따라서 박스 아래쪽이 Anchor의 Z=0에 놓인다.
	 */
	GridPreviewComponent->SetRelativeLocation(
		FVector(0.0f, 0.0f, 5.0f));

	GridPreviewComponent->SetRelativeRotation(
		FRotator::ZeroRotator);

	GridPreviewComponent->SetRelativeScale3D(
		FVector::OneVector);

	const bool bHasAnchorConflict =
	HasEditorSpawnAnchorGridConflict(
		EditorGridInfo,
		GridSubsystem);

	const bool bHasResourceConflict =
		HasEditorResourceGridConflict(
			Definition,
			EditorGridInfo,
			GridSubsystem);

	const USFMiningBuildingDefinition* MiningDefinition =
		Cast<USFMiningBuildingDefinition>(Definition);

	const bool bMissingMiningResource =
		IsValid(MiningDefinition) &&
		!HasUsableEditorMiningResource(
			MiningDefinition,
			EditorGridInfo,
			GridSubsystem);

	const bool bHasConflict =
		bHasAnchorConflict ||
		bHasResourceConflict ||
		bMissingMiningResource;

	GridPreviewComponent->ShapeColor =
		bHasConflict
			? FColor::Red
			: FColor::Green;

	GridPreviewComponent->MarkRenderStateDirty();
	GridPreviewComponent->SetVisibility(true);
#endif
}

void ASFBuildingSpawnAnchor::RefreshEditorPreview()
{
#if WITH_EDITORONLY_DATA
	ClearEditorPreview();

	if (!IsValid(PreviewMeshComponent) ||
		!IsValid(GridPreviewComponent))
	{
		return;
	}

	// Definition이 지정되지 않은 것은 정상적인 빈 상태다.
	if (PreviewDefinition.IsNull())
	{
		return;
	}

	USFBuildingDefinition* Definition =
		PreviewDefinition.LoadSynchronous();

	// 지정된 에셋을 불러올 수 없다면 잘못된 참조를 제거한다.
	if (!IsValid(Definition))
	{
		PreviewDefinition.Reset();
		return;
	}

	const FSFBuildingTierData* PreviewTier =
		Definition->FindTier(InitialTierIndex);

	// 지정한 Tier가 없다면 사용할 수 없는 Definition 상태다.
	if (PreviewTier == nullptr)
	{
		PreviewDefinition.Reset();
		return;
	}

	// 격자 프리뷰는 메시 데이터와 독립적으로 갱신합니다.
	RefreshEditorGridPreview(Definition);

	UStaticMesh* CompletedMesh =
		PreviewTier->Visual.CompletedMesh.LoadSynchronous();

	/*
	 * Tier 자체는 유효하므로 Definition은 유지한다.
	 * CompletedMesh만 누락된 경우에는 DataAsset을 수정할 수 있어야 한다.
	 */
	if (!IsValid(CompletedMesh))
	{
		return;
	}

	FTransform BuildingMeshRelativeTransform;

	if (!TryGetBuildingMeshRelativeTransform(
			Definition,	BuildingMeshRelativeTransform))
	{
		return;
	}

	PreviewMeshComponent->SetStaticMesh(CompletedMesh);
	/*
	 * Actor 자체의 Scale은 변경하지 않는다.
	 * 메시 컴포넌트에만 건물 BP의 상대 Transform을 적용한다.
	 */
	PreviewMeshComponent->SetRelativeTransform(BuildingMeshRelativeTransform);
	PreviewMeshComponent->SetVisibility(true);
#endif
}
#endif
