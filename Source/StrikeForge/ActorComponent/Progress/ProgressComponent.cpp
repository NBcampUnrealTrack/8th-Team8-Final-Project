#include "ProgressComponent.h"

#include "Kismet/KismetMathLibrary.h"

UProgressComponent::UProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UProgressComponent::StartProgress()
{
	// 메쉬적용 broadcast
	// will change getword()->gettimeseconds() > managesubsystem's timer
	InitializeProperties();
	OnChangedRatio.Broadcast(CurrentRatioIndex); // currentratio = 0 fill mesh 0 index
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

		Tracker.Advance(ActualDelta);
		CurrentRatio = Tracker.GetRatio();
		UE_LOG(LogTemp, Warning, TEXT("UProgressComponent::Ratio: %f"), CurrentRatio);
		
		//const float Ratio = FMath::Clamp(UKismetMathLibrary::SafeDivide(Accumulate,CompleteAmount), 0.f, 1.f);
		//UE_LOG(LogTemp, Warning, TEXT("UProgressComponent::Ratio: %f"), Ratio);
		// 메쉬적용 broadcast
		// max인지 확인
		const int32 TempRatioIndex = FindRatioIndex(CurrentRatio);
		
		if ( TempRatioIndex != CurrentRatioIndex ) // set mesh twice when completed
		{
			CurrentRatioIndex = TempRatioIndex;
			OnChangedRatio.Broadcast(CurrentRatioIndex);
		}
		if (QuarterofRatios.Num() > 0 && CurrentRatio >= QuarterofRatios[QuarterofRatios.Num() - 1])
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

void UProgressComponent::BeginPlay()
{
	Super::BeginPlay();
	//InitializeProperties(); when save load maybe this logic make trouble
}

void UProgressComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopProgress(); // defence code
	Super::EndPlay(EndPlayReason);
}
