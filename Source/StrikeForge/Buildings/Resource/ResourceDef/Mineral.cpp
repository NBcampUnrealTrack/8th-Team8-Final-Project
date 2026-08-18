#include "Mineral.h"

#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

AMineral::AMineral()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ResourceType = EResourceType::Mineral;
	ResourceDeposit = 500;
}

bool AMineral::Digging(const int32 InDigAmount)
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return false;
	
	if (ResourceDeposit < InDigAmount) return false;
	
	ResourceDeposit -= InDigAmount;
	SF_LOG(Warning, "Left Deposit Mineral: %d", ResourceDeposit);
	SFGameState->SetMineral(SFGameState->GetMineral() + InDigAmount);
	return true;
}

void AMineral::BeginPlay()
{
	Super::BeginPlay();
	
	//SF_LOG(Warning, "%d", ResourceType);
}
