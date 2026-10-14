#include "ProgressComponent.h"

UProgressComponent::UProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
void UProgressComponent::InitializeProperties()
{
	CurrentRatio = 0.f;
	Tracker.Reset(RequiredWork, BaseWorkRate);
}

void UProgressComponent::InitProgressWorks(float InRequiredWork, float InBaseWorkRate)
{
	RequiredWork =FMath::Max(0.01f,InRequiredWork);
	BaseWorkRate =FMath::Max(0.f,	InBaseWorkRate);
}

void UProgressComponent::StartProgress()
{
	// current ratio = 0, Tracker Reset
	InitializeProperties();
	
	LastAdvanceTime = GetWorld()->GetTimeSeconds();
	// 초기화값을 broadcast  > 0, 0 
	OnChangedProgressRatio.Broadcast(CurrentRatio, CurrentRatio);
	
	GetWorld()->GetTimerManager().SetTimer(
	ProgressTimerHandle,
	this,
	&ThisClass::AdvanceProgress,
	TickInterval,
	true);
}
void UProgressComponent::AdvanceProgress()
{
	// tick interval에 따른 경과시간 계산
	// 실행될때의 누적월드시간 get ( Now - LastAdvanceTime = 거의TickInterval )
	const float Now = GetWorld()->GetTimeSeconds();
	// Max로 음수값 차단 - Max = 둘중 큰값 return
	const float ActualDelta = FMath::Max(0.f, Now - LastAdvanceTime);
	// interval을 알기위해 현재시간대입
	LastAdvanceTime = Now;
	
	const float PreviousRatio = CurrentRatio; // 증가되기전 비율 캐싱
	const bool bCompleted = Tracker.Advance(ActualDelta); // 시간에 따라 단순 + 축적만
	CurrentRatio = Tracker.GetRatio(); // 축적값에 대한 비율 계산
	
	OnChangedProgressRatio.Broadcast(PreviousRatio, CurrentRatio);
	
	if (bCompleted)
	{
		StopProgress();
		OnCompletedProgress.Broadcast();
	}
}

void UProgressComponent::StopProgress()
{
	GetWorld()->GetTimerManager().ClearTimer(ProgressTimerHandle);
}

void UProgressComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopProgress();
	Super::EndPlay(EndPlayReason);
}
