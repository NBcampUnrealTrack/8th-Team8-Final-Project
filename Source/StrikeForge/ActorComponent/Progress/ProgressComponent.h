#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FChangedProgressRatio, int32, RatioIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCompletedBuilding);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressComponent();
	
	void StartProgress();
	int32 FindRatioIndex(float InRatio) const;
	void StopProgress();
	
	void InitializeProperties();
	
	void SetQuarterOfRatios(TArray<float> InQuarterOfRatios);
	
	FChangedProgressRatio OnChangedRatio;
	FCompletedBuilding OnCompletedBuilding;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
public:
	UPROPERTY()
	FTimerHandle ProgressTimerHandle;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float Accumulate = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float CompleteAmount = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float AccumulateSpeed = 10.f;
	
	UPROPERTY()
	int32 CurrentRatioIndex = 0;
	UPROPERTY()
	float CurrentRatio = 0.f;
	UPROPERTY()
	TArray<float> QuarterofRatios;
};