#include "SFHouseBuilding.h"

#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

ASFHouseBuilding::ASFHouseBuilding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	
}

FSFSelectableInfo ASFHouseBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();
	Info.SelectableType = ESFSelectableType::NormalBuilding;
	return Info;
}

void ASFHouseBuilding::OnTierActivated(const FBuildingTier& ActivatedTier)
{
	Super::OnTierActivated(ActivatedTier);
	// 최대 인구 증가 로직
	if (ActivatedTier.PopulationCapacity <= 0)
	{
		SF_LOG(Error, "House Increase Population is less than 0");
	}
	// 다음티어의 증가분 - 현재티어 증가분 = 증가할 값
	 const int32 TempMaxPopulationIncreaseAmount = MaxPopulationIncreaseAmount;
	MaxPopulationIncreaseAmount = ActivatedTier.PopulationCapacity;
	 const int32 IncreasePopValue = MaxPopulationIncreaseAmount - TempMaxPopulationIncreaseAmount;
	// 증가 로직
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMaxPopulation(GetTeamIndex(), IncreasePopValue);
}

void ASFHouseBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
}

void ASFHouseBuilding::OnTierDeactivated(const FBuildingTier& DeactivatedTier)
{
	Super::OnTierDeactivated(DeactivatedTier);
	// Deactivate에 감소 로직을 적으면 전체 인구가 감소됬다가 증가됨.
	// 이경우 최대인구가 감소될때 생산시설의 진행도가 짧은시간이지만 전부 정지될수있음.
}

void ASFHouseBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	
	// 최대 인구 감소 로직
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMaxPopulation(GetTeamIndex(), MaxPopulationIncreaseAmount * -1);
}
