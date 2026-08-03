#include "ProgressComponent.h"

UProgressComponent::UProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UProgressComponent::InitProgress()
{
	// 메쉬적용 broadcast
	GetWorld()->GetTimerManager().SetTimer(
	TimerHandle,
	[]()
	{
		// float ++
		// 메쉬적용 broadcast
		// max인지 확인
	},
	1.f,
	true);
}

void UProgressComponent::BeginPlay()
{
	Super::BeginPlay();
	
}
