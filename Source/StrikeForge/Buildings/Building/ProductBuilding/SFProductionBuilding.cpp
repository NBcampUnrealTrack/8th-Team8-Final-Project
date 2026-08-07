#include "SFProductionBuilding.h"

ASFProductionBuilding::ASFProductionBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
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
}
