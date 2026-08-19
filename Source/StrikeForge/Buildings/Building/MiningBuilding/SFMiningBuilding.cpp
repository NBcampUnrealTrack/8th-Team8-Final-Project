#include "SFMiningBuilding.h"
#include "Game/GameState/SFGameState.h"
#include "Buildings/Resource/ResourceBase.h"
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

void ASFMiningBuilding::OnBuildingDefinitionLoaded(const FBuildingAppearance& Definition)
{
	Super::OnBuildingDefinitionLoaded(Definition);
	
	CanDigResourceType = Definition.CanDigResourceType;
}

void ASFMiningBuilding::OnTierActivated(const FBuildingTier& ActivatedTier)
{
	Super::OnTierActivated(ActivatedTier);
	
	StopMining();
	
	DiggingIncomeRate = DiggingAmount / DiggingInterval;
	
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	SFGameState->AddResourceIncomeRate(LinkedResource->GetResourceType(), DiggingIncomeRate);
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
	SFGameState->AddResourceIncomeRate(LinkedResource->GetResourceType(), DiggingIncomeRate * -1);
}

void ASFMiningBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	

}

void ASFMiningBuilding::StartMining()
{
	if (!LinkedResource.IsValid()) return;
	
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
	
	if (!LinkedResource.IsValid())
	{
		StopMining();
		SF_LOG(Error, "Mine에 LinkedResource가 등록되지않았습니다.");
		return;
	}
	
	const int32 ExtractedAmount = 
	LinkedResource.Get()->Extract(DiggingAmount);
	
	if (ExtractedAmount <= 0) // 자원고갈
	{
		StopMining();
		SF_LOG(Error, "Mine에 LinkedResource의 자원이 고갈되었습니다.");
		return;
	}
	
	SFGameState->AddResource(
		LinkedResource.Get()->GetResourceType(), ExtractedAmount);
}

void ASFMiningBuilding::TryLinkResource(AResourceBase* InResourceRef)
{
	LinkedResource = InResourceRef;
}
