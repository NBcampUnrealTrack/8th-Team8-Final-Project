#include "SFProductionBuilding.h"

#include "ActorComponent/Product/ProductComponent.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "Datas/Unit/UnitProductData.h"
#include "Game/GameState/SFGameState.h"
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
	return ProductData;
}

void ASFProductionBuilding::TryAddUnitProductLine(FName InUnitName)
{
	// 건설완료상태일때만 생산할수있다
	if (CurrentBuildingState == EBuildingState::Completed)
	{
#pragma region CheckResources
		// //TODO: resource check
		// ASFGameState* SFGameState = Cast<ASFGameState>(GetWorld()->GetGameState());
		// if (!IsValid(SFGameState)) return;
		// if (!SFGameState->CheckCanBuildAndSpendCost(unitcost?.mineral, unitcost?.ether))
		// 	return;
#pragma endregion 
		
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
}
