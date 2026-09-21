#include "ResourceBase.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Logging/SFLog.h"

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

	BoxComponent->SetVisibility(true);
	BoxComponent->SetLineThickness(2.f);
	BoxComponent->SetHiddenInGame(false);
	
	BoxComponent->SetCollisionProfileName("SpawnedBuilding");
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
	
	const FVector WorldCenterVector = GridSubsystem->GridInfoToWorldCenter(RegisteredResourceGridInfo);
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
