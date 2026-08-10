// SFGameMode.cpp


#include "SFGameMode.h"
#include "Logging/SFLog.h"

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
		SF_LOG(Error, "[SFGameMode] Failed to find SFGameState.");

		return;
	}

	SF_LOG(Log, "[SFGameMode] Stage initialization started.");

	// 나중에 타이틀, 로비, 인게임 맵 추가 시 작성될 부분

	StartStage();
}

bool ASFGameMode::StartStage()
{
	if (!IsValid(SFGameState))
	{
		SF_LOG(Error, "[SFGameMode] StartStage failed: GameState is invalid.");

		return false;
	}

	if (!SFGameState->StartStage())
	{
		SF_LOG(Warning, "[SFGameMode] StartStage was rejected by GameState.");

		return false;
	}

	SF_SCREEN_LOG(Log, 5.0f, FColor::Green, "[SFGameMode] Stage started.");

	return true;
}

bool ASFGameMode::ProcessCoreDestroyed(int32 DestroyedTeamIndex)
{
	if (DestroyedTeamIndex == INDEX_NONE)
	{
		SF_LOG(Warning, "[SFGameMode] ProcessCoreDestroyed failed: Destroyed Team Index is invalid.");

		return false;
	}

	SF_LOG(Log, "[SFGameMode] Core destroyed. DestroyedTeam: %d, PlayerTeam: %d", DestroyedTeamIndex, PlayerTeamIndex);

	if (DestroyedTeamIndex == PlayerTeamIndex)
	{
		return ProcessDefeat();
	}

	return ProcessVictory();
	
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
		SF_LOG(Error, "[SFGameMode] FinishStage failed: GameState is invalid.");

		return false;
	}

	if (!SFGameState->IsStagePlaying())
	{
		SF_LOG(Warning, "[SFGameMode] FinishStage failed: Stage is not playing.");

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		SF_LOG(Warning, "[SFGameMode] Undetermined is not a valid final result.");

		return false;
	}

	if (!SFGameState->FinishStage(NewStageResult))
	{
		SF_LOG(Warning, "[SFGameMode] FinishStage was rejected by GameState.");

		return false;
	}

	const FColor ResultColor = NewStageResult == ESFStageResult::Victory ? FColor::Green : FColor::Red;

	SF_SCREEN_LOG(Log, 5.0f, ResultColor, "[SFGameMode] Stage finished. Result: %s", *UEnum::GetValueAsString(NewStageResult));

	return true;
}

ASFGameState* ASFGameMode::GetSFGameState() const
{
	return SFGameState;
}
