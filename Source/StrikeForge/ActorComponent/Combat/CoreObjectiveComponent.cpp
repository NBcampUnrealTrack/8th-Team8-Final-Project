#include "CoreObjectiveComponent.h"

UCoreObjectiveComponent::UCoreObjectiveComponent()
{
	// 이 값이 유닛의 진격 목표가 되는 조건이다
	SetTargetType(ETargetType::CoreBuilding);
}
