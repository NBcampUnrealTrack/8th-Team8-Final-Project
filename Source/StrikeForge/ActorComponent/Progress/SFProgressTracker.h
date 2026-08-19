#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "SFProgressTracker.generated.h"

class UKismetMathLibrary;
/**
 * 누적 방식 진행도 계산기.
 * UProgressComponent(건설), UProductComponent(생산)가 공통으로 쓴다.
 * SpeedMultiplier만 바꾸면 외부 요인(버프, 자원 보너스 등)으로 즉시 속도가 바뀐다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFProgressTracker
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "SF|Progress")
	float CurrentWork = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "SF|Progress")
	float RequiredWork = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "SF|Progress")
	float BaseWorkRate = 10.f;
	/** 외부 요인(버프, 일꾼 지원, 자원 보너스 등)이 건드리는 단 하나의 값 */
	UPROPERTY(BlueprintReadWrite, Category = "SF|Progress")
	float WorkRateMultiplier = 1.f;

	void Reset(float InRequiredWork, float InBaseWorkRate)
	{
		CurrentWork = 0.f;
		RequiredWork = InRequiredWork;
		BaseWorkRate = InBaseWorkRate;
		//SpeedMultiplier = 1.f;
	}

	/** 주기 콜백(타이머)에서 호출한다. true 반환 = 이번 호출로 100% 도달 */
	bool Advance(float DeltaTime)
	{
		// 현재 축적치 = 시간당 축적량 * 속도 * deltatime
		// deltatime이 곧 타이머 interval이기때문에 값이 작아지면 그만큼더 자주 불림
		CurrentWork += BaseWorkRate * WorkRateMultiplier * DeltaTime;
		return CurrentWork >= RequiredWork ;
	}
	
	FORCEINLINE float GetRatio() const
	{
		return FMath::Clamp(UKismetMathLibrary::SafeDivide(CurrentWork, RequiredWork ), 0.f, 1.f);
	}
	
	FORCEINLINE void ResetWorkRateMultiplier()	{ WorkRateMultiplier = 1.f; }
};
 