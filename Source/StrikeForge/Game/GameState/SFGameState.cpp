// SFGameState.cpp


#include "SFGameState.h"
#include "Logging/SFLog.h"

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
