#pragma once

#include "CoreMinimal.h"
#include "Targetable.generated.h"

/**
 * 타겟 분류.
 * 전투 AI 는 클래스 캐스팅 없이 이 값만으로 공격 우선순위를 판단한다.
 */
UENUM(BlueprintType)
enum class ETargetType : uint8
{
	CombatUnit			UMETA(DisplayName = "전투 유닛"),
	Commander			UMETA(DisplayName = "지휘관"),
	Worker				UMETA(DisplayName = "일꾼"),			// 미사용 — 일꾼 없는 설계
	ProductionBuilding	UMETA(DisplayName = "생산 건물"),
	CoreBuilding		UMETA(DisplayName = "핵심 건물"),
	ResourceBuilding	UMETA(DisplayName = "채광 건물")		// 광물 위에 짓는 건물
};

namespace Targetable
{
	/** 중립 팀. 어느 쪽과도 교전하지 않는다 */
	static constexpr int32 NeutralTeam = INDEX_NONE;
	
	/** 플레이어 진영 팀 번호 (1v1 프로토타입 기준) */
	static constexpr int32 PlayerTeam = 0;
	
	/** 적 진영 팀 번호 (1v1 프로토타입 기준) */
	static constexpr int32 EnemyTeam = 1;
}

/*
 * 참고 — ITargetable 인터페이스는 제거했다.
 *
 * 전투 대상 여부·팀·분류는 UCombatTargetComponent 가 전담한다.
 * 액터가 인터페이스를 구현하게 하면 클래스 선언을 수정해야 하는데,
 * 그러면 다른 작업자 파일을 건드리게 된다.
 *
 * 컴포넌트를 붙이는 방식은 BP 에서 끝나므로 C++ 수정이 필요 없다.
 */
