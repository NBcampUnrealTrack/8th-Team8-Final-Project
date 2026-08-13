#include "SFNexusBuilding.h"
#include "ActorComponent/Combat/CoreObjectiveComponent.h"
#include "Logging/SFLog.h"
ASFNexusBuilding::ASFNexusBuilding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCoreObjectiveComponent>(TEXT("CombatTargetComponent")))
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASFNexusBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();

	SF_LOG(Log, "[SFNexusBuilding] Nexus destroyed : %s", *GetName());

	// CoreObjectiveComponent 부분에서 승리, 패배 전달 담당하므로 밑에 주석 제거해도 상관없습니다.

}
