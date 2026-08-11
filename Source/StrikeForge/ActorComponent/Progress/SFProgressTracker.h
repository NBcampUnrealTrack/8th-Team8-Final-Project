#pragma once

#include "CoreMinimal.h"
#include "SFProgressTracker.generated.h"

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
	float Accumulate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SF|Progress")
	float CompleteAmount = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "SF|Progress")
	float BaseSpeed = 10.f;

	/** 외부 요인(버프, 일꾼 지원, 자원 보너스 등)이 건드리는 단 하나의 값 */
	UPROPERTY(BlueprintReadWrite, Category = "SF|Progress")
	float SpeedMultiplier = 1.f;

	void Reset(float InCompleteAmount, float InBaseSpeed)
	{
		Accumulate = 0.f;
		CompleteAmount = InCompleteAmount;
		BaseSpeed = InBaseSpeed;
		//SpeedMultiplier = 1.f;
	}

	/** 주기 콜백(타이머)에서 호출한다. true 반환 = 이번 호출로 100% 도달 */
	bool Advance(float DeltaTime)
	{
		Accumulate += BaseSpeed * SpeedMultiplier * DeltaTime;
		return Accumulate >= CompleteAmount;
	}

	FORCEINLINE float GetRatio() const
	{
		return CompleteAmount > 0.f ? FMath::Clamp(Accumulate / CompleteAmount, 0.f, 1.f) : 1.f;
	}
	
	FORCEINLINE void ClearSpeedMultiplier()	{ SpeedMultiplier = 1.f; }
};
 