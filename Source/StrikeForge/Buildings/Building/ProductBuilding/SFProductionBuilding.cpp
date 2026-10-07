#include "SFProductionBuilding.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/BuildingDefinition/SFProductionBuildingDefinition.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

namespace SFProductionBuildingSpeedModifiers
{
	const FName CommanderSkill(TEXT("CommanderSkill"));
}

ASFProductionBuilding::ASFProductionBuilding(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	ProductComponent = CreateDefaultSubobject<UProductComponent>(TEXT("ProductComponent"));
}

void ASFProductionBuilding::OnTierActivated()
{
	Super::OnTierActivated();

	const USFProductionBuildingDefinition* Definition =	Cast<USFProductionBuildingDefinition>(GetBuildingDefinition());
	if (!IsValid(Definition))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] ProductionBuildingDefinition이 아닙니다. Definition=%s"),
			*GetName(),
			*GetNameSafe(GetBuildingDefinition()));

		return;
	}

	const FSFProductionBuildingTierData* ActivatedTier = Definition->FindProductionTier(GetCurrentTier());
	if (!ActivatedTier)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] Production Tier가 없습니다. Tier=%d"),
			*GetName(),
			GetCurrentTier());

		return;
	}

	StoredSpawnableUnitDatas = ActivatedTier->SpawnableUnits;
	OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);

	ProductComponent->ResumeProductLine();

	// 완성 직후 대기열이 비어 있으면 생산 대기 아이콘을 띄운다.
	RefreshWorkStatus();
}

void ASFProductionBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
	ProductComponent->PauseProductLine();

	// 업그레이드 중에는 쉬는 게 아니므로 대기 아이콘을 내린다.
	RefreshWorkStatus();
}

void ASFProductionBuilding::OnBuildingDestroyed()
{ // deactivated -> destroy
	Super::OnBuildingDestroyed();
	// 생산열에 있는 모든 product을 지우기만 한다
	// 환불이 필요하면 로직을 여기 작성해야하나
	// 환불이 되지않는것이 자연스럽다
	ProductComponent->AllClearProductLine();

}

void ASFProductionBuilding::ProductUnits_Implementation(FName InUnitName)
{
	if (!USFTutorialSubsystem::IsUnitProductionAllowed(this, this, InUnitName))
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] 현재 튜토리얼 임무에서는 광전사만 생산할 수 있습니다."), *GetName());
		return;
	}

	TryAddUnitProductLine(InUnitName);
}

FProductableData ASFProductionBuilding::GetProductableData_Implementation()
{
	FProductableData ProductData;
	ProductData.ProductableUnitFNames = StoredSpawnableUnitDatas;
	ProductData.CurrentProductingFNames = ProductComponent->GetProductLine();
	ProductData.ProductComponent = ProductComponent;
	ProductData.UpgradeCost = GetUpgradeCost();
	return ProductData;
}

void ASFProductionBuilding::TryAddUnitProductLine(FName InUnitName)
{
	if (CurrentBuildingState != EBuildingState::Completed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 완성되지 않은 건물은 유닛을 생산할 수 없습니다."), *GetName());
		return;
	}

	ProductComponent->AddProductInLine(InUnitName);
}

FSFSelectableInfo ASFProductionBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();

	Info.SelectableType = ESFSelectableType::ProductionBuilding;
	return Info;
}

void ASFProductionBuilding::SetAssembleVectorPoint(const FVector& InAssemblePoint)
{
	ProductComponent->SetAssemblePoint(InAssemblePoint);
}

void ASFProductionBuilding::ApplySpeedBuff(float InMultiflyValue)
{
	SetProductionSpeedModifier(
		SFProductionBuildingSpeedModifiers::CommanderSkill,
		InMultiflyValue);
}

void ASFProductionBuilding::ResetSpeedMultiplier()
{
	RemoveProductionSpeedModifier(
		SFProductionBuildingSpeedModifiers::CommanderSkill);
}

void ASFProductionBuilding::SetProductionSpeedModifier(
	FName ModifierId,
	float InMultiplier)
{
	if (IsValid(ProductComponent))
	{
		ProductComponent->SetSpeedModifier(ModifierId, InMultiplier);
	}
}

void ASFProductionBuilding::RemoveProductionSpeedModifier(
	FName ModifierId)
{
	if (IsValid(ProductComponent))
	{
		ProductComponent->RemoveSpeedModifier(ModifierId);
	}
}

void ASFProductionBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	// 부모 BeginPlay는 로딩 실패 시 Destroy()를 호출한다.
	if (IsActorBeingDestroyed())	return;
	if (!IsValid(ProductComponent))	return;

	// 진행도는 건물을 거치지 않고 상태 컴포넌트가 직접 받는다.
	ProductComponent->OnProductRatioChanged.AddUniqueDynamic(
		StatusComponent, &USFOverheadStatusComponent::HandleProgressRatio);
	ProductComponent->OnProductLineChanged.AddUniqueDynamic(this, &ThisClass::HandleProductLineChanged);

	// 현재 대기열 상태로 표시 여부를 한 번 맞춘다.
	HandleProductLineChanged(ProductComponent->GetProductLine());
}

void ASFProductionBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ProductComponent))
	{
		ProductComponent->OnProductRatioChanged.RemoveDynamic(
			StatusComponent, &USFOverheadStatusComponent::HandleProgressRatio);
		ProductComponent->OnProductLineChanged.RemoveDynamic(this, &ThisClass::HandleProductLineChanged);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ASFProductionBuilding::OnBuildingDefinitionLoaded()
{
	Super::OnBuildingDefinitionLoaded();

	const USFProductionBuildingDefinition* Definition =
		Cast<USFProductionBuildingDefinition>(GetBuildingDefinition());

	if (!IsValid(Definition))	return;


	StatusComponent->SetProgressColor(SFOverheadUIColors::Production);
}

bool ASFProductionBuilding::HasRallyPoint() const
{
	return IsValid(ProductComponent) &&
		ProductComponent->HasAssemblePoint();
}

FVector ASFProductionBuilding::GetRallyPoint() const
{
	if (!IsValid(ProductComponent))
	{
		return FVector::ZeroVector;
	}

	return ProductComponent->GetAssemblePoint();
}

void ASFProductionBuilding::HandleProductLineChanged(TArray<FProductInfo> InProductLine)
{
	RefreshWorkStatus();
}

void ASFProductionBuilding::RefreshWorkStatus()
{
	// 내 건물만 보여준다. 적 생산 상황은 노출하지 않는다.
	const bool bOwned = IsOwnedByLocalPlayer();
	const bool bHasProducts = IsValid(ProductComponent) && ProductComponent->bIsHasProducts();

	StatusComponent->SetElementVisible(ESFOverheadElement::Progress, bOwned && bHasProducts);

	// 완성돼 기능이 켜진 상태에서 대기열이 비었을 때만 쉬는 중이다. 건설·업그레이드 중은 제외한다.
	const bool bIdle = bOwned && IsTierActivated() && !bHasProducts;

	StatusComponent->SetStatusIcon(bIdle
		? ESFOverheadStatusIcon::ProductionIdle
		: ESFOverheadStatusIcon::None);
}
