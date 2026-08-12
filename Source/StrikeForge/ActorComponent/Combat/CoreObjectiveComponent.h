#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "CoreObjectiveComponent.generated.h"

/**
 * 핵심 건물 표식 컴포넌트.
 *
 * 하는 일은 타겟 분류를 CoreBuilding 으로 지정하는 것뿐이다.
 * 이 값이 있어야 전투 유닛이 "진격 목표" 로 인식한다.
 * (AUnitBase::Advance 가 ETargetType::CoreBuilding 을 최우선으로 찾는다)
 *
 * 승패 판정은 ASFNexusBuilding::OnBuildingDestroyed 가 담당한다.
 * 파괴 → 승패는 넥서스 자신의 책임이므로 그쪽에 두는 것이 읽기 쉽다.
 */
UCLASS(ClassGroup=(StrikeForge), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UCoreObjectiveComponent : public UCombatTargetComponent
{
	GENERATED_BODY()

public:
	UCoreObjectiveComponent();
};
