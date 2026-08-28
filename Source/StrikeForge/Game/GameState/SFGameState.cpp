// SFGameState.cpp
#include "SFGameState.h"
#include "Logging/SFLog.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Interfaces/Units/Targetable.h"

ASFGameState::ASFGameState()
{
	// 상태 초기값
	StageState = ESFStageState::Initializing;
	StageResult = ESFStageResult::Undetermined;
}

bool ASFGameState::InitializeStage(FName StageRowName)
{
	if (StageRowName.IsNone())
	{
		SF_LOG(Warning, "[SFGameState] InitializeStage failed : Stage Row Name is None.");

		return false;
	}

	if (StageState != ESFStageState::Initializing)
	{
		SF_LOG(Warning, "[SFGameState] InitializeStage failed : Stage is not Initializing.");

		return false;
	}

	CurrentStageRowName = StageRowName;

	SF_LOG(Log, "[SFGameState] Stage initialized : %s", *CurrentStageRowName.ToString());

	return true;
}

bool ASFGameState::StartStage()
{
	// 스테이지 시작 부분
	if (StageState != ESFStageState::Initializing)
	{
		SF_LOG(Warning, "[SFGameState] StartStage failed: Stage is not Initializing.");

		return false;
	}

	StageState = ESFStageState::Playing;

	SF_LOG(Log, "[SFGameState] Stage started.");

	OnStageStateChanged.Broadcast(StageState);

	return true;
}

bool ASFGameState::FinishStage(ESFStageResult NewStageResult)
{
	if (StageState != ESFStageState::Playing)
	{
		SF_LOG(Warning, "[SFGameState] FinishStage failed: Stage is not Playing.");

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		SF_LOG(Warning, "[SFGameState] FinishStage failed: Result is Undetermined.");

		return false;
	}

	/*
	*	1. 이벤트 보내기 전에 결과와 상태를 모두 설정
	*	2. 받은 객체가 즉시 GameState를 조회시
	*	3. 완성된 종료 상태를 확인
	*/
	StageResult = NewStageResult;
	StageState = ESFStageState::Finished;

	SF_LOG(Log, "[SFGameState] Stage finished. Result: %s", *UEnum::GetValueAsString(StageResult));

	OnStageResultChanged.Broadcast(StageResult);
	OnStageStateChanged.Broadcast(StageState);

	return true;
}

FName ASFGameState::GetCurrentStageRowName() const
{
	return CurrentStageRowName;
}

ESFStageState ASFGameState::GetStageState() const
{
	return StageState;
}

ESFStageResult ASFGameState::GetStageResult() const
{
	return StageResult;
}

bool ASFGameState::IsStagePlaying() const
{
	return StageState == ESFStageState::Playing;
}

bool ASFGameState::IsStageFinished() const
{
	return StageState == ESFStageState::Finished;
}

//기존 Delegate 시그니처를 그대로 활용


// 26.08.18
bool ASFGameState::RegisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	if (!IsValid(Nexus))
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: Nexus is invalid.");
		return false;
	}

	USFTeamState* Team = FindOrAddTeam(TeamIndex);
	if (!Team)
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: TeamIndex %d is invalid.", TeamIndex);
		return false;
	}

	if (ASFNexusBuilding* Existing = Team->GetNexus())
	{
		if (Existing != Nexus)
		{
			SF_LOG(Warning, "[SFGameState] Team %d 의 넥서스가 교체됩니다. Old: %s, New: %s",
				TeamIndex, *GetNameSafe(Existing), *GetNameSafe(Nexus));
		}
	}

	Team->SetNexus(Nexus);
	return true;
}

bool ASFGameState::UnregisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	USFTeamState* Team = GetTeamState(TeamIndex);
	if (!Team || !Team->IsNexus(Nexus)) return false;

	// 팀 항목 자체는 남긴다 — 자원·인구가 들어있다
	Team->SetNexus(nullptr);
	return true;
}

ASFNexusBuilding* ASFGameState::GetNexus(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetNexus() : nullptr;
}

USFTeamState* ASFGameState::GetTeamState(int32 TeamIndex) const
{
	const TObjectPtr<USFTeamState>* Found = TeamStates.Find(TeamIndex);
	return Found ? Found->Get() : nullptr;
}

int32 ASFGameState::GetNexusTier(int32 TeamIndex) const
{
	const ASFNexusBuilding* Nexus = GetNexus(TeamIndex);
	if (!Nexus)
	{
		SF_LOG(Error, "[SFGameState] Team %d 에 넥서스가 등록되어 있지 않습니다. "
			"GameMode 의 PlayerTeamIndex 와 레벨 넥서스의 팀이 일치하는지 확인하세요.", TeamIndex);
		return INDEX_NONE;
	}
	return Nexus->GetCurrentTier();
}

USFTeamState* ASFGameState::FindOrAddTeam(int32 TeamIndex)
{
	if (TeamIndex == Targetable::NeutralTeam) return nullptr;

	if (TObjectPtr<USFTeamState>* Found = TeamStates.Find(TeamIndex))
	{
		return *Found;
	}

	USFTeamState* NewTeam = NewObject<USFTeamState>(this);
	TeamStates.Add(TeamIndex, NewTeam);
	return NewTeam;
}

// 여기까지 추가분

// 판단로직

bool ASFGameState::CanAffordResource(int32 TeamIndex, const FResources& InResource) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->CanAffordResource(InResource) : false;
}

bool ASFGameState::CanAffordPopulation(int32 TeamIndex, int32 InPopulationCost) const
{	// 음수도 가능하게 해서 유닛이 죽엇을때 감소되게함
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->CanAffordPopulation(InPopulationCost) : false;
}

// add

void ASFGameState::AddResource(int32 TeamIndex, EResourceType InResourceType, int32 InResourceAmount)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->AddResource(InResourceType, InResourceAmount);
}
void ASFGameState::AddResourceIncomeRate(int32 TeamIndex, EResourceType InResourceType, float InIncomeRate)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex)) Team->AddResourceIncomeRate(InResourceType, InIncomeRate);
}
void ASFGameState::AddPopulation(int32 TeamIndex, int32 InMaximumPopulation)
{
if (USFTeamState* Team = FindOrAddTeam(TeamIndex)) Team->AddPopulation(InMaximumPopulation);
}

void ASFGameState::AddMaxPopulation(int32 TeamIndex, int32 InMaximumPopulation)
{	// 최대인구 증가
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->AddMaxPopulation(InMaximumPopulation);
}

// Minus

bool ASFGameState::TrySpendCost(int32 TeamIndex, const FResources& InResources)
{	// 판단 + 소비까지
	USFTeamState* Team = FindOrAddTeam(TeamIndex);
	return Team ? Team->TrySpendCost(InResources) : false;
}

void ASFGameState::RefundCost(int32 TeamIndex, const FResources& InResources)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->RefundCost(InResources);
}

// getter setter

int32 ASFGameState::GetMineral(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMineral() : 0;
}

int32 ASFGameState::GetEther(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetEther() : 0;
}

float ASFGameState::GetMineralIncomeRate(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMineralIncomeRate() : 0.f;
}

float ASFGameState::GetEtherIncomeRate(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetEtherIncomeRate() : 0.f;
}

int32 ASFGameState::GetCurrentPopulation(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetCurrentPopulation() : 0;
}

int32 ASFGameState::GetMaximumPopulation(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMaximumPopulation() : 0;
}

bool ASFGameState::SetTeamBuildingCatalog(int32 TeamIndex, USFBuildingCatalog* InBuildingCatalog)
{
	if (!IsValid(InBuildingCatalog))
	{
		SF_LOG(Error,
			"[SFGameState] Team %d에 설정할 "
			"BuildingCatalog가 유효하지 않습니다.",
			TeamIndex);

		return false;
	}

	USFTeamState* Team = FindOrAddTeam(TeamIndex);

	if (!IsValid(Team))
	{
		SF_LOG(
			Error,
			"[SFGameState] TeamState를 준비할 수 없습니다. "
			"Team=%d",
			TeamIndex);

		return false;
	}

	return Team->SetBuildingCatalog(InBuildingCatalog);
}

USFBuildingCatalog* ASFGameState::GetTeamBuildingCatalog(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);

	return IsValid(Team)?
			Team->GetBuildingCatalog(): nullptr;
}
