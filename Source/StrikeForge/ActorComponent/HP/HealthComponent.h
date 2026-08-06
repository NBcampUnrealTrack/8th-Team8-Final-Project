#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHealthValueChanged, float, InBeforeHP, float, InCurrentHP, float, InMaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHealthStateChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();
	
	void ApplyDamage(float InDamage);

	// ── 아래는 전투 AI 를 위해 추가한 부분 ──────────────

	/**
	 * 최대 체력을 다시 잡고 가득 채운다.
	 * 스폰 시 데이터테이블 수치를 주입할 때 쓴다.
	 * BeginPlay 보다 먼저 불려도 안전하다.
	 */
	void InitHealth(float InMaxHealth);

	/** 체력바 · 디버그 표시용 0~1 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead != 0; }
	
#pragma region Properties
	
	UPROPERTY(BlueprintAssignable)
	FHealthValueChanged OnHealthValueChanged;
	UPROPERTY(BlueprintAssignable)
	FHealthStateChanged OnHealthStateChanged;
	
	UPROPERTY()
	uint8 bIsDead;
#pragma endregion 
protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float CurrentHealth = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = true))
	float MaxHealth = 100.f;
};
