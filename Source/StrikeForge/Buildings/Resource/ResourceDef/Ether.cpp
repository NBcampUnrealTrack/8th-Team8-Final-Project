#include "Ether.h"

#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

AEther::AEther()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ResourceType = EResourceType::Ether;
	ResourceDeposit = 300;
}

bool AEther::Digging(const int32 InDigAmount)
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return false;
	
	if (ResourceDeposit < InDigAmount) return false;
	
	ResourceDeposit -= InDigAmount;
	SF_LOG(Warning, "Left Deposit Ether: %d", ResourceDeposit);
	SFGameState->SetEther(SFGameState->GetEther() + InDigAmount);
	return true;
}

void AEther::BeginPlay()
{
	Super::BeginPlay();
	
	//SF_LOG(Warning, "%d", ResourceType);
	
}


