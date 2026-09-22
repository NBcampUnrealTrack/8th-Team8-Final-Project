#include "SFBuildingSpawnAnchor.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"

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
	GridPreviewComponent->SetLineThickness(3.0f);
#endif
}

#if WITH_EDITOR
void ASFBuildingSpawnAnchor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	RefreshEditorPreview();
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

	if (PreviewDefinition.IsNull())
	{
		RefreshEditorPreview();
		return;
	}

	USFBuildingDefinition* Definition =
		PreviewDefinition.LoadSynchronous();
	if (!IsValid(Definition))
	{
		RefreshEditorPreview();
		return;
	}

	const FIntPoint GridSize =	Definition->GridSize;

	if (GridSize.X <= 0 ||	GridSize.Y <= 0)
	{
		RefreshEditorPreview();
		return;
	}

	USFBuildingGridSubsystem* GridSubsystem =
		World->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem))
	{
		RefreshEditorPreview();
		return;
	}

	const FVector CurrentLocation =	GetActorLocation();

	/*
	 * 현재 월드 중심을 기준으로
	 * 이 건물이 차지할 격자 정보를 계산한다.
	 */
	const FBuildingGridInfo EditorGridInfo =
		GridSubsystem->WorldCenterToGridInfo(
			CurrentLocation, GridSize);
	if (!EditorGridInfo.IsValid())
	{
		RefreshEditorPreview();
		return;
	}

	/*
	 * 계산된 격자의 정확한 월드 중심을 구한다.
	 */
	FVector SnappedLocation =
		GridSubsystem->GridInfoToWorldCenter(EditorGridInfo);

	/*
	 * 격자 시스템은 XY 위치만 담당한다.
	 * 디자이너가 지정한 지면 높이는 그대로 유지한다.
	 */
	SnappedLocation.Z =	CurrentLocation.Z;

	if (!CurrentLocation.Equals(
			SnappedLocation, KINDA_SMALL_NUMBER))
	{
		SetActorLocation(
			SnappedLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	RefreshEditorPreview();
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

	const FIntPoint GridSize = Definition->GridSize;

	if (!IsValid(GridSubsystem) ||
		GridSize.X <= 0 || GridSize.Y <= 0)
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

	/*
	 * 겹침 판정은 이후 단계에서 추가한다.
	 * 지금은 유효한 격자를 초록색으로 표시한다.
	 */
	GridPreviewComponent->ShapeColor = FColor::Green;

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
	
	RefreshEditorGridPreview(Definition);
#endif
}
#endif
