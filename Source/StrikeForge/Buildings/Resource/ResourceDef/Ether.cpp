#include "Ether.h"

AEther::AEther()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ResourceType = EResourceType::Ether;
	ResourceDeposit = 300;
	ResourceTier = 1;
}