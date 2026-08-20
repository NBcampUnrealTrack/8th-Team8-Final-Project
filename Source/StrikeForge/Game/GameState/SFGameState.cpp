// SFGameState.cpp
#include "SFGameState.h"
#include "Logging/SFLog.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"

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

int32 ASFGameState::GetMineral() const
{
	return Mineral;
}

int32 ASFGameState::GetEther() const
{
	return Ether;
}

float ASFGameState::GetMineralIncomeRate() const
{
	return MineralIncomeRate;
}

float ASFGameState::GetEtherIncomeRate() const
{
	return EtherIncomeRate;
}

int32 ASFGameState::GetCurrentPopulation() const
{
	return CurrentPopulation;
}

int32 ASFGameState::GetMaximumPopulation() const
{
	return MaximumPopulation;
}

//기존 Delegate 시그니처를 그대로 활용
void ASFGameState::SetMineral(int32 NewMineral)
{
	Mineral = FMath::Max(0, NewMineral);

	OnMineralChanged.Broadcast(
		Mineral,
		MineralIncomeRate
	);
}

void ASFGameState::SetEther(int32 NewEther)
{
	Ether = FMath::Max(0, NewEther);

	OnEtherChanged.Broadcast(
		Ether,
		EtherIncomeRate
	);
}

void ASFGameState::SetMineralIncomeRate(
	float NewMineralIncomeRate
)
{
	MineralIncomeRate =
		FMath::Max(0.0f, NewMineralIncomeRate);

	OnMineralChanged.Broadcast(
		Mineral,
		MineralIncomeRate
	);
}

void ASFGameState::SetEtherIncomeRate(
	float NewEtherIncomeRate
)
{
	EtherIncomeRate =
		FMath::Max(0.0f, NewEtherIncomeRate);

	OnEtherChanged.Broadcast(
		Ether,
		EtherIncomeRate
	);
}

void ASFGameState::SetPopulation(
	int32 NewCurrentPopulation,
	int32 NewMaximumPopulation
)
{
	MaximumPopulation =
		FMath::Max(0, NewMaximumPopulation);

	CurrentPopulation =
		FMath::Clamp(
			NewCurrentPopulation,
			0,
			MaximumPopulation
		);

	OnPopulationChanged.Broadcast(
		CurrentPopulation,
		MaximumPopulation
	);
	
	SF_LOG(Warning, "Current Population: %d / %d", CurrentPopulation, MaximumPopulation);
}

void ASFGameState::AddCurrentPopulation(int32 NewCurrentPopulation)
{	// 현재인구 증가
	//const int32 IncreasePopValue = FMath::Max(0, NewCurrentPopulation);
	//if (IncreasePopValue <= 0) return;
	CurrentPopulation += NewCurrentPopulation;
	OnPopulationChanged.Broadcast(CurrentPopulation, MaximumPopulation);
	
	SF_LOG(Warning, "Current Population: %d / %d", CurrentPopulation, MaximumPopulation);
}

void ASFGameState::AddMaxPopulation(int32 NewMaximumPopulation)
{	// 최대인구 증가
	//const int32 IncreasePopValue = FMath::Max(0, NewMaximumPopulation);
	//if (IncreasePopValue <= 0) return;
	MaximumPopulation += NewMaximumPopulation;
	OnPopulationChanged.Broadcast(CurrentPopulation, MaximumPopulation);
	
	SF_LOG(Warning, "Current Population: %d / %d", CurrentPopulation, MaximumPopulation);
}

void ASFGameState::AddResourceIncomeRate(EResourceType InResourceType, float InIncomeRate)
{
	switch (InResourceType)
	{
	case EResourceType::Ether:
		{
			SetEtherIncomeRate(EtherIncomeRate + InIncomeRate);
			break;
		}
	case EResourceType::Mineral:
		{
			SetMineralIncomeRate(MineralIncomeRate + InIncomeRate);
			break;
		}
	default:
		{
			SF_LOG(Warning, "Unknown Resource Type");
			break;
		}
	}
}

void ASFGameState::AddResource(EResourceType InResourceType, int32 InResourceAmount)
{
	switch (InResourceType)
	{
	case EResourceType::Ether:
		{
			SetEther(Ether + InResourceAmount);
			break;
		}
	case EResourceType::Mineral:
		{
			SetMineral(Mineral + InResourceAmount);
			break;
		}
	default:
		{
			SF_LOG(Warning, "Unknown Resource Type");
			break;
		}
	}
}

bool ASFGameState::CanAfford(int32 InMineral, int32 InEther) const
{
	return Mineral >= FMath::Max(0, InMineral)
		&& Ether   >= FMath::Max(0, InEther);
}

bool ASFGameState::TrySpendCost(int32 InMineral, int32 InEther)
{
	if (!CanAfford(InMineral, InEther)) return false;

	SetMineral(Mineral - FMath::Max(0, InMineral));
	SetEther(Ether - FMath::Max(0, InEther));
	return true;
}

void ASFGameState::RefundCost(int32 InMineral, int32 InEther)
{
	SetMineral(Mineral + FMath::Max(0, InMineral));
	SetEther(Ether + FMath::Max(0, InEther));
}

// 26.08.18
bool ASFGameState::RegisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	if (TeamIndex == INDEX_NONE)
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: TeamIndex is invalid.");

		return false;
	}

	if (!IsValid(Nexus))
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: Nexus is invalid.");

		return false;
	}

	ASFNexusBuilding* ExistingNexus = GetNexus(TeamIndex);

	if (IsValid(ExistingNexus) && ExistingNexus != Nexus)
	{
		SF_LOG(Warning, "[SFGameState] Existing Nexus will be replaced. Team: %d, Old: %s, New: %s", TeamIndex, *GetNameSafe(ExistingNexus), *GetNameSafe(Nexus));
	}

	NexusByTeam.Add(TeamIndex, Nexus);

	SF_LOG(Log, "[SFGameState] Nexus registered. Team: %d, Nexus: %s", TeamIndex, *GetNameSafe(Nexus));

	return true;
}

bool ASFGameState::UnregisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	ASFNexusBuilding* RegisteredNexus = GetNexus(TeamIndex);

	if (!IsValid(RegisteredNexus))
	{
		return false;
	}

	// 같은 팀에 새 넥서스가 등록된 경우 잘못 제거하지 않도록 비교
	if (RegisteredNexus != Nexus)
	{
		SF_LOG(Warning, "[SFGameState] UnregisterNexus rejected: Nexus does not match. Team: %d", TeamIndex);

		return false;
	}

	NexusByTeam.Remove(TeamIndex);

	SF_LOG(Log, "[SFGameState] Nexus unregistered. Team: %d", TeamIndex);

	return true;
}

ASFNexusBuilding* ASFGameState::GetNexus(int32 TeamIndex) const
{
	ASFNexusBuilding* Nexus = NexusByTeam.FindRef(TeamIndex).Get();

	return IsValid(Nexus) ? Nexus : nullptr;
}
// 여기까지 추가분
