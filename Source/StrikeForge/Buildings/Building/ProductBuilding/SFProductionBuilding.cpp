#include "SFProductionBuilding.h"

#include "ActorComponent/Product/ProductComponent.h"
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
	GetSpawnableUnitNames(CurrentTier);
	
	
}

void ASFProductionBuilding::InitInfo(const FBuildingAppearance* InData)
{
	Super::InitInfo(InData);
	
	GetSpawnableUnitNames(CurrentTier);
	//const TArray<FName> GetSpawnableList = InData->AllTierBuildingStates.TierBuildingState[CurrentTier].SpawnableUnits;
	
}

void ASFProductionBuilding::ProductUnits_Implementation(FName InUnitName)
{
	TryAddUnitProductLine(InUnitName);
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

void ASFProductionBuilding::GetSpawnableUnitNames(int32 InCurrentTier)
{
	for (int i = 0; i <= CurrentTier; ++i)
	{
		for (const auto& Name : AllTierMeshesSoftArray.TierBuildingState[CurrentTier].SpawnableUnits)
		{	// if tier is down still exist pre timer fname
			StoredSpawnableUnitDatas.AddUnique(Name);
			// need to delete when tier is down
		}
	}
	
	// debug
	for (const auto& Name : StoredSpawnableUnitDatas)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawnable Units: %s, BuildingTier: %d"), *Name.ToString(), CurrentTier);
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
