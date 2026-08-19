#include "SFMiningBuilding.h"

#include "Game/GameState/SFGameState.h"

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
	
	MiningIncomeRate = ActivatedTier.MiningIncomeRate;
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMineralIncomeRate(ActivatedTier.MiningIncomeRate);
}

void ASFMiningBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMineralIncomeRate(MiningIncomeRate * -1);
}

void ASFMiningBuilding::OnTierDeactivated(const FBuildingTier& DeactivatedTier)
{
	Super::OnTierDeactivated(DeactivatedTier);
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMineralIncomeRate(MiningIncomeRate * -1);
}

void ASFMiningBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	

}
