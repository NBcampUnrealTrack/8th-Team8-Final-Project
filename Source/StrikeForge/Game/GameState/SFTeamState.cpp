#include "SFTeamState.h"
#include "Logging/SFLog.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "InGame/Controller/SFInGamePlayerController.h"

void USFTeamState::AddPopulation(int32 InDelta)
{
	if (InDelta == 0) return;

	// Maximum 으로 Clamp 하지 않는다 — 집이 파괴돼 Max 가 줄면
	// Current > Maximum 이 정상 상태다(예: 15/10).
	CurrentPopulation = FMath::Max(0, CurrentPopulation + InDelta);
	OnPopulationChanged.Broadcast(CurrentPopulation, MaximumPopulation);
}

void USFTeamState::AddMaxPopulation(int32 InDelta)
{
	if (InDelta == 0) return;

	MaximumPopulation = FMath::Max(0, MaximumPopulation + InDelta);
	OnPopulationChanged.Broadcast(CurrentPopulation, MaximumPopulation);
}

bool USFTeamState::InitializeTeamData(ESFRace InRace, USFBuildingCatalog* InBuildingCatalog)
{
	if (!IsValid(InBuildingCatalog))
	{
		SF_LOG(
			Error,
			"[SFTeamState] 유효하지 않은 BuildingCatalog입니다.");

		return false;
	}
	if (InRace == ESFRace::None) return false;
	
	if (IsValid(BuildingCatalog) &&
		BuildingCatalog != InBuildingCatalog)
	{
		SF_LOG(
			Warning,
			"[SFTeamState] BuildingCatalog가 교체됩니다. "
			"Old=%s, New=%s",
			*GetNameSafe(BuildingCatalog),
			*GetNameSafe(InBuildingCatalog));
	}

	Race = InRace;
	BuildingCatalog = InBuildingCatalog;
	return true;
}

bool USFTeamState::CanAffordPopulation(int32 InPopulationCost) const
{
	if (InPopulationCost <= 0) return true;
	return CurrentPopulation + InPopulationCost <= MaximumPopulation;
}

bool USFTeamState::CanAffordResource(const FResources& InCost) const
{
	return Mineral >= FMath::Max(0, InCost.Mineral)
		&& Ether   >= FMath::Max(0, InCost.Ether);
}

bool USFTeamState::TrySpendCost(const FResources& InCost)
{
	if (!CanAffordResource(InCost)) return false;

	SetMineral(Mineral - FMath::Max(0, InCost.Mineral));
	SetEther(Ether - FMath::Max(0, InCost.Ether));
	return true;
}

void USFTeamState::RefundCost(const FResources& InCost)
{
	SetMineral(Mineral + FMath::Max(0, InCost.Mineral));
	SetEther(Ether + FMath::Max(0, InCost.Ether));
}

void USFTeamState::SetNexus(ASFNexusBuilding* InNexus)
{
	Nexus = InNexus;
}

bool USFTeamState::IsNexus(const ASFNexusBuilding* InNexus) const
{
	return Nexus.Get() == InNexus;
}

void USFTeamState::SetPlayer(ASFInGamePlayerController* InPlayer)
{
	Player = InPlayer;
}

void USFTeamState::AddResource(EResourceType InType, int32 InAmount)
{
	switch (InType)
	{
	case EResourceType::Mineral: SetMineral(Mineral + InAmount); break;
	case EResourceType::Ether:   SetEther(Ether + InAmount);     break;
	default: SF_LOG(Warning, "Unknown Resource Type"); break;
	}
}

void USFTeamState::AddResourceIncomeRate(EResourceType InType, float InRate)
{
	switch (InType)
	{
	case EResourceType::Mineral: SetMineralIncomeRate(MineralIncomeRate + InRate); break;
	case EResourceType::Ether:   SetEtherIncomeRate(EtherIncomeRate + InRate);     break;
	default: SF_LOG(Warning, "Unknown Resource Type"); break;
	}
}

void USFTeamState::SetMineral(int32 NewMineral)
{
	Mineral = FMath::Max(0, NewMineral);
	OnMineralChanged.Broadcast(Mineral, MineralIncomeRate);
}

void USFTeamState::SetEther(int32 NewEther)
{
	Ether = FMath::Max(0, NewEther);
	OnEtherChanged.Broadcast(Ether, EtherIncomeRate);
}

void USFTeamState::SetMineralIncomeRate(float NewRate)
{
	MineralIncomeRate = FMath::Max(0.f, NewRate);
	OnMineralChanged.Broadcast(Mineral, MineralIncomeRate);
}

void USFTeamState::SetEtherIncomeRate(float NewRate)
{
	EtherIncomeRate = FMath::Max(0.f, NewRate);
	OnEtherChanged.Broadcast(Ether, EtherIncomeRate);
}



ASFNexusBuilding* USFTeamState::GetNexus() const
{
	ASFNexusBuilding* Found = Nexus.Get();
	return IsValid(Found) ? Found : nullptr;
}

USFBuildingCatalog* USFTeamState::GetBuildingCatalog() const
{
	return IsValid(BuildingCatalog)
		? BuildingCatalog
		: nullptr;
}

ASFInGamePlayerController* USFTeamState::GetPlayer() const
{
	ASFInGamePlayerController* PlayerC = Player.Get();
	return IsValid(PlayerC) ? PlayerC : nullptr;
}
