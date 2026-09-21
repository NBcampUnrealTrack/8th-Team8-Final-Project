#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHealthValueChanged, float, InBeforeHP, float, InCurrentHP, float, InMaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();
	
	void ApplyDamage(float InDamage);
	
	// 최대체력으로 완전히 채워 초기화
	void InitializeHealth(float InMaxHealth);

	// 최대체력과 현재체력을 각각 초기화
	void InitializeHealth(float InMaxHealth, float InCurrentHealth);

	// 최대체력과 현재체력을 설정
	void SetHealth(float InMaxHealth, float InCurrentHealth);

	// 최대체력 배수증가 (체력비율 유지)
	void BoostHealth(float HealthMultiplier);

	// 현재체력만 증가. 건물 수리시 적용에정. 
	void IncreaseCurrentHealth(float InAmount);

	// // 최대체력과 현재체력을 같은 값만큼 증가
	// void IncreaseMaxAndCurrentHealth(float InAmount);

	/** 체력바 · 디버그 표시용 0~1 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	FORCEINLINE bool GetIsDead() const { return IsDead; }
#pragma region Properties

	UPROPERTY(BlueprintAssignable)
	FHealthValueChanged OnHealthValueChanged;
	UPROPERTY(BlueprintAssignable)
	FOnDeath OnHealthIsZero;
	
#pragma endregion 
	
private:
	UPROPERTY()
	bool IsDead = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float InitialHealth = 100.f;	// 순정 상태의 최대체력 (버프의 영향을 받지 않은)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float MaxHealth = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float CurrentHealth = 1.f;
};
