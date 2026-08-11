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
	if (!AllTierMeshesSoftArray.TierBuildingState.IsValidIndex(InCurrentTier))
	{
		UE_LOG(LogTemp, Error, TEXT("ASFProductionBuilding::UpdateSpawnableUnitNames: AllTierMeshesSoftArray.TierBuildingState INDEX IS INVALILD"));
		return;
	}
	
	StoredSpawnableUnitDatas = 
	AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].SpawnableUnits;
	OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);
	// for (int i = 0; i <= CurrentTier; ++i)
	// {
	// 	for (const auto& Name : AllTierMeshesSoftArray.TierBuildingState[CurrentTier].SpawnableUnits)
	// 	{	// if tier is down still exist pre timer fname
	// 		StoredSpawnableUnitDatas.AddUnique(Name);
	// 		// need to delete when tier is down
	// 	}
	// }
	
	// debug
	for (const auto& Name : StoredSpawnableUnitDatas)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawnable Units: %s, BuildingTier: %d\n"), *Name.ToString(), CurrentTier);
	}
	
	// USFDataTableManager* DataManager = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	// if (!DataManager) return;
	// for (auto& Name : StoredSpawnableUnitDatas)
	// {
	// 	const FUnitProductData* Row = DataManager->GetDataRow<FUnitProductData>(EDataTableType::Product, Name.UnitRowName);
	// 	Name.UnitProductionCost = Row->UnitProductionCost;
	// 	Name.UnitProductionDuration = Row->UnitProductionDuration;
	// }
}
