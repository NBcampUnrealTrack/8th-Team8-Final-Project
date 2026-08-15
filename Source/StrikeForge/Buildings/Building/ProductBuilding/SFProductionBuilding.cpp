#include "SFProductionBuilding.h"
#include "ActorComponent/Product/ProductComponent.h"

ASFProductionBuilding::ASFProductionBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	ProductComponent = CreateDefaultSubobject<UProductComponent>(TEXT("ProductComponent"));
}

void ASFProductionBuilding::OnTierActivated(const FBuildingTier& ActivatedTier)
{
	Super::OnTierActivated(ActivatedTier);

	StoredSpawnableUnitDatas = ActivatedTier.SpawnableUnits;
	OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);
	ProductComponent->ResumeProductLine();
}

void ASFProductionBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
	ProductComponent->PauseProductLine();
}

void ASFProductionBuilding::OnTierDeactivated(const FBuildingTier& DeactivatedTier)
{
	Super::OnTierDeactivated(DeactivatedTier);
	
}

void ASFProductionBuilding::OnBuildingDestroyed()
{ // deactivated -> destroy
	Super::OnBuildingDestroyed();
	ProductComponent->AllClearProductLine();
}
void ASFProductionBuilding::ProductUnits_Implementation(FName InUnitName)
{
	TryAddUnitProductLine(InUnitName);
}

FProductableData ASFProductionBuilding::GetProductableData_Implementation()
{
	FProductableData ProductData;
	ProductData.ProductableUnitFNames = StoredSpawnableUnitDatas;
	ProductData.CurrentProductingFNames = ProductComponent->GetProductLine();
	ProductData.ProductComponent = ProductComponent;
	ProductData.UpgradeCost = CurrentUpgradeBuildCost;
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