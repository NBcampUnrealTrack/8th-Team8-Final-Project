#include "ProgressComponent.h"

UProgressComponent::UProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProgressComponent::StartProgress()
{
	InitializeProperties();
	OnChangedRatio.Broadcast(CurrentRatioIndex);
	LastAdvanceTime = GetWorld()->GetTimeSeconds();

	GetWorld()->GetTimerManager().SetTimer(
	ProgressTimerHandle,
	[this]()
	{
		// float ++
		//Accumulate += AccumulateSpeed;
		//Tracker.Advance(TickInterval);
		const float Now = GetWorld()->GetTimeSeconds();
		const float ActualDelta = Now - LastAdvanceTime;
		LastAdvanceTime = Now;

		const bool bCompleted = Tracker.Advance(ActualDelta);
		CurrentRatio = Tracker.GetRatio();
		const int32 NewRatioIndex = FindRatioIndex(CurrentRatio);

		if (NewRatioIndex != CurrentRatioIndex)
		{
			CurrentRatioIndex = NewRatioIndex;
			OnChangedRatio.Broadcast(CurrentRatioIndex);
		}

		if (bCompleted)
		{
			StopProgress();
			OnCompletedBuilding.Broadcast();
		}
	},
	TickInterval,
	true);
}

int32 UProgressComponent::FindRatioIndex(float Ratio) const
{
	int32 FoundIndex = 0;

	for (int32 Index = 0; Index < QuarterofRatios.Num(); ++Index)
	{
		if (Ratio >= QuarterofRatios[Index])
		{
			FoundIndex = Index;
		}
		else
		{
			break;
		}
	}

	return FoundIndex;
}

void UProgressComponent::StopProgress()
{
	GetWorld()->GetTimerManager().ClearTimer(ProgressTimerHandle);
}

void UProgressComponent::InitializeProperties()
{
	CurrentRatioIndex = 0;
	CurrentRatio = 0.f;
	Tracker.Reset(CompleteAmount, AccumulateSpeed);
}

void UProgressComponent::SetQuarterOfRatios(TArray<float> InQuarterOfRatios)
{
	QuarterofRatios = InQuarterOfRatios;
}

void UProgressComponent::SetDuration(float InSeconds)
{
	if (InSeconds <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("UProgressComponent::SetDuration: Invalid duration %f"), InSeconds);
		return;
	}
	AccumulateSpeed = CompleteAmount / InSeconds;
}

void UProgressComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UProgressComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopProgress();
	Super::EndPlay(EndPlayReason);
}
