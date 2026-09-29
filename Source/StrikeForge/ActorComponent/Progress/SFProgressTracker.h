#pragma once

#include "CoreMinimal.h"
#include "SFProgressTracker.generated.h"

/**
 * 누적 방식 진행도 계산기.
 * 건설·생산·채굴 컴포넌트가 진행도와 출처별 작업 배율을 공통으로 관리할 때 사용합니다.
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
	float BaseWorkAmount = 10.f;
	/** 등록된 버프·시너지 배율을 모두 곱한 최종 작업속도 값입니다. */
	UPROPERTY(BlueprintReadWrite, Category = "SF|Progress")
	float WorkRateMultiplier = 1.f;

	/** 진행도를 0으로 되돌리고 새 작업량과 초당 기본 작업량을 설정합니다. */
	void Reset(float InRequiredWork, float InBaseWorkAmount);

	/** 주기 콜백(타이머)에서 호출한다. true 반환 = 이번 호출로 100% 도달 */
	bool Advance(float DeltaTime);
	
	/** 현재 누적 작업량을 0~1 범위의 진행률로 반환합니다. */
	float GetRatio() const;

	/** 같은 출처의 배율이 새 값으로 실제 변경되는지 검사합니다. */
	bool WouldChangeWorkRateModifier(FName ModifierId, float InMultiplier) const;

	/** 지정한 출처의 작업 배율이 현재 등록되어 있는지 반환합니다. */
	FORCEINLINE bool HasWorkRateModifier(FName ModifierId) const
	{
		return !ModifierId.IsNone() && WorkRateModifiers.Contains(ModifierId);
	}

	/** 출처별 배율을 등록하거나 교체한 뒤 최종 작업 배율을 다시 계산합니다. */
	void SetWorkRateModifier(FName ModifierId, float InMultiplier);

	/** 지정한 출처의 배율을 제거하고 남은 배율들의 곱을 다시 계산합니다. */
	bool RemoveWorkRateModifier(FName ModifierId);

private:
	/** 등록된 모든 출처의 배율을 곱해 실제 작업 배율에 반영합니다. */
	void RecalculateWorkRateMultiplier();

	/** 시너지와 스킬처럼 서로 다른 출처가 등록한 작업 배율입니다. */
	TMap<FName, float> WorkRateModifiers;
};
