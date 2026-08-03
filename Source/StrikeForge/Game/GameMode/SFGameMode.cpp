// SFGameMode.cpp


#include "SFGameMode.h"

ASFGameMode::ASFGameMode()
{
	// GameMode가 사용할 GameState 클래스 지정
	// World Setting에서 별도로 잘못 설정하는 상황 방지
	GameStateClass = ASFGameState::StaticClass();
}

void ASFGameMode::BeginPlay()
{
	Super::BeginPlay();

	SFGameState = GetGameState<ASFGameState>();

	if (!IsValid(SFGameState))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[SFGameMode] Failed to find SFGameState.")
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SFGameMode] Stage initialization Started.")
	);

	// 나중에 타이틀, 로비, 인게임 맵 추가 시 작성될 부분

	StartStage();
}

bool ASFGameMode::StartStage()
{
	if (!IsValid(SFGameState))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[SFGameMode] StartStage failed : GameState is invalid.")
		);

		return false;
	}

	if (!SFGameState->StartStage())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameMode] StartStage was rejected by GameState.")
		);

		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SFGameMode] Stage Started")
	);

	return true;
}

bool ASFGameMode::ProcessVictory()
{
	return FinishStage(ESFStageResult::Victory);
}

bool ASFGameMode::ProcessDefeat()
{
	return FinishStage(ESFStageResult::Defeat);
}

bool ASFGameMode::FinishStage(ESFStageResult NewStageResult)
{
	if (!IsValid(SFGameState))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[SFGameMode] FinishStage failed : GameState is invalid.")
		);

		return false;
	}

	if (!SFGameState->IsStagePlaying())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameMode] FinishStage failed : Stage is not playing.")
		);

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameMode] Undetermined is not a valid final result.")
		);

		return false;
	}

	if (!SFGameState->FinishStage(NewStageResult))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SFGameMode] FinishStage was rejected by GameState.")
		);

		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[SFGameMode] Stage finished. Result : %s"),
		*UEnum::GetValueAsString(NewStageResult)
	);

	return true;
}

ASFGameState* ASFGameMode::GetSFGameState() const
{
	return SFGameState;
}
