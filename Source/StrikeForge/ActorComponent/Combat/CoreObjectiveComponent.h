#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "CoreObjectiveComponent.generated.h"

/**
 * 핵심 건물용 전투 대상 컴포넌트.
 *
 * CombatTargetComponent 와 다른 점은 파괴 시 스테이지를 끝낸다는 것뿐이다.
 * 별도 액터 클래스를 만들지 않기 위해 컴포넌트로 분리했다.
 * 건물 BP 에 이것을 붙이면 그 건물이 곧 핵심 건물이 된다.
 */
UCLASS(ClassGroup=(StrikeForge), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UCoreObjectiveComponent : public UCombatTargetComponent
{
	GENERATED_BODY()

public:
	UCoreObjectiveComponent();

	/** 파괴 순간. 연출은 BP 에서 */
	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Objective")
	void OnCoreDestroyed(int32 InLoserTeamIndex);

protected:
	virtual void HandleOwnerDeath() override;

	/**
	 * 플레이어 진영의 팀 번호.
	 * 이 팀의 핵심이 파괴되면 패배, 그 외 팀이면 승리로 처리한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 PlayerTeamIndex = 0;

	/** 끄면 파괴돼도 스테이지를 끝내지 않는다. 전투 반복 테스트용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	uint8 bFinishStageOnDestroyed : 1;
};
