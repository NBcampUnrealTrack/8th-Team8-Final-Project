#include "SFHouseBuilding.h"

#include "Datas/Building/BuildingDefinition/SFHouseBuildingDefinition.h"
#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

FSFSelectableInfo ASFHouseBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();
	Info.SelectableType = ESFSelectableType::NormalBuilding;
	return Info;
}

void ASFHouseBuilding::OnTierActivated()
{
	Super::OnTierActivated();
	// 최대 인구 증가 로직
	const USFHouseBuildingDefinition* Definition = Cast<USFHouseBuildingDefinition>(GetBuildingDefinition());
	if (!IsValid(Definition))
	{
		SF_LOG(
			Error,
			"[%s] HouseBuildingDefinition이 아닙니다. Definition=%s",
			*GetName(),
			*GetNameSafe(GetBuildingDefinition()));

		return;
	}

	const FSFHouseBuildingTierData* ActivatedTier =	Definition->FindHouseTier(GetCurrentTier());
	if (!ActivatedTier)
	{
		SF_LOG(
			Error,
			"[%s] House Tier가 없습니다. Tier=%d",
			*GetName(),
			GetCurrentTier());

		return;
	}

	if (ActivatedTier->PopulationCapacity < 0)
	{
		SF_LOG(
			Error,
			"[%s] PopulationCapacity가 0 미만입니다.",
			*GetName());

		return;
	}

	const int32 PreviousPopulationIncrease = MaxPopulationIncreaseAmount;
	MaxPopulationIncreaseAmount = ActivatedTier->PopulationCapacity;
	const int32 IncreasePopulation = MaxPopulationIncreaseAmount - PreviousPopulationIncrease;

	ASFGameState* SFGameState =	GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState))
	{
		return;
	}

	SFGameState->AddMaxPopulation(GetTeamIndex(),IncreasePopulation);
}

void ASFHouseBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	
	// 최대 인구 감소 로직
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	SFGameState->AddMaxPopulation(GetTeamIndex(), MaxPopulationIncreaseAmount * -1);

}
