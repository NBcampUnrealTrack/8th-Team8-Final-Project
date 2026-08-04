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
	
	GetWorld()->GetTimerManager().SetTimer(
	TimerHandle,
	[this]()
	{
		// float ++
		Accumulate += AccumulateSpeed;
		const float Ratio = FMath::Clamp(UKismetMathLibrary::SafeDivide(Accumulate,CompleteAmount), 0.f, 1.f);
		UE_LOG(LogTemp, Warning, TEXT("UProgressComponent::Ratio: %f"), Ratio);
		// 메쉬적용 broadcast
		// max인지 확인
		const int32 TempRatioIndex = FindRatioIndex(Ratio);
		
		if ( TempRatioIndex != CurrentRatioIndex )
		{
			CurrentRatioIndex = TempRatioIndex;
			OnChangedRatio.Broadcast(CurrentRatioIndex);
		}
		if (Ratio >= QuarterofRatios[QuarterofRatios.Num() - 1])
		{
			//StopProgress();
			OnCompletedBuilding.Broadcast();
		}
	},
	1.f,
	true);
}

int32 UProgressComponent::FindRatioIndex(float Ratio) const
{
	int32 FoundIndex = INDEX_NONE;

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
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
}

void UProgressComponent::InitializeProperties()
{
	
	CurrentRatioIndex = 0;
	CurrentRatio = 0.f;
	Accumulate = 0.f;
}

void UProgressComponent::SetQuarterOfRatios(TArray<float> InQuarterOfRatios)
{
	QuarterofRatios = InQuarterOfRatios;
}

void UProgressComponent::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeProperties();
}
