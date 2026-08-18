#include "SFMiningBuilding.h"

#include "Buildings/Resource/Interface/Minable.h"
#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

ASFMiningBuilding::ASFMiningBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	
}

FSFSelectableInfo ASFMiningBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();
	Info.SelectableType = ESFSelectableType::NormalBuilding;
	return Info;
}

void ASFMiningBuilding::OnTierActivated(const FBuildingTier& ActivatedTier)
{
	Super::OnTierActivated(ActivatedTier);
	
	StopMining();
	
	DiggingIncomeRate = DiggingAmount / DiggingInterval;
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	SFGameState->AddMineralIncomeRate(DiggingIncomeRate);
	StartMining();
}

void ASFMiningBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
	
	// ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	// if (!IsValid(SFGameState)) return;
	// SFGameState->AddMineralIncomeRate(MiningIncomeRate * -1);
	StopMining();
}

void ASFMiningBuilding::OnTierDeactivated(const FBuildingTier& DeactivatedTier)
{
	Super::OnTierDeactivated(DeactivatedTier);
	
	StopMining();
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMineralIncomeRate(DiggingIncomeRate * -1);
	
}

void ASFMiningBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	

}

void ASFMiningBuilding::StartMining()
{
	GetWorldTimerManager().SetTimer(
		DiggingProductTimerHandle,
		this,
		&ThisClass::Mining,
		DiggingInterval,
		true);
}

void ASFMiningBuilding::StopMining()
{
	GetWorldTimerManager().ClearTimer(DiggingProductTimerHandle);
}

void ASFMiningBuilding::PauseMining()
{

}

void ASFMiningBuilding::UnPauseMining()
{
}

void ASFMiningBuilding::Mining()
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	if (IMinable* Minable =	Cast<IMinable>(LinkedResource.Get()))
	{
		if (Minable->Digging(DiggingAmount))
		{
			SF_LOG(Warning, "Mine_Digging!");
			return;
		}
		SF_LOG(Warning, "Mine_Dig_Failed...");
	}
}

void ASFMiningBuilding::TryLinkResource(AResourceBase* InResourceRef)
{
	LinkedResource = InResourceRef;
}
