// SFGameState.cpp


#include "SFGameState.h"

ASFGameState::ASFGameState()
{
	// 상태 초기값
	StageState = ESFStageState::Initializing;
	StageResult = ESFStageResult::Undetermined;
}

bool ASFGameState::StartStage()
{
	// 스테이지 시작 부분
	if (StageState != ESFStageState::Initializing)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameState] StartStage failed: "
				"Stage is not Initializing.")
		);

		return false;
	}

	StageState = ESFStageState::Playing;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SFGameState] Stage started.")
	);

	OnStageStateChanged.Broadcast(StageState);

	return true;
}

bool ASFGameState::FinishStage(ESFStageResult NewStageResult)
{
	if (StageState != ESFStageState::Playing)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameState] FinishStage failed: "
				"Stage is not Playing.")
		);

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameState] FinishStage failed: "
				"Result is Undetermined.")
		);

		return false;
	}

	/*
	*	1. 이벤트 보내기 전에 결과와 상태를 모두 설정
	*	2. 받은 객체가 즉시 GameState를 조회시
	*	3. 완성된 종료 상태를 확인
	*/
	StageResult = NewStageResult;
	StageState = ESFStageState::Finished;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SFGameState] Stage finished. Result: %s"),
		*UEnum::GetValueAsString(StageResult)
	);

	OnStageResultChanged.Broadcast(StageResult);
	OnStageStateChanged.Broadcast(StageState);

	return true;
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
