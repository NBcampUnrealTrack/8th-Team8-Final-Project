#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/Progress/SFProgressTracker.h"
#include "Components/ActorComponent.h"
#include "ProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChangedProgressRatio, float, InPreviousRatio, float, InCurrentRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCompletedProgress);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressComponent();
	
	void StartProgress();
	void StopProgress();
	
	void InitializeProperties();
	
	// change speed
	UFUNCTION(BlueprintCallable, Category = "SF|Progress")
	void SetWorkRateMultiplier(float InMultiplier) { Tracker.WorkRateMultiplier = InMultiplier; }
	UFUNCTION(BlueprintCallable, Category = "SF|Progress")
	//void SetDuration(float InSeconds);
	void InitProgressWorks(float InRequiredWork, float InBaseWorkRate);
	// getter
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Progress")
	FORCEINLINE float GetCurrentProgressRatio() const { return CurrentRatio; }
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void AdvanceProgress();	
public:
	
	UPROPERTY()
	FTimerHandle ProgressTimerHandle;
	UPROPERTY()
	float CurrentRatio = 0.f;
	UPROPERTY()
	float LastAdvanceTime = 0.f;
	/** 타이머 주기(초).**/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress")
	float TickInterval = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float RequiredWork = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (AllowPrivateAccess = true))
	float BaseWorkRate = 10.f;
	// time structure
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FSFProgressTracker Tracker;
	// delegate
	UPROPERTY(BlueprintAssignable)
	FChangedProgressRatio OnChangedProgressRatio;
	UPROPERTY(BlueprintAssignable)
	FCompletedProgress OnCompletedProgress;
};