#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
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
	Worker				UMETA(DisplayName = "일꾼"),
	ProductionBuilding	UMETA(DisplayName = "생산 건물"),
	CoreBuilding		UMETA(DisplayName = "핵심 건물")
};

/**
 * 전투 대상 규약.
 *
 * IDamagable 은 피해를 "받는" 것만 정의한다.
 * 전투 AI 가 대상을 "고르려면" 진영·분류·유효성을 물어볼 수 있어야 하므로
 * 그 부분을 여기로 분리했다.
 *
 * 유닛·건물·일꾼이 IDamagable 과 함께 이것을 구현하면
 * USFCombatSubsystem 의 탐색 대상이 된다.
 */
UINTERFACE()
class UTargetable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API ITargetable
{
	GENERATED_BODY()
public:
	/** 소속 팀. Targetable::NeutralTeam(-1) 은 누구와도 적대하지 않는다 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	int32 GetTeamIndex() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	ETargetType GetTargetType() const;

	/** 죽었지만 아직 연출 중인 대상을 걸러내기 위해 필요하다 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool IsTargetable() const;

	/**
	 * 받는 피해 감소 0~0.9.
	 *
	 * 관통 계산이 필요해 공격자가 미리 알아야 하므로 여기서 노출한다.
	 * 최종 피해 = 공격력 * (1 - max(0, 방어 - 관통))
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	float GetArmor() const;
};

namespace Targetable
{
	/** 중립 팀. 어느 쪽과도 교전하지 않는다 */
	static constexpr int32 NeutralTeam = INDEX_NONE;
}
