#include "SFProductionBuilding.h"

#include "ActorComponent/Product/ProductComponent.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "Datas/Unit/UnitProductData.h"
#include "Game/Subsystem/SFDataTableManager.h"

ASFProductionBuilding::ASFProductionBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ProductComponent = CreateDefaultSubobject<UProductComponent>(TEXT("ProductComponent"));
}

void ASFProductionBuilding::BeginPlay()
{
	Super::BeginPlay();
	
}

void ASFProductionBuilding::OnConstructionFinished()
{
	Super::OnConstructionFinished();
	
	// product component initialize
	UE_LOG(LogTemp, Warning, TEXT("AProductionBuilding::OnConstructionFinished: ProduceComponent Init."));
	UpdateSpawnableUnitNames(CurrentTier);
	
	
}

void ASFProductionBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	
	ProductComponent->AllClearProductLine();
}

void ASFProductionBuilding::InitInfo(const FBuildingAppearance* InData)
{
	Super::InitInfo(InData);
	
	UpdateSpawnableUnitNames(CurrentTier);
	//const TArray<FName> GetSpawnableList = InData->AllTierBuildingStates.TierBuildingState[CurrentTier].SpawnableUnits;
	
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
	ProductData.ConstructCost = BuildCost;
	return ProductData;
}

void ASFProductionBuilding::TryAddUnitProductLine(FName InUnitName)
{
	if (CurrentBuildingState == EBuildingState::Completed)
	{
		ProductComponent->AddProductInLine(InUnitName);
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("Building is not Completed."));
}

const TArray<FName>& ASFProductionBuilding::GetSpawnableUnitNames() const
{
	return StoredSpawnableUnitDatas;
}

void ASFProductionBuilding::UpdateSpawnableUnitNames(int32 InCurrentTier)
{
	StoredSpawnableUnitDatas.Reset();

	if (!CachedTiersInfo.IsValidIndex(InCurrentTier))          // ★FIX
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateSpawnableUnitNames: Invalid Tier %d / %d"),
			InCurrentTier, CachedTiersInfo.Num());             // ★FIX
		OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);
		return;
	}

	StoredSpawnableUnitDatas = CachedTiersInfo[InCurrentTier].SpawnableUnits;   // ★FIX
	OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);
	
	///
	///
	/// buildcost cached by tier
	BuildCost = CachedTiersInfo[InCurrentTier].Cost;
}
