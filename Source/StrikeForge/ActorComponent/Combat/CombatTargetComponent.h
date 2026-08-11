#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Units/Targetable.h"
#include "CombatTargetComponent.generated.h"

class UHealthComponent;
class UCombatTargetComponent;	// 아래 델리게이트가 자기 타입을 참조하므로 선언이 먼저 필요하다

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatTargetDestroyed, UCombatTargetComponent*, InTarget);

/**
 * 이 컴포넌트를 붙이면 어떤 액터든 전투 대상이 된다.
 *
 * 액터 클래스를 수정하지 않는 것이 목적이다.
 * 건물이든 유닛이든 BP 에서 Add Component 로 붙이고
 * 팀과 분류만 지정하면 전투 시스템에 편입된다.
 *
 * 체력은 소유 액터의 UHealthComponent 를 찾아 사용한다.
 * 없으면 등록되지 않는다 (= 공격 대상이 되지 않는다).
 *
 * ITargetable 을 구현하지 않는 이유:
 * 레지스트리가 이 컴포넌트를 구체 타입으로 들고 있어 Execute_ 경유가 필요 없다.
 * 인터페이스를 함께 상속하면 BlueprintNativeEvent 와 일반 UFUNCTION 의
 * 이름이 충돌해 UHT 가 거부한다.
 */
UCLASS(ClassGroup=(StrikeForge), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UCombatTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatTargetComponent();

	// ── 조회 ────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	FORCEINLINE int32 GetTeamIndex() const { return TeamIndex; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	FORCEINLINE ETargetType GetTargetType() const { return TargetType; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	FORCEINLINE float GetArmor() const { return Armor; }

	/** 아직 때릴 수 있는 상태인가 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	FORCEINLINE UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	// ── 설정 ────────────────────────────────────────────
	/** 스폰 시 팀을 지정한다. BeginPlay 전에 호출해야 올바른 팀으로 등록된다 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void SetTeamIndex(int32 InTeamIndex);

	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	FORCEINLINE void SetTargetType(ETargetType InTargetType) { TargetType = InTargetType; }

	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void SetArmor(float InArmor) { Armor = FMath::Clamp(InArmor, 0.f, 0.9f); }

	// ── 피해 ────────────────────────────────────────────
	/**
	 * 최종 피해를 적용한다. 방어 감산은 공격자가 이미 계산했다.
	 *
	 * 소유 액터가 IDamagable 을 구현했다면 그쪽을 우선 호출하고,
	 * 아니면 UHealthComponent 에 직접 적용한다.
	 * 덕분에 액터 클래스를 수정하지 않아도 피해가 들어간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void ApplyCombatDamage(float InFinalDamage, AActor* InInstigator);

	/** 소유 액터가 파괴된 순간 */
	UPROPERTY(BlueprintAssignable, Category = "SF|Combat")
	FCombatTargetDestroyed OnTargetDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 소유 액터의 체력이 0 이 됐을 때 */
	UFUNCTION()
	virtual void HandleOwnerDeath();

	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> HealthComponent;

private:
	/** 소속 팀. -1 은 중립이며 누구와도 교전하지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	int32 TeamIndex = 0;

	/** 공격 우선순위 판단에 쓰인다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	ETargetType TargetType = ETargetType::ProductionBuilding;

	/** 받는 피해 감소 0~0.9 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0", ClampMax = "0.9"))
	float Armor = 0.f;

	/**
	 * 소유 액터의 최대 체력을 이 값으로 초기화한다.
	 * 0 이하면 건드리지 않고 액터가 설정한 값을 그대로 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float OverrideMaxHealth = 0.f;

	uint8 bRegistered : 1;
};
