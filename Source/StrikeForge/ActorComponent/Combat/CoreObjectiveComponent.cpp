#include "CoreObjectiveComponent.h"

#include "Game/GameMode/SFGameMode.h"
#include "Logging/SFLog.h"

UCoreObjectiveComponent::UCoreObjectiveComponent()
	: bFinishStageOnDestroyed(true)
{
	// 이 값이 유닛의 진격 목표가 되는 조건이다
	SetTargetType(ETargetType::CoreBuilding);
}

void UCoreObjectiveComponent::HandleOwnerDeath()
{
	const int32 LoserTeam = GetTeamIndex();

	// 레지스트리 해제와 OnTargetDestroyed 브로드캐스트는 베이스가 처리한다
	Super::HandleOwnerDeath();

	SF_LOG(Warning, "[CoreObjective] Team %d 핵심 건물 파괴", LoserTeam);

	OnCoreDestroyed(LoserTeam);

	if (!bFinishStageOnDestroyed)
	{
		SF_LOG(Warning, "[CoreObjective] bFinishStageOnDestroyed 가 꺼져 있어 스테이지를 끝내지 않습니다.");
		return;
	}

	const UWorld* World = GetWorld();
	ASFGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<ASFGameMode>() : nullptr;

	// 조용히 빠져나가면 "로그가 안 찍힌다" 는 증상만 남아 원인 파악이 어렵다.
	if (!IsValid(GameMode))
	{
		SF_LOG(Error,
			"[CoreObjective] ASFGameMode 를 찾지 못했습니다. World Settings 의 GameMode Override 가 SFGameMode(또는 그 상속 BP)인지 확인하세요. 현재 GameMode : %s",
			World && World->GetAuthGameMode() ? *World->GetAuthGameMode()->GetClass()->GetName() : TEXT("None"));
		return;
	}

	// 파괴된 코어(메인) 건물의 팀 번호를 GameMode에 전달
	// 실제 승리, 패배 판정은 GameMode가 담당
	const bool bResult = GameMode->ProcessCoreDestroyed(LoserTeam);

	if (!bResult)
	{
		// GameMode 쪽에서 이미 사유를 로그로 남긴다.
		// 가장 흔한 원인은 StartStage 가 호출되지 않아 Playing 상태가 아닌 것이다.
		SF_LOG(Warning, "[CoreObjective] 스테이지 종료 요청이 거절됐습니다. 위 로그를 확인하세요.");
	}
}
