#include "Mineral.h"

AMineral::AMineral()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ResourceType = EResourceType::Mineral;
	ResourceDeposit = 500;
	ResourceTier = 1;
}