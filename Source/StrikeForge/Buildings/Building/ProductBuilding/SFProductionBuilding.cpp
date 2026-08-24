#include "SFProductionBuilding.h"
#include "ActorComponent/Product/ProductComponent.h"

ASFProductionBuilding::ASFProductionBuilding(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
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
	// 생산열에 있는 모든 product을 지우기만 한다
	// 환불이 필요하면 로직을 여기 작성해야하나
	// 환불이 되지않는것이 자연스럽다
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